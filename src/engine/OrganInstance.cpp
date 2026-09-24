// SPDX-License-Identifier: GPL-2.0-or-later
#include "OrganInstance.h"
#include "model/GOManual.h"
#include "model/GOStop.h"
#include "model/GOCoupler.h"
#include "model/GOTremulant.h"
#include "model/GOSwitch.h"
#include "sound/buffer/GOSoundBufferPlanarMutable.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace organvst {
namespace {
class Progress final : public GOProgressMonitor {
public:
  explicit Progress(std::function<bool(unsigned,const std::string&)> f):fn(std::move(f)){}
  void Setup(long n,const wxString&,const wxString& text) override { max=n; fn(0,text.ToStdString()); }
  void Reset(long n,const wxString&) override { max=n; }
  bool Update(unsigned n,const wxString& text) override { return fn(max>0?std::min(100u,unsigned(100ull*n/max)):0,text.ToStdString()); }
  long max=1;
  std::function<bool(unsigned,const std::string&)> fn;
};
}
OrganInstance::OrganInstance(const std::filesystem::path& definition,
  const std::filesystem::path& data,const std::filesystem::path& resources,
  unsigned rate,std::function<bool(unsigned,const std::string&)> progress)
  : assets_(definition,data/"pack-cache",progress),
    config_("OrganVST",""),organ_(config_),path_(assets_.definition().string()),rate_(rate) {
  // GOOrganController constructs its file store from the config resource path;
  // configure before loading by providing an explicit host resource override.
  config_.ConfigureForHost(wxString::FromUTF8(data.string()),wxString::FromUTF8(resources.string()));
  organ_.SetHostResourceDirectory(wxString::FromUTF8(resources.string()));
  Progress monitor(std::move(progress));
  try {
    const auto error=organ_.Load(GOOrgan(wxString::FromUTF8(path_)),"",false,monitor);
    if(!error.empty()) throw std::runtime_error(error.ToStdString());
    name_=organ_.GetOrganName().ToStdString();
    unsigned channel=0;
    // Keyboards first, pedal last: conventional multitimbral DAW routing.
    for(unsigned i=1;i<=organ_.GetODFManualCount() && channel<15;++i)
      manuals_[channel++]=organ_.GetManual(i);
    if(organ_.GetFirstManualIndex()==0) manuals_[15]=organ_.GetManual(0);
    std::unordered_set<GOButtonControl*> included;
    auto add=[&](GOButtonControl* button,const std::string& key,const std::string& group,const std::string& kind,int channel) {
      // Generated couplers and read-only logic are not independent console controls.
      // Displayed=N does NOT mean absent: auxiliary panels may reference it.
      if(button->IsReadOnly() || !included.insert(button).second)return;
      controls_.push_back({key,channel,button->GetName().ToStdString(),group,kind,button});
    };
    for(unsigned i=organ_.GetFirstManualIndex();i<organ_.GetManualAndPedalCount();++i) {
      auto* m=organ_.GetManual(i);
      const auto group=m->GetName().ToStdString();
      const int midi=i==0?15:(i<=15?int(i)-1:-1);
      const auto prefix="Manual"+std::to_string(i)+"/";
      for(unsigned j=0;j<m->GetStopCount();++j)
        add(m->GetStop(j),prefix+"Stop"+std::to_string(j),group,"Stop",midi);
      for(unsigned j=0;j<m->GetODFCouplerCount();++j)
        add(m->GetCoupler(j),prefix+"Coupler"+std::to_string(j),group,"Coupler",midi);
      for(unsigned j=0;j<m->GetSwitchCount();++j)
        add(m->GetSwitch(j),prefix+"Switch"+std::to_string(j),group,"Switch",midi);
    }
    for(unsigned i=0;i<organ_.GetTremulantCount();++i)
      add(organ_.GetTremulant(i),"Tremulant"+std::to_string(i),"Tremulants","Tremulant",-1);
    for(unsigned i=0;i<organ_.GetSwitchCount();++i)
      add(organ_.GetSwitch(i),"Switch"+std::to_string(i),"Switches","Switch",-1);
    if(controls_.size()>16384)throw std::runtime_error("Organ exceeds the 16384 console-control safety limit");
    auto& engine=organ_.GetSoundEngine();
    engine.SetFromConfig(config_);
    engine.SetNAuxThreads(0);
    engine.BuildEngine(GOSoundOrganEngine::createDefaultOutputConfigs(),maxFrames,rate);
    prepared_=true;
    organ_.GOSoundSamplerPlayerProxy::Connect(&engine.GetSamplerPlayer());
    organ_.PreparePlayback();
    organ_.StartPlayback();
    surface_=std::make_shared<ControlSurface>();
    for(const auto& c:controls_)surface_->controls.push_back({c.key,c.name,c.group,c.kind,c.channel});
    surface_->values=std::make_unique<ControlValue[]>(controls_.size());
    publishControls();
  } catch (...) {
    if(prepared_) { organ_.AbortPlayback(); organ_.GOSoundSamplerPlayerProxy::Disconnect(); organ_.GetSoundEngine().DestroyEngine(); }
    organ_.Clear();
    throw;
  }
}
OrganInstance::~OrganInstance() {
  if(prepared_) {
    organ_.AbortPlayback();
    organ_.GOSoundSamplerPlayerProxy::Disconnect();
    organ_.GetSoundEngine().DestroyEngine();
  }
  organ_.Clear();
}
void OrganInstance::note(unsigned channel,unsigned pitch,unsigned velocity) {
  if(channel<manuals_.size() && manuals_[channel] && pitch<128)
    manuals_[channel]->SetMidiNoteState(pitch,std::min(velocity,127u));
}
void OrganInstance::stop(unsigned index,bool value) {
  if(index<controls_.size()) controls_[index].button->SetButtonState(value);
}
void OrganInstance::panic() { organ_.AllNotesOff();auditionChannel_=-1;auditionFrames_=0; }
void OrganInstance::applyCommands() {
  for(unsigned i=0;i<controls_.size();++i) {
    const int requested=surface_->values[i].requested.exchange(-1);
    if(requested>=0)stop(i,requested!=0);
  }
  const int audition=surface_->audition.exchange(-1);
  if(audition>=0 && audition<16) {
    if(auditionChannel_>=0)note(auditionChannel_,auditionPitch_,0);
    auto* manual=manuals_[audition];
    if(!manual)return;
    const unsigned first=manual->GetFirstAccessibleKeyMIDINoteNumber();
    const unsigned count=manual->GetNumberOfAccessibleKeys();
    if(!count)return;
    auditionPitch_=std::clamp(60u,first,first+count-1);
    auditionChannel_=audition;auditionFrames_=rate_/2;note(audition,auditionPitch_,100);
  }
}
void OrganInstance::publishControls() {
  if(surface_)for(unsigned i=0;i<controls_.size();++i)
    surface_->values[i].actual.store(controls_[i].button->IsEngaged());
}
void OrganInstance::render(float* left,float* right,unsigned frames) {
  while(frames) {
    unsigned n=std::min(frames,maxFrames);
    if(auditionFrames_)n=std::min(n,auditionFrames_);
    GOSoundBufferPlanarMutable buffer(output_.data(),2,n);
    organ_.GetSoundEngine().RenderHost(&buffer,1,n);
    std::copy_n(output_.data(),n,left);
    std::copy_n(output_.data()+n,n,right);
    left+=n;right+=n;frames-=n;
    if(auditionFrames_ && (auditionFrames_-=n)==0) {
      note(auditionChannel_,auditionPitch_,0);auditionChannel_=-1;
    }
  }
}
}
