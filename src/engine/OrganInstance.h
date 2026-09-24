// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "GOOrganController.h"
#include "../assets/OrganAssets.h"
#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace organvst {
class HostOrgan final : public GOOrganController {
public:
  using GOOrganController::GOOrganController;
  using GOEventDistributor::PreparePlayback;
  using GOEventDistributor::StartPlayback;
  using GOEventDistributor::AbortPlayback;
};

struct ControlInfo {
  std::string name;
  std::string group;
  std::string kind;
  GOButtonControl* button{};
};

// Construct, load and destroy off the audio thread. Once prepared, the audio
// thread is the exclusive owner of all GrandOrgue mutable state.
class OrganInstance final {
public:
  static constexpr unsigned maxFrames=2048;
  OrganInstance(const std::filesystem::path& definition,
                const std::filesystem::path& data,
                const std::filesystem::path& resources, unsigned rate,
                std::function<bool(unsigned,const std::string&)> progress);
  ~OrganInstance();
  void note(unsigned channel,unsigned pitch,unsigned velocity);
  void stop(unsigned index,bool value);
  void panic();
  void render(float* left,float* right,unsigned frames);
  const std::vector<ControlInfo>& controls() const { return controls_; }
  const std::string& name() const { return name_; }
  const std::string& path() const { return path_; }
  unsigned sampleRate() const { return rate_; }
private:
  OrganAssets assets_;
  GOConfig config_;
  HostOrgan organ_;
  std::vector<ControlInfo> controls_;
  std::array<GOManual*,16> manuals_{};
  std::array<float,maxFrames*2> output_{};
  std::string name_,path_;
  unsigned rate_;
  bool prepared_=false;
};
}
