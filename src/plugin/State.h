// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "base/source/fstreamer.h"
#include <cmath>
#include <string>
#include <vector>
namespace organvst {
struct ProjectState {
  std::string path;
  double gain=.5;
  std::vector<std::pair<std::string,bool>> controls;
  bool legacy=false;
  int inputDivision=-1; // -1 uses incoming MIDI channels; otherwise target channel 0..15
};
inline bool readProjectState(Steinberg::IBStream* stream,ProjectState& result) {
  if(!stream)return false;
  Steinberg::IBStreamer s(stream,kLittleEndian);
  Steinberg::uint32 magic=0,version=0,count=0;
  auto text=[&](std::string& value,unsigned limit) {
    Steinberg::uint32 size=0;
    if(!s.readInt32u(size)||size>limit)return false;
    value.resize(size);return !size||s.readRaw(value.data(),size)==size;
  };
  if(!s.readInt32u(magic)||magic!=0x4F565354||!s.readInt32u(version)||(version!=1&&version!=2&&version!=3)||!text(result.path,1048576)||!s.readDouble(result.gain)||!std::isfinite(result.gain)||result.gain<0||result.gain>1)return false;
  if(version==1) {
    for(unsigned i=0;i<128;++i){double value;if(!s.readDouble(value)||!std::isfinite(value)||value<0||value>1)return false;}
    // The old catalog included generated couplers. Never apply those positional
    // values to the corrected catalog; restore the pack with authored defaults.
    result.legacy=true;return true;
  }
  if(!s.readInt32u(count)||count>16384)return false;
  result.controls.reserve(count);
  for(unsigned i=0;i<count;++i) {
    std::string key;Steinberg::uint32 value=0;
    if(!text(key,256)||!s.readInt32u(value)||value>1)return false;
    result.controls.emplace_back(std::move(key),value!=0);
  }
  if(version>=3) {
    Steinberg::uint32 route=0;
    if(!s.readInt32u(route)||route>16)return false;
    result.inputDivision=int(route)-1;
  }
  return true;
}
inline bool writeProjectState(Steinberg::IBStream* stream,const ProjectState& state) {
  if(!stream)return false;
  Steinberg::IBStreamer s(stream,kLittleEndian);
  auto text=[&](const std::string& value){return s.writeInt32u(Steinberg::uint32(value.size()))&&(value.empty()||s.writeRaw(value.data(),value.size())==value.size());};
  if(!s.writeInt32u(0x4F565354)||!s.writeInt32u(3)||!text(state.path)||!s.writeDouble(state.gain)||!s.writeInt32u(Steinberg::uint32(state.controls.size())))return false;
  for(const auto& [key,value]:state.controls)if(!text(key)||!s.writeInt32u(value?1:0))return false;
  return state.inputDivision>=-1 && state.inputDivision<16 && s.writeInt32u(state.inputDivision+1);
}
}
