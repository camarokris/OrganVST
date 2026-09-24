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
    for(unsigned i=organ_.GetFirstManualIndex();i<organ_.GetManualAndPedalCount();++i) {
      auto* m=organ_.GetManual(i);
      for(unsigned j=0;j<m->GetStopCount();++j)
        controls_.push_back({m->GetStop(j)->GetName().ToStdString(),m->GetName().ToStdString(),"Stop",m->GetStop(j)});
      for(unsigned j=0;j<m->GetCouplerCount();++j)
        controls_.push_back({m->GetCoupler(j)->GetName().ToStdString(),m->GetName().ToStdString(),"Coupler",m->GetCoupler(j)});
    }
    for(unsigned i=0;i<organ_.GetTremulantCount();++i) {
      auto* b=organ_.GetTremulant(i);
      controls_.push_back({b->GetName().ToStdString(),"Tremulants","Tremulant",b});
    }
    auto& engine=organ_.GetSoundEngine();
    engine.SetFromConfig(config_);
    engine.SetNAuxThreads(0);
    engine.BuildEngine(GOSoundOrganEngine::createDefaultOutputConfigs(),maxFrames,rate);
    prepared_=true;
    organ_.GOSoundSamplerPlayerProxy::Connect(&engine.GetSamplerPlayer());
    organ_.PreparePlayback();
    organ_.StartPlayback();
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
void OrganInstance::panic() { organ_.AllNotesOff(); }
void OrganInstance::render(float* left,float* right,unsigned frames) {
  while(frames) {
    unsigned n=std::min(frames,maxFrames);
    GOSoundBufferPlanarMutable buffer(output_.data(),2,n);
    organ_.GetSoundEngine().RenderHost(&buffer,1,n);
    std::copy_n(output_.data(),n,left);
    std::copy_n(output_.data()+n,n,right);
    left+=n;right+=n;frames-=n;
  }
}
}
