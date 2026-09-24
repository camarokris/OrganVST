// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace organvst {
inline constexpr unsigned crescendoSteps=32,maxEnclosures=64;
struct PedalDescriptor { std::string key,name; };
struct PedalValue { std::atomic<int> requested{-1},actual{127}; };
struct PerformanceState {
  unsigned layerMask=0; // zero = independent incoming channels
  double crescendo=0;
  std::vector<std::pair<std::string,unsigned>> enclosures;
  std::array<std::vector<std::pair<std::string,bool>>,crescendoSteps> steps;
  std::uint32_t programmed=0;
};
struct PerformanceSurface {
  std::vector<PedalDescriptor> enclosures;
  std::unique_ptr<PedalValue[]> pedals;
  std::unique_ptr<std::atomic<unsigned char>[]> steps; // step * control count + index
  std::atomic<std::uint32_t> programmed{0};
  std::atomic<unsigned> revision{0}; // odd while the audio thread captures a row
  std::atomic<int> capture{-1},clear{-1};
  std::atomic<int> crescendoRequested{-1},crescendoActual{0};
};
// Fixed storage and no allocation in MIDI dispatch. Source-channel overlap is
// merged only for layer mode; independent channels keep their own voices.
class NoteRouter {
public:
  void reset() { held_={}; }
  template<class Note> void dispatch(unsigned mask,unsigned channel,unsigned pitch,unsigned velocity,Note note) {
    if(channel>=16||pitch>=128)return;
    if(!mask){note(channel,pitch,velocity);return;}
    held_[channel][pitch]=velocity!=0;
    if(!velocity)for(unsigned source=0;source<16;++source)if(held_[source][pitch])return;
    for(unsigned target=0;target<16;++target)if(mask&(1u<<target))note(target,pitch,velocity);
  }
private:
  std::array<std::array<bool,128>,16> held_{};
};
}
