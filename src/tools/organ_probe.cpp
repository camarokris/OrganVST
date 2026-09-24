// SPDX-License-Identifier: GPL-2.0-or-later
#include "GOOrganController.h"
#include "model/GOManual.h"
#include "model/GOStop.h"
#include "sound/buffer/GOSoundBufferPlanarMutable.h"
#include "sound/GOSoundDefs.h"
#include <wx/init.h>
#include <wx/log.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <array>
#include <cmath>
#include <chrono>

static void word(std::ostream& out, unsigned value, int bytes) {
  for (int i=0; i<bytes; ++i) out.put(static_cast<char>(value >> (8*i)));
}

class HostOrgan : public GOOrganController {
public:
  using GOOrganController::GOOrganController;
  using GOEventDistributor::PreparePlayback;
  using GOEventDistributor::StartPlayback;
  using GOEventDistributor::AbortPlayback;
};

static bool render(HostOrgan& organ, GOConfig& config, char** argv) {
  const unsigned manualIndex = std::stoul(argv[3]);
  const unsigned stopIndex = std::stoul(argv[4]);
  const unsigned note = std::stoul(argv[5]);
  const unsigned rate = std::stoul(argv[6]);
  if (manualIndex >= organ.GetManualAndPedalCount() || note > 127 || rate < 1000 || rate > 192000)
    throw std::runtime_error("Invalid manual, note or sample rate");
  auto* manual = organ.GetManual(manualIndex);
  if (stopIndex >= manual->GetStopCount()) throw std::runtime_error("Invalid stop");
  auto& engine = organ.GetSoundEngine();
  engine.SetFromConfig(config);
  engine.SetNAuxThreads(0);
  engine.BuildEngine(GOSoundOrganEngine::createDefaultOutputConfigs(), MAX_FRAME_SIZE, rate);
  organ.GOSoundSamplerPlayerProxy::Connect(&engine.GetSamplerPlayer());
  organ.PreparePlayback();
  organ.StartPlayback();
  manual->GetStop(stopIndex)->SetButtonState(true);
  manual->SetMidiNoteState(note, 100);
  std::ofstream out(argv[2], std::ios::binary);
  if (!out) throw std::runtime_error("Cannot open render output");
  const unsigned total = 12 * rate;
  out.write("RIFF",4); word(out,36+total*4,4); out.write("WAVEfmt ",8);
  word(out,16,4); word(out,1,2); word(out,2,2); word(out,rate,4);
  word(out,rate*4,4); word(out,4,2); word(out,16,2);
  out.write("data",4); word(out,total*4,4);
  std::array<float, MAX_FRAME_SIZE*2> memory{};
  const std::array<unsigned,8> blocks{1,7,64,127,256,511,1024,2048};
  double energy=0, tailEnergy=0;
  double renderSeconds=0;
  unsigned iteration=0;
  for (unsigned pos=0; pos<total;) {
    if (pos == 8*rate) manual->SetMidiNoteState(note,0);
    unsigned frames = std::min(blocks[iteration++%blocks.size()],total-pos);
    if(pos<8*rate) frames=std::min(frames,8*rate-pos);
    GOSoundBufferPlanarMutable buffer(memory.data(),2,frames);
    const auto start=std::chrono::steady_clock::now();
    engine.RenderHost(&buffer,1,frames);
    renderSeconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    for(unsigned i=0;i<frames;++i) for(unsigned c=0;c<2;++c) {
      float sample=memory[c*frames+i];
      if (!std::isfinite(sample)) throw std::runtime_error("Non-finite audio");
      energy+=sample*sample;
      if(pos+i>11*rate) tailEnergy+=sample*sample;
      const auto pcm=static_cast<int16_t>(std::clamp(sample,-1.0f,1.0f)*32767);
      word(out,static_cast<uint16_t>(pcm),2);
    }
    pos+=frames;
  }
  std::cout << "Render stop: " << manual->GetStop(stopIndex)->GetName()
            << "\nEnergy: " << energy << "\nFinal-second energy: " << tailEnergy
            << "\nRender wall seconds: " << renderSeconds << " for 12 audio seconds\n";
  organ.AbortPlayback();
  organ.GOSoundSamplerPlayerProxy::Disconnect();
  engine.DestroyEngine();
  return energy>0 && out.good();
}

class Progress final : public GOProgressMonitor {
public:
  void Setup(long, const wxString& title, const wxString& message) override {
    std::cerr << title << ": " << message << '\n';
  }
  void Reset(long, const wxString&) override {}
  bool Update(unsigned, const wxString&) override { return true; }
};

int main(int argc, char** argv) {
  if (argc != 2 && argc != 7) { std::cerr << "Usage: organ_probe organ.organ [output.wav manual-index stop-index midi-note sample-rate]\n"; return 2; }
  wxInitializer wx;
  if (!wx.IsOk()) return 3;
  wxLog::SetActiveTarget(new wxLogStderr());
  GOConfig config("OrganVST-probe", "");
  config.ConfigureForHost(wxString::FromUTF8(std::filesystem::absolute("build/probe-data").string()), GO_RESOURCE_DIR);
  HostOrgan organ(config);
  Progress progress;
  const auto path = std::filesystem::absolute(argv[1]).string();
  const wxString error = organ.Load(GOOrgan(wxString::FromUTF8(path)), "", false, progress);
  if (!error.empty()) {
    std::cerr << "Load failed: " << error << '\n';
    organ.Clear();
    return 1;
  }
  std::cout << "Organ: " << organ.GetOrganName() << '\n'
            << "ODF ranks: " << organ.GetODFRankCount() << '\n';
  for (unsigned i = organ.GetFirstManualIndex(); i < organ.GetManualAndPedalCount(); ++i) {
    auto* manual = organ.GetManual(i);
    std::cout << "Manual " << i << ": " << manual->GetName() << " ("
              << manual->GetStopCount() << " stops)\n";
    for(unsigned j=0;j<manual->GetStopCount();++j)
      std::cout << "  " << j << ": " << manual->GetStop(j)->GetName() << '\n';
  }
  bool ok=true;
  try { if(argc==7) ok=render(organ,config,argv); }
  catch(const std::exception& e) { std::cerr << e.what() << '\n'; ok=false; }
  organ.Clear();
  return ok ? 0 : 1;
}
