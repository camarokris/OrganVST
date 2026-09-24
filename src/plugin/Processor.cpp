// SPDX-License-Identifier: GPL-2.0-or-later
#include "Plugin.h"
#include "Services.h"
#include "engine/OrganVSTSampleCache.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"
#include <wx/init.h>
#include <wx/stdpaths.h>
#include <wx/log.h>
#include <chrono>
#include <cstring>
#include <sstream>
#include <cmath>
#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace organvst {
namespace {
// Module-local wx runtime. The SDK build hides non-exported symbols.
wxInitializer& wxRuntime() { static wxInitializer runtime; return runtime; }

}
Processor::Processor() { setControllerClass(controllerId); for(auto& s:stops_) s.store(0); }
Processor::~Processor() { shutdown(); }
void Processor::shutdown() {
  quit_.store(true);
  if(loader_.joinable()) loader_.join();
  delete pending_.exchange(nullptr);
  delete retired_.exchange(nullptr);
  delete active_;active_=nullptr;
}
tresult PLUGIN_API Processor::terminate() { shutdown();return AudioEffect::terminate(); }
tresult PLUGIN_API Processor::initialize(FUnknown* context) {
  auto result=AudioEffect::initialize(context); if(result!=kResultOk)return result;
  if(!wxRuntime().IsOk())return kResultFalse;
  addEventInput(STR16("Manuals and pedalboard"),16);
  addAudioOutput(STR16("Organ mix"),SpeakerArr::kStereo);
  quit_.store(false);
  loader_=std::thread(&Processor::worker,this);
  return kResultOk;
}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement*,int32 inputs,SpeakerArrangement* outputs,int32 count) {
  return inputs==0 && count==1 && outputs && outputs[0]==SpeakerArr::kStereo ? AudioEffect::setBusArrangements(nullptr,0,outputs,count):kResultFalse;
}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 size) { return size==kSample32?kResultTrue:kResultFalse; }
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
  if(setup.sampleRate<1000||setup.sampleRate>192000)return kResultFalse;
  auto result=AudioEffect::setupProcessing(setup);
  if(result==kResultOk && rate_.exchange(unsigned(setup.sampleRate))!=unsigned(setup.sampleRate)) {
    std::lock_guard lock(mutex_);
    if(!requestedPath_.empty() || !savedPath_.empty()) {
      if(requestedPath_.empty())requestedPath_=savedPath_;
      if(requestedPath_==savedPath_ && surface_) {
        requestedRegistration_.clear();legacyRestore_=false;
        for(unsigned i=0;i<surface_->controls.size();++i) {
        const int pending=surface_->values[i].requested.load();
        requestedRegistration_.emplace_back(surface_->controls[i].key,pending>=0?pending!=0:surface_->values[i].actual.load());
        }
      }
      ++requestSerial_;
    }
  }
  return result;
}
tresult PLUGIN_API Processor::setProcessing(TBool state) { panic_.store(true); return AudioEffect::setProcessing(state); }
void Processor::requestLoad(std::string path) {
  std::lock_guard lock(mutex_); requestedPath_=std::move(path); requestedRegistration_.clear();legacyRestore_=false; ++requestSerial_; status_="Loading organ…";
}
void Processor::worker() {
 try {
  unsigned handled=0;int loggedInput=-1,loggedRoute=-2;
  const auto data=dataDirectory();
  Diagnostics diagnostics(data/"logs");
  auto* previousLog=wxLog::SetThreadActiveTarget(&diagnostics);
  struct RestoreLog { wxLog* previous; ~RestoreLog(){wxLog::SetThreadActiveTarget(previous);} } restoreLog{previousLog};
  while(!quit_.load()) {
    delete retired_.exchange(nullptr);
    const int input=lastInput_.load(),route=inputDivision_.load();
    if(input!=loggedInput) {
      loggedInput=input;
      if(input>=0)diagnostics.text("Received MIDI channel="+std::to_string(input/128+1)+" note="+std::to_string(input%128));
    }
    if(route!=loggedRoute) {
      loggedRoute=route;diagnostics.text(route<0?"Input routing: native MIDI channels":"Input routing: all channels to division channel="+std::to_string(route+1));
    }
    std::string path,exportPath; unsigned serial;
    std::vector<std::pair<std::string,bool>> registration;bool legacy=false;
    { std::lock_guard lock(mutex_); serial=requestSerial_; if(serial!=handled){path=requestedPath_;registration=requestedRegistration_;legacy=legacyRestore_;}exportPath=std::move(diagnosticDestination_);diagnosticDestination_.clear(); }
    if(!exportPath.empty()) {
      try {std::lock_guard lock(mutex_);diagnostics.exportBundle(exportPath,status_,metadata_);status_="Diagnostics exported";}
      catch(const std::exception& e){std::lock_guard lock(mutex_);status_=e.what();}
    }
    if(serial!=handled) {
      handled=serial;
      if(path.empty())continue;
      try {
        diagnostics.text("Loading "+path+" at "+std::to_string(rate_.load())+" Hz");
        auto instance=std::make_unique<OrganInstance>(path,data,resourceDirectory(),rate_.load(),
          [this,serial](unsigned percent,const std::string& detail) {
            std::lock_guard lock(mutex_);
            if(quit_.load() || serial!=requestSerial_)return false;
            status_="Loading organ: "+std::to_string(percent)+"% "+detail;return true;
          });
        auto surface=instance->surface();surface->generation=serial;
        for(const auto& [key,value]:registration)
          for(unsigned i=0;i<surface->controls.size();++i)
            if(surface->controls[i].key==key)instance->stop(i,value);
        instance->publishControls();
        diagnostics.text("Loaded "+instance->name()+"; authored controls="+std::to_string(surface->controls.size()));
        diagnostics.text(surface->metadata());
        auto memory=sampleCacheStats();
        diagnostics.text("Shared immutable sample payloads in this plugin process: "+std::to_string(memory.bytes)+" bytes, "+std::to_string(memory.blocks)+" blocks; cumulative reuse="+std::to_string(memory.reused));
        { std::lock_guard lock(mutex_);
          if(serial!=requestSerial_ || quit_.load())continue;
          if(inputDivision_.load()>=0 && std::none_of(surface->controls.begin(),surface->controls.end(),[&](const auto& c){return c.channel==inputDivision_.load();}))inputDivision_.store(-1);
          savedPath_=path;status_=instance->name()+(legacy?" — legacy registration reset; recheck automation":"");
          metadata_=surface->metadata();surface_=surface;
          delete pending_.exchange(instance.release());
        }
      } catch(const std::exception& e) { std::lock_guard lock(mutex_); if(serial==requestSerial_)status_="Load failed: "+std::string(e.what());diagnostics.text(e.what()); }
      catch(...) { std::lock_guard lock(mutex_); if(serial==requestSerial_)status_="Load failed with an unknown error"; }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
 } catch(const std::exception& e) {
   std::lock_guard lock(mutex_);status_="Loader unavailable: "+std::string(e.what());
 } catch(...) {
   std::lock_guard lock(mutex_);status_="Loader unavailable";
 }
}
void Processor::render(ProcessData& data,int start,int count) {
  if(count<=0 || !data.numOutputs || data.outputs[0].numChannels!=2)return;
  float* l=data.outputs[0].channelBuffers32[0]+start;
  float* r=data.outputs[0].channelBuffers32[1]+start;
  if(active_ && active_->sampleRate()==rate_.load()) {
    active_->render(l,r,unsigned(count));
    float gain=float(gain_.load()*2.0);
    for(int i=0;i<count;++i){l[i]*=gain;r[i]*=gain;}
  } else { std::fill_n(l,count,0.f);std::fill_n(r,count,0.f); }
}
tresult PLUGIN_API Processor::process(ProcessData& data) {
  if(data.symbolicSampleSize!=kSample32)return kResultFalse;
  if(!retired_.load()) if(auto* next=pending_.exchange(nullptr)) {
    auto* old=active_;active_=next;
    active_->markReady();
    retired_.store(old);
  }
  if(panic_.exchange(false) && active_)active_->panic();
  const int route=inputDivision_.load();
  if(route!=activeInputDivision_) {
    if(active_)active_->panic();
    activeInputDivision_=route;
  }
  if(active_)active_->applyCommands();
  // Merge event and automation offsets without allocating or sorting.
  int cursor=0;
  const int events=data.inputEvents?data.inputEvents->getEventCount():0;
  int eventIndex=0;
  const int parameters=data.inputParameterChanges?data.inputParameterChanges->getParameterCount():0;
  std::array<int,129> point{};
  const int queues=std::min(parameters,129);
  while(cursor<=data.numSamples) {
    Event ev{};
    while(eventIndex<events && data.inputEvents->getEvent(eventIndex,ev)==kResultOk && ev.sampleOffset<=cursor) {
      if(active_) {
        if(ev.type==Event::kNoteOnEvent && ev.noteOn.channel>=0 && ev.noteOn.channel<16 && ev.noteOn.pitch>=0 && ev.noteOn.pitch<128) {
          lastInput_.store(ev.noteOn.channel*128+ev.noteOn.pitch);
          active_->note(route>=0?route:ev.noteOn.channel,ev.noteOn.pitch,unsigned((std::isfinite(ev.noteOn.velocity)?std::clamp(ev.noteOn.velocity,0.f,1.f):0.f)*127));
        }
        if(ev.type==Event::kNoteOffEvent && ev.noteOff.channel>=0 && ev.noteOff.channel<16 && ev.noteOff.pitch>=0 && ev.noteOff.pitch<128)active_->note(route>=0?route:ev.noteOff.channel,ev.noteOff.pitch,0);
      }
      ++eventIndex;
    }
    int next=data.numSamples;
    if(eventIndex<events && data.inputEvents->getEvent(eventIndex,ev)==kResultOk)next=std::min(next,std::max(cursor,ev.sampleOffset));
    for(int q=0;q<queues;++q) {
      auto* queue=data.inputParameterChanges->getParameterData(q); if(!queue)continue;
      int offset=0;double value=0;
      while(point[q]<queue->getPointCount() && queue->getPoint(point[q],offset,value)==kResultOk && offset<=cursor) {
        const auto id=queue->getParameterId();
        value=std::isfinite(value)?std::clamp(value,0.0,1.0):0.0;
        if(id==gainId)gain_.store(value);
        else if(id>=stopBase && id<stopBase+128) { stops_[id-stopBase].store(value);if(active_)active_->stop(id-stopBase,value>=.5); }
        ++point[q];
      }
      if(point[q]<queue->getPointCount()&&queue->getPoint(point[q],offset,value)==kResultOk)next=std::min(next,std::max(cursor,offset));
    }
    if(cursor==data.numSamples)break;
    if(next<=cursor)next=cursor+1;
    render(data,cursor,next-cursor);cursor=next;
  }
  if(active_)active_->publishControls();
  if(data.numOutputs)data.outputs[0].silenceFlags=active_?0:3;
  return kResultOk;
}
tresult PLUGIN_API Processor::notify(IMessage* message) {
  if(!message)return kInvalidArgument;
  if(std::strcmp(message->getMessageID(),"route")==0) {
    int64 generation=0,channel=-1;
    auto* attrs=message->getAttributes();
    if(attrs->getInt("generation",generation)!=kResultOk || attrs->getInt("channel",channel)!=kResultOk || channel < -1 || channel>15)return kInvalidArgument;
    std::lock_guard lock(mutex_);
    if(!surface_ || generation!=surface_->generation || !surface_->ready.load())return kResultFalse;
    if(channel>=0 && std::none_of(surface_->controls.begin(),surface_->controls.end(),[&](const auto& c){return c.channel==channel;}))return kInvalidArgument;
    inputDivision_.store(int(channel));return kResultOk;
  }
  if(std::strcmp(message->getMessageID(),"control")==0 || std::strcmp(message->getMessageID(),"audition")==0) {
    int64 generation=0,index=0,value=0;
    auto* attrs=message->getAttributes();
    if(attrs->getInt("generation",generation)!=kResultOk||attrs->getInt("index",index)!=kResultOk)return kInvalidArgument;
    std::lock_guard lock(mutex_);
    if(!surface_ || generation!=surface_->generation || !surface_->ready.load())return kResultFalse;
    if(std::strcmp(message->getMessageID(),"audition")==0) {
      if(index>=0 && index<16*128)surface_->audition.store(int(index));
    } else if(index>=0 && size_t(index)<surface_->controls.size() && attrs->getInt("value",value)==kResultOk)
      surface_->values[index].requested.store(value?1:0);
    return kResultOk;
  }
  if(std::strcmp(message->getMessageID(),"cancel")==0) {
    std::lock_guard lock(mutex_);requestedPath_.clear();++requestSerial_;status_="Load cancelled";
    return kResultOk;
  }
  if(std::strcmp(message->getMessageID(),"diagnostics")==0) {
    const void* bytes=nullptr;uint32 size=0;
    if(message->getAttributes()->getBinary("path",bytes,size)==kResultOk && size>0 && size<1048576) {
      std::lock_guard lock(mutex_);diagnosticDestination_.assign(static_cast<const char*>(bytes),size);
    }
    return kResultOk;
  }
  if(std::strcmp(message->getMessageID(),"load")==0) {
    const void* bytes=nullptr;uint32 size=0;
    if(message->getAttributes()->getBinary("path",bytes,size)==kResultOk && size>0 && size<1048576)
      requestLoad(std::string(static_cast<const char*>(bytes),size));
    return kResultOk;
  }
  if(std::strcmp(message->getMessageID(),"poll")==0) {
    auto response=owned(allocateMessage());if(!response)return kResultFalse;
    std::lock_guard lock(mutex_);response->setMessageID("status");
    response->getAttributes()->setBinary("status",status_.data(),uint32(status_.size()));
    response->getAttributes()->setBinary("controls",metadata_.data(),uint32(metadata_.size()));
    if(surface_) {
      response->getAttributes()->setInt("generation",surface_->generation);
      response->getAttributes()->setInt("ready",surface_->ready.load()?1:0);
      response->getAttributes()->setInt("route",inputDivision_.load());
      response->getAttributes()->setInt("input",lastInput_.load());
      std::vector<unsigned char> states;
      for(unsigned i=0;i<surface_->controls.size();++i)states.push_back(surface_->values[i].actual.load()?1:0);
      response->getAttributes()->setBinary("states",states.data(),uint32(states.size()));
    }
    return sendMessage(response);
  }
  return AudioEffect::notify(message);
}
tresult PLUGIN_API Processor::getState(IBStream* stream) {
  ProjectState state;state.gain=gain_.load();state.inputDivision=inputDivision_.load();
  {std::lock_guard lock(mutex_);state.path=savedPath_;
    if(surface_)for(unsigned i=0;i<surface_->controls.size();++i) {
      const int pending=surface_->values[i].requested.load();
      state.controls.emplace_back(surface_->controls[i].key,pending>=0?pending!=0:surface_->values[i].actual.load());
    }
  }
  return writeProjectState(stream,state)?kResultOk:kResultFalse;
}
tresult PLUGIN_API Processor::setState(IBStream* stream) {
  ProjectState state;if(!readProjectState(stream,state))return kResultFalse;
  gain_.store(state.gain);inputDivision_.store(state.inputDivision);
  if(!state.path.empty()) {
    std::lock_guard lock(mutex_);requestedPath_=std::move(state.path);
    requestedRegistration_=std::move(state.controls);legacyRestore_=state.legacy;
    ++requestSerial_;status_="Restoring organ…";
  }
  return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream) {
  ProjectState state;if(!readProjectState(stream,state))return kResultFalse;
  setParamNormalized(gainId,state.gain);
  return kResultOk;
}
}
