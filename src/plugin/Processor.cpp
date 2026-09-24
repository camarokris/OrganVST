// SPDX-License-Identifier: GPL-2.0-or-later
#include "Plugin.h"
#include "Services.h"
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
bool readState(IBStream* state,std::string& path,double& gain,std::array<double,128>& stops) {
  IBStreamer s(state,kLittleEndian);
  uint32 magic=0,version=0,size=0;
  if(!s.readInt32u(magic)||magic!=0x4F565354||!s.readInt32u(version)||version!=1||!s.readInt32u(size)||size>1048576) return false;
  path.resize(size);
  if(size && s.readRaw(path.data(),size)!=size) return false;
  if(!s.readDouble(gain)||!std::isfinite(gain)||gain<0||gain>1) return false;
  for(auto& v:stops) if(!s.readDouble(v)||!std::isfinite(v)||v<0||v>1) return false;
  return true;
}
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
    if(!savedPath_.empty()) { requestedPath_=savedPath_; ++requestSerial_; }
  }
  return result;
}
tresult PLUGIN_API Processor::setProcessing(TBool state) { panic_.store(true); return AudioEffect::setProcessing(state); }
void Processor::requestLoad(std::string path) {
  std::lock_guard lock(mutex_); requestedPath_=std::move(path); ++requestSerial_; status_="Loading organ…";
}
void Processor::worker() {
 try {
  unsigned handled=0;
  const auto data=dataDirectory();
  Diagnostics diagnostics(data/"logs");
  auto* previousLog=wxLog::SetThreadActiveTarget(&diagnostics);
  struct RestoreLog { wxLog* previous; ~RestoreLog(){wxLog::SetThreadActiveTarget(previous);} } restoreLog{previousLog};
  while(!quit_.load()) {
    delete retired_.exchange(nullptr);
    std::string path,exportPath; unsigned serial;
    { std::lock_guard lock(mutex_); serial=requestSerial_; if(serial!=handled) path=requestedPath_;exportPath=std::move(diagnosticDestination_);diagnosticDestination_.clear(); }
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
        std::ostringstream metadata;
        for(const auto& c:instance->controls()) metadata << c.group << " / " << c.name << '\n';
        { std::lock_guard lock(mutex_);
          if(serial!=requestSerial_ || quit_.load())continue;
          savedPath_=path;status_=instance->name();metadata_=metadata.str();
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
    for(unsigned i=0;i<stops_.size();++i) active_->stop(i,stops_[i].load()>=.5);
    retired_.store(old);
  }
  if(panic_.exchange(false) && active_)active_->panic();
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
        if(ev.type==Event::kNoteOnEvent)active_->note(ev.noteOn.channel,ev.noteOn.pitch,unsigned(std::clamp(ev.noteOn.velocity,0.f,1.f)*127));
        if(ev.type==Event::kNoteOffEvent)active_->note(ev.noteOff.channel,ev.noteOff.pitch,0);
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
  if(data.numOutputs)data.outputs[0].silenceFlags=active_?0:3;
  return kResultOk;
}
tresult PLUGIN_API Processor::notify(IMessage* message) {
  if(!message)return kInvalidArgument;
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
    return sendMessage(response);
  }
  return AudioEffect::notify(message);
}
tresult PLUGIN_API Processor::getState(IBStream* state) {
  IBStreamer s(state,kLittleEndian);std::string path;
  {std::lock_guard lock(mutex_);path=savedPath_;}
  if(!s.writeInt32u(0x4F565354)||!s.writeInt32u(1)||!s.writeInt32u(uint32(path.size()))||s.writeRaw(path.data(),path.size())!=path.size()||!s.writeDouble(gain_.load()))return kResultFalse;
  for(auto& v:stops_)if(!s.writeDouble(v.load()))return kResultFalse;
  return kResultOk;
}
tresult PLUGIN_API Processor::setState(IBStream* state) {
  std::string path;double gain;std::array<double,128> stops;
  if(!readState(state,path,gain,stops))return kResultFalse;
  gain_.store(gain);for(unsigned i=0;i<128;++i)stops_[i].store(stops[i]);
  if(!path.empty())requestLoad(path);
  return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
  std::string path;double gain;std::array<double,128> stops;
  if(!readState(state,path,gain,stops))return kResultFalse;
  setParamNormalized(gainId,gain);
  for(unsigned i=0;i<128;++i)setParamNormalized(stopBase+i,stops[i]);
  return kResultOk;
}
}
