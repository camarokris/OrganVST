// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "base/source/fstreamer.h"
#include "../engine/Performance.h"
#include <cmath>
namespace organvst {
struct ProjectState : PerformanceState {
  std::string path;
  double gain=.5;
  std::vector<std::pair<std::string,bool>> controls;
  bool legacy=false;
};
inline bool readProjectState(Steinberg::IBStream* stream,ProjectState& result) {
  if(!stream)return false;
  result=ProjectState{};
  Steinberg::IBStreamer s(stream,kLittleEndian);
  Steinberg::uint32 magic=0,version=0,count=0;
  auto text=[&](std::string& value,unsigned limit) {
    Steinberg::uint32 size=0;
    if(!s.readInt32u(size)||size>limit)return false;
    value.resize(size);return !size||s.readRaw(value.data(),size)==size;
  };
  auto registration=[&](auto& values) {
    Steinberg::uint32 length=0;if(!s.readInt32u(length)||length>16384)return false;
    values.reserve(length);
    for(unsigned i=0;i<length;++i) {
      std::string key;Steinberg::uint32 value=0;
      if(!text(key,256)||!s.readInt32u(value)||value>1)return false;
      values.emplace_back(std::move(key),value!=0);
    }
    return true;
  };
  if(!s.readInt32u(magic)||magic!=0x4F565354||!s.readInt32u(version)||version<1||version>4||!text(result.path,1048576)||!s.readDouble(result.gain)||!std::isfinite(result.gain)||result.gain<0||result.gain>1)return false;
  if(version==1) {
    for(unsigned i=0;i<128;++i){double value;if(!s.readDouble(value)||!std::isfinite(value)||value<0||value>1)return false;}
    result.legacy=true;return true;
  }
  if(!registration(result.controls))return false;
  if(version==3) {
    Steinberg::uint32 route=0;if(!s.readInt32u(route)||route>16)return false;
    result.layerMask=route?1u<<(route-1):0;
  }
  if(version==4) {
    if(!s.readInt32u(result.layerMask)||result.layerMask>65535||!s.readDouble(result.crescendo)||!std::isfinite(result.crescendo)||result.crescendo<0||result.crescendo>1)return false;
    if(!s.readInt32u(count)||count>maxEnclosures)return false;
    for(unsigned i=0;i<count;++i) {
      std::string key;Steinberg::uint32 value=0;
      if(!text(key,256)||!s.readInt32u(value)||value>127)return false;
      result.enclosures.emplace_back(std::move(key),value);
    }
    if(!s.readInt32u(result.programmed))return false;
    for(unsigned i=0;i<crescendoSteps;++i)if(result.programmed&(1u<<i))if(!registration(result.steps[i]))return false;
  }
  return true;
}
inline bool writeProjectState(Steinberg::IBStream* stream,const ProjectState& state) {
  if(!stream||state.layerMask>65535)return false;
  Steinberg::IBStreamer s(stream,kLittleEndian);
  auto text=[&](const std::string& value){return s.writeInt32u(Steinberg::uint32(value.size()))&&(value.empty()||s.writeRaw(value.data(),value.size())==value.size());};
  auto registration=[&](const auto& values) {
    if(!s.writeInt32u(Steinberg::uint32(values.size())))return false;
    for(const auto& [key,value]:values)if(!text(key)||!s.writeInt32u(value?1:0))return false;
    return true;
  };
  if(!s.writeInt32u(0x4F565354)||!s.writeInt32u(4)||!text(state.path)||!s.writeDouble(state.gain)||!registration(state.controls)||!s.writeInt32u(state.layerMask)||!s.writeDouble(state.crescendo)||!s.writeInt32u(Steinberg::uint32(state.enclosures.size())))return false;
  for(const auto& [key,value]:state.enclosures)if(!text(key)||!s.writeInt32u(value))return false;
  if(!s.writeInt32u(state.programmed))return false;
  for(unsigned i=0;i<crescendoSteps;++i)if(state.programmed&(1u<<i))if(!registration(state.steps[i]))return false;
  return true;
}
}
