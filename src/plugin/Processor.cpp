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
        surface_->snapshotPerformance(requestedPerformance_);
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
  std::lock_guard lock(mutex_); requestedPath_=std::move(path); requestedRegistration_.clear();requestedPerformance_=PerformanceState{};legacyRestore_=false; ++requestSerial_; status_="Loading organ…";
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
    const int input=lastInput_.load(),route=layerMask_.load();
    if(input!=loggedInput) {
      loggedInput=input;
      if(input>=0)diagnostics.text("Received MIDI channel="+std::to_string(input/128+1)+" note="+std::to_string(input%128));
    }
    if(route!=loggedRoute) {
      loggedRoute=route;diagnostics.text(route==0?"Input routing: native MIDI channels":"Input routing: layered destination mask="+std::to_string(route));
    }
    std::string path,exportPath; unsigned serial;
    std::vector<std::pair<std::string,bool>> registration;bool legacy=false;PerformanceState performance;
    { std::lock_guard lock(mutex_); serial=requestSerial_; if(serial!=handled){path=requestedPath_;registration=requestedRegistration_;legacy=legacyRestore_;performance=requestedPerformance_;}exportPath=std::move(diagnosticDestination_);diagnosticDestination_.clear(); }
    if(!exportPath.empty()) {
      try {
        std::lock_guard lock(mutex_);
        std::string details=status_+"\nLayer mask: "+std::to_string(layerMask_.load());
        if(surface_) {
          const auto& perf=surface_->performance;
          details+="\nCrescendo position: "+std::to_string(perf.crescendoActual.load())+"\nProgrammed step mask: "+std::to_string(perf.programmed.load());
          for(unsigned i=0;i<perf.enclosures.size();++i)details+="\nExpression "+perf.enclosures[i].name+": "+std::to_string(perf.pedals[i].actual.load());
        }
        diagnostics.exportBundle(exportPath,details,metadata_);status_="Diagnostics exported";
      }
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
        instance->restorePerformance(performance);
        instance->publishControls();
        diagnostics.text("Loaded "+instance->name()+"; authored controls="+std::to_string(surface->controls.size()));
        diagnostics.text(surface->metadata());
        auto memory=sampleCacheStats();
        diagnostics.text("Shared immutable sample payloads in this plugin process: "+std::to_string(memory.bytes)+" bytes, "+std::to_string(memory.blocks)+" blocks; cumulative reuse="+std::to_string(memory.reused));
        { std::lock_guard lock(mutex_);
          if(serial!=requestSerial_ || quit_.load())continue;
          unsigned available=0;for(const auto& c:surface->controls)if(c.channel>=0)available|=1u<<c.channel;
          layerMask_.store(layerMask_.load()&available);
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
    active_->markReady();router_.reset();
    retired_.store(old);
  }
  if(panic_.exchange(false)) {if(active_)active_->panic();router_.reset();}
  const int route=layerMask_.load();
  if(route!=activeLayerMask_) {
    if(active_)active_->panic();
    router_.reset();activeLayerMask_=route;
  }
  if(active_)active_->applyCommands();
  // Merge event and automation offsets without allocating or sorting.
  int cursor=0;
  const int events=data.inputEvents?data.inputEvents->getEventCount():0;
  int eventIndex=0;
  const int parameters=data.inputParameterChanges?data.inputParameterChanges->getParameterCount():0;
  std::array<int,194> point{};
  const int queues=std::min(parameters,194);
  while(cursor<=data.numSamples) {
    Event ev{};
    while(eventIndex<events && data.inputEvents->getEvent(eventIndex,ev)==kResultOk && ev.sampleOffset<=cursor) {
      if(active_) {
        if(ev.type==Event::kNoteOnEvent && ev.noteOn.channel>=0 && ev.noteOn.channel<16 && ev.noteOn.pitch>=0 && ev.noteOn.pitch<128) {
          lastInput_.store(ev.noteOn.channel*128+ev.noteOn.pitch);
          router_.dispatch(route,ev.noteOn.channel,ev.noteOn.pitch,unsigned((std::isfinite(ev.noteOn.velocity)?std::clamp(ev.noteOn.velocity,0.f,1.f):0.f)*127),[&](unsigned ch,unsigned pitch,unsigned vel){active_->note(ch,pitch,vel);});
        }
        if(ev.type==Event::kNoteOffEvent && ev.noteOff.channel>=0 && ev.noteOff.channel<16 && ev.noteOff.pitch>=0 && ev.noteOff.pitch<128)router_.dispatch(route,ev.noteOff.channel,ev.noteOff.pitch,0,[&](unsigned ch,unsigned pitch,unsigned vel){active_->note(ch,pitch,vel);});
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
        else if(id==crescendoId && active_)active_->crescendo(unsigned(value*127+.5));
        else if(id>=expressionBase && id<expressionBase+maxEnclosures && active_)active_->expression(id-expressionBase,unsigned(value*127+.5));
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
  if(std::strcmp(message->getMessageID(),"route")==0 || std::strcmp(message->getMessageID(),"layers")==0) {
    int64 generation=0,channel=-1;
    auto* attrs=message->getAttributes();
    if(attrs->getInt("generation",generation)!=kResultOk)return kInvalidArgument;
    unsigned mask=0;
    if(std::strcmp(message->getMessageID(),"route")==0) {
      if(attrs->getInt("channel",channel)!=kResultOk || channel < -1 || channel>15)return kInvalidArgument;
      mask=channel<0?0:1u<<channel;
    } else {
      if(attrs->getInt("mask",channel)!=kResultOk || channel<0 || channel>65535)return kInvalidArgument;
      mask=unsigned(channel);
    }
    std::lock_guard lock(mutex_);
    if(!surface_ || generation!=surface_->generation || !surface_->ready.load())return kResultFalse;
    unsigned available=0;for(const auto& c:surface_->controls)if(c.channel>=0)available|=1u<<c.channel;
    if(mask&~available)return kInvalidArgument;
    layerMask_.store(mask);return kResultOk;
  }
  if(std::strcmp(message->getMessageID(),"pedal")==0 || std::strcmp(message->getMessageID(),"capture")==0 || std::strcmp(message->getMessageID(),"clear-step")==0) {
    int64 generation=0,index=-1,value=0;auto* attrs=message->getAttributes();
    if(attrs->getInt("generation",generation)!=kResultOk || attrs->getInt("index",index)!=kResultOk)return kInvalidArgument;
    std::lock_guard lock(mutex_);
    if(!surface_ || generation!=surface_->generation || !surface_->ready.load())return kResultFalse;
    auto& perf=surface_->performance;
    if(std::strcmp(message->getMessageID(),"pedal")==0) {
      if(attrs->getInt("value",value)!=kResultOk || value<0 || value>127)return kInvalidArgument;
      if(index==-1)perf.crescendoRequested.store(int(value));
      else if(index>=0 && size_t(index)<perf.enclosures.size())perf.pedals[index].requested.store(int(value));
      else return kInvalidArgument;
    } else {
      if(index<0 || index>=crescendoSteps)return kInvalidArgument;
      if(std::strcmp(message->getMessageID(),"capture")==0)perf.capture.store(int(index));
      else perf.clear.store(int(index));
    }
    return kResultOk;
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
      response->getAttributes()->setInt("layers",layerMask_.load());
      response->getAttributes()->setInt("input",lastInput_.load());
      auto& perf=surface_->performance;
      response->getAttributes()->setInt("crescendo",perf.crescendoActual.load());
      response->getAttributes()->setInt("programmed",perf.programmed.load());
      std::string pedals;std::vector<unsigned char> expression;
      for(unsigned i=0;i<perf.enclosures.size();++i) {
        auto name=perf.enclosures[i].name;for(char& c:name)if(c=='\n'||c=='\t'||c=='\r')c=' ';
        pedals+=perf.enclosures[i].key+'\t'+name+'\n';expression.push_back(perf.pedals[i].actual.load());
      }
      response->getAttributes()->setBinary("pedals",pedals.data(),uint32(pedals.size()));
      response->getAttributes()->setBinary("expression",expression.data(),uint32(expression.size()));
      std::vector<unsigned char> states;
      for(unsigned i=0;i<surface_->controls.size();++i)states.push_back(surface_->values[i].actual.load()?1:0);
      response->getAttributes()->setBinary("states",states.data(),uint32(states.size()));
    }
    return sendMessage(response);
  }
  return AudioEffect::notify(message);
}
tresult PLUGIN_API Processor::getState(IBStream* stream) {
  ProjectState state;state.gain=gain_.load();state.layerMask=layerMask_.load();
  {std::lock_guard lock(mutex_);state.path=savedPath_;
    if(surface_ && !surface_->snapshotPerformance(state))return kResultFalse;
    if(surface_)for(unsigned i=0;i<surface_->controls.size();++i) {
      const int pending=surface_->values[i].requested.load();
      state.controls.emplace_back(surface_->controls[i].key,pending>=0?pending!=0:surface_->values[i].actual.load());
    }
  }
  return writeProjectState(stream,state)?kResultOk:kResultFalse;
}
tresult PLUGIN_API Processor::setState(IBStream* stream) {
  ProjectState state;if(!readProjectState(stream,state))return kResultFalse;
  gain_.store(state.gain);layerMask_.store(state.layerMask);
  if(!state.path.empty()) {
    std::lock_guard lock(mutex_);requestedPath_=std::move(state.path);
    requestedRegistration_=std::move(state.controls);requestedPerformance_=state;legacyRestore_=state.legacy;
    ++requestSerial_;status_="Restoring organ…";
  }
  return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream) {
  ProjectState state;if(!readProjectState(stream,state))return kResultFalse;
  setParamNormalized(gainId,state.gain);
  setParamNormalized(crescendoId,state.crescendo);
  return kResultOk;
}
}
