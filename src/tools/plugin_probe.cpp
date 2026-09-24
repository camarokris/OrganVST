// SPDX-License-Identifier: GPL-2.0-or-later
#include "../plugin/State.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <array>
#include <chrono>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace organvst;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static ProjectState saved(IComponent* component) {
  MemoryStream stream;check(component->getState(&stream)==kResultOk,"getState failed");stream.seek(0,IBStream::kIBSeekSet,nullptr);
  ProjectState state;check(readProjectState(&stream,state),"Invalid saved state");return state;
}
int main(int argc,char** argv) {
  if(argc!=3)return 2;
  try {
    HostApplication host;PluginContextFactory::instance().setPluginContext(&host);
    std::string error;auto module=VST3::Hosting::Module::create(argv[1],error);check(bool(module),error.c_str());
    auto& factory=module->getFactory();
    for(const auto& info:factory.classInfos())if(info.category()==kVstAudioEffectClass) {
      auto provider=owned(new PlugProvider(factory,info));check(provider->initialize(),"Plugin initialization failed");
      auto component=provider->getComponentPtr();FUnknownPtr<IAudioProcessor> processor(component);FUnknownPtr<IConnectionPoint> connection(component);
      check(processor&&connection,"Missing processor/message interface");
      ProcessSetup setup{kRealtime,kSample32,127,48000};check(processor->setupProcessing(setup)==kResultOk,"setup failed");
      component->activateBus(kAudio,kOutput,0,true);component->activateBus(kEvent,kInput,0,true);
      check(component->setActive(true)==kResultOk,"setActive failed");processor->setProcessing(true);
      std::array<float,127> l{},r{};float* channels[]{l.data(),r.data()};AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=channels;
      EventList events;ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=127;data.numOutputs=1;data.outputs=&output;data.inputEvents=&events;
      auto process=[&](){check(processor->process(data)==kResultOk,"process failed");events.clear();double energy=0;for(unsigned i=0;i<127;++i){check(std::isfinite(l[i])&&std::isfinite(r[i]),"Non-finite audio");energy+=l[i]*l[i]+r[i]*r[i];}return energy;};
      auto restore=[&](const ProjectState& state){MemoryStream stream;check(writeProjectState(&stream,state),"state write failed");stream.seek(0,IBStream::kIBSeekSet,nullptr);check(component->setState(&stream)==kResultOk,"state restore failed");};
      auto waitLoaded=[&](){for(unsigned i=0;i<5000;++i){process();if(saved(component).controls.size()==134)return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Organ load timeout");};
      ProjectState state;state.path=argv[2];restore(state);waitLoaded();process();
      auto initial=saved(component);check(initial.controls[0].second,"Plugin cleared authored default");
      auto command=[&](unsigned generation,unsigned index,bool value){HostMessage msg;msg.setMessageID("control");msg.getAttributes()->setInt("generation",generation);msg.getAttributes()->setInt("index",index);msg.getAttributes()->setInt("value",value?1:0);return connection->notify(&msg);};
      check(command(0,131,true)==kResultFalse,"Stale editor command accepted");
      check(command(1,131,true)==kResultOk,"Late control command failed");process();
      state=saved(component);check(state.controls[131].second,"Control >128 missing from project state");
      Event note{};note.type=Event::kNoteOnEvent;note.busIndex=0;note.noteOn.channel=1;note.noteOn.pitch=60;note.noteOn.velocity=.8f;note.noteOn.noteId=-1;events.addEvent(note);
      double energy=0;for(unsigned i=0;i<200;++i)energy+=process();check(energy>0,"Late control produced no plugin audio");
      // Replace the live engine using its full keyed registration, without an editor.
      restore(state);for(unsigned i=0;i<1000;++i){process();std::this_thread::sleep_for(std::chrono::milliseconds(2));if(command(2,131,true)==kResultOk)break;}
      check(command(1,131,false)==kResultFalse,"Previous organ generation accepted");process();
      check(saved(component).controls[131].second,"Editorless registration recall failed");
      processor->setProcessing(false);component->setActive(false);
      std::cout<<"PASS actual VST3 loading, defaults, stale commands, >128 playback and keyed editorless recall\n";
      return 0;
    }
    throw std::runtime_error("No instrument component");
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
