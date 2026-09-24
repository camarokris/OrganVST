// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "engine/OrganInstance.h"
#include "State.h"
#include <atomic>
#include <array>
#include <mutex>
#include <thread>

namespace organvst {
inline const Steinberg::FUID processorId(0xB79DAA93,0xCA7C4358,0xA8CEE630,0xC317E541);
inline const Steinberg::FUID controllerId(0x7B2BE706,0xBF624D12,0x95C844E6,0xD2BD2719);
inline constexpr Steinberg::Vst::ParamID gainId=0,stopBase=100;

class Processor final : public Steinberg::Vst::AudioEffect {
public:
  Processor();
  ~Processor() override;
  static Steinberg::FUnknown* create(void*) { return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor); }
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
  Steinberg::tresult PLUGIN_API terminate() override;
  Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup&) override;
  Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement*,Steinberg::int32,Steinberg::Vst::SpeakerArrangement*,Steinberg::int32) override;
  Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32) override;
  Steinberg::tresult PLUGIN_API setProcessing(Steinberg::TBool) override;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) override;
  Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage*) override;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
  Steinberg::uint32 PLUGIN_API getTailSamples() override { return Steinberg::Vst::kInfiniteTail; }
private:
  void requestLoad(std::string);
  void shutdown();
  void worker();
  void render(Steinberg::Vst::ProcessData&,int,int);
  std::mutex mutex_;
  std::string requestedPath_,savedPath_,status_="No organ loaded";
  std::string metadata_;
  std::string diagnosticDestination_;
  std::shared_ptr<ControlSurface> surface_;
  std::vector<std::pair<std::string,bool>> requestedRegistration_;
  bool legacyRestore_=false;
  unsigned requestSerial_=0;
  std::atomic<unsigned> rate_{48000};
  std::atomic<bool> quit_{false},panic_{false};
  std::atomic<OrganInstance*> pending_{nullptr},retired_{nullptr};
  OrganInstance* active_=nullptr;
  std::array<std::atomic<double>,128> stops_{};
  std::atomic<double> gain_{0.5};
  std::thread loader_;
};

class Controller final : public Steinberg::Vst::EditController {
public:
  static Steinberg::FUnknown* create(void*) { return static_cast<Steinberg::Vst::IEditController*>(new Controller); }
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
  Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream*) override;
  Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString) override;
  Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage*) override;
  void load(const std::string&);
  void poll();
  void cancelLoad();
  void exportDiagnostics(const std::string&);
  void control(unsigned,bool);
  void audition(int);
  std::string status="No organ loaded",metadata;
  unsigned generation=0;
  bool ready=false;
  std::vector<bool> actual;
  std::vector<ControlDescriptor> catalog;
};
}
