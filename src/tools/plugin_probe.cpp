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
      // FL Studio-style channel-1 notes can explicitly address another division.
      auto route=[&](unsigned generation,int channel){HostMessage msg;msg.setMessageID("route");msg.getAttributes()->setInt("generation",generation);msg.getAttributes()->setInt("channel",channel);return connection->notify(&msg);};
      check(route(0,1)==kResultFalse,"Stale routing command accepted");
      check(route(1,16)==kInvalidArgument,"Invalid input route accepted");
      check(route(1,14)==kInvalidArgument,"Absent division accepted");
      check(command(1,0,false)==kResultOk,"Could not disable default stop");
      check(route(1,1)==kResultOk,"Single-division route failed");process();
      for(unsigned i=0;i<4000;++i)process();
      note.noteOn.channel=0;events.addEvent(note);energy=0;
      for(unsigned i=0;i<200;++i)energy+=process();check(energy>0,"Channel-1 input did not reach second division");
      state=saved(component);check(state.layerMask==2,"Input route missing from state");
      check(route(1,-1)==kResultOk,"Native channel route failed");process();
      for(unsigned i=0;i<4000;++i)process();
      check(process()==0,"Route change left a stuck note");
      events.addEvent(note);energy=0;for(unsigned i=0;i<200;++i)energy+=process();
      check(energy==0,"Native routing sent channel 1 to second division");
      // Explicit low-key audition is independent of incoming route and channel.
      HostMessage audition;audition.setMessageID("audition");audition.getAttributes()->setInt("generation",1);audition.getAttributes()->setInt("index",2*128+36);
      check(connection->notify(&audition)==kResultOk,"Low-key audition command failed");
      energy=0;for(unsigned i=0;i<200;++i)energy+=process();check(energy>0,"Short keyboard audition silent");
      for(unsigned i=0;i<4000;++i)process();check(process()==0,"Audition left a stuck note");
      auto action=[&](const char* name,int index,int value=0) {
        HostMessage msg;msg.setMessageID(name);msg.getAttributes()->setInt("generation",1);msg.getAttributes()->setInt("index",index);msg.getAttributes()->setInt("value",value);return connection->notify(&msg);
      };
      HostMessage layers;layers.setMessageID("layers");layers.getAttributes()->setInt("generation",1);layers.getAttributes()->setInt("mask",3);
      check(connection->notify(&layers)==kResultOk,"Layer mask rejected");process();
      command(1,131,false);process();check(action("capture",0)==kResultOk,"Capture low failed");process();
      command(1,131,true);process();check(action("capture",16)==kResultOk,"Capture high failed");process();
      action("pedal",-1,127);process();
      check(action("pedal",-1,0)==kResultOk,"Crescendo command failed");process();
      check(!saved(component).controls[131].second,"Crescendo low did not clear stop");
      action("pedal",-1,127);process();check(saved(component).controls[131].second,"Crescendo high did not restore stop");
      action("pedal",0,0);process();events.addEvent(note);
      for(unsigned i=0;i<4000;++i)process();check(process()==0,"Expression zero did not mute");
      action("pedal",0,127);energy=0;for(unsigned i=0;i<200;++i)energy+=process();check(energy>0,"Layered channel-1 input silent after reopening expression");
      action("pedal",0,67);process();state=saved(component);
      check(state.layerMask==3&&state.programmed==((1u<<16)|1)&&state.enclosures[0].second==67,"Layer/pedal/crescendo state incomplete");
      // Replace the live engine using its full keyed registration, without an editor.
      restore(state);for(unsigned i=0;i<1000;++i){process();std::this_thread::sleep_for(std::chrono::milliseconds(2));if(command(2,131,true)==kResultOk)break;}
      check(command(1,131,false)==kResultFalse,"Previous organ generation accepted");process();
      check(saved(component).controls[131].second,"Editorless registration recall failed");
      check(saved(component).layerMask==3,"Editorless input routing recall failed");
      events.addEvent(note);energy=0;for(unsigned i=0;i<200;++i)energy+=process();check(energy>0,"Restored single-division route is silent");
      check(saved(component).programmed==((1u<<16)|1)&&saved(component).enclosures[0].second==67,"Performance recall failed");
      // V2 registration layout is the same; V3 stores a single destination plus one.
      MemoryStream old;check(writeProjectState(&old,state),"Legacy fixture failed");
      old.seek(4,IBStream::kIBSeekSet,nullptr);IBStreamer writer(&old,kLittleEndian);writer.writeInt32u(2);
      old.seek(0,IBStream::kIBSeekSet,nullptr);ProjectState previous;check(readProjectState(&old,previous)&&previous.layerMask==0&&previous.controls==state.controls,"V2 registration compatibility failed");
      MemoryStream v3;ProjectState single=state;single.layerMask=3; // raw 3 encodes channel index 2 in V3
      check(writeProjectState(&v3,single),"V3 fixture failed");v3.seek(4,IBStream::kIBSeekSet,nullptr);IBStreamer v3writer(&v3,kLittleEndian);v3writer.writeInt32u(3);
      v3.seek(0,IBStream::kIBSeekSet,nullptr);ProjectState migrated;check(readProjectState(&v3,migrated)&&migrated.layerMask==4,"V3 route migration failed");
      processor->setProcessing(false);component->setActive(false);
      std::cout<<"PASS actual VST3 loading, defaults, stale commands, >128 playback, input routing, short-key audition, route-change tails and V2/V3/V4 editorless recall, layers, expression and crescendo\n";
      return 0;
    }
    throw std::runtime_error("No instrument component");
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
