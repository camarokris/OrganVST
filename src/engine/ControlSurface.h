// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Performance.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>
namespace organvst {
struct ControlDescriptor {
  std::string key, name, group, kind;
  int channel=-1; // zero-based; -1 for global controls
  unsigned firstNote=0,lastNote=127;
};
struct ControlValue {
  std::atomic<int> requested{-1};
  std::atomic<bool> actual{false};
};
// Constructed on the loader, shared with the message thread. Audio owns the model
// and only exchanges atomic values here; it never takes/drops shared ownership.
struct ControlSurface {
  unsigned generation=0;
  PerformanceSurface performance;
  std::vector<ControlDescriptor> controls;
  std::unique_ptr<ControlValue[]> values;
  std::atomic<int> audition{-1}; // packed channel * 128 + pitch; bounded half-second note
  std::atomic<bool> ready{false};
  std::string metadata() const;
  bool snapshotPerformance(PerformanceState& state) const;
};
inline std::string ControlSurface::metadata() const {
  std::string result;
  auto clean=[](std::string s){for(char& c:s)if(c=='\t'||c=='\n'||c=='\r')c=' ';return s;};
  for(const auto& c:controls)result+=clean(c.key)+'\t'+clean(c.group)+'\t'+clean(c.kind)+'\t'+std::to_string(c.channel)+'\t'+std::to_string(c.firstNote)+'\t'+std::to_string(c.lastNote)+'\t'+clean(c.name)+'\n';
  return result;
}
}

inline bool organvst::ControlSurface::snapshotPerformance(PerformanceState& state) const {
  const auto& perf=performance;
  for(unsigned attempt=0;attempt<3;++attempt) {
    const unsigned before=perf.revision.load();if(before&1)continue;
    state.enclosures.clear();for(auto& row:state.steps)row.clear();
    const int requested=perf.crescendoRequested.load();
    state.crescendo=double(requested>=0?requested:perf.crescendoActual.load())/127;
    for(unsigned i=0;i<perf.enclosures.size();++i) {
      const int pending=perf.pedals[i].requested.load();
      state.enclosures.emplace_back(perf.enclosures[i].key,unsigned(pending>=0?pending:perf.pedals[i].actual.load()));
    }
    state.programmed=perf.programmed.load();
    for(unsigned step=0;step<crescendoSteps;++step)if(state.programmed&(1u<<step))
      for(unsigned i=0;i<controls.size();++i)state.steps[step].emplace_back(controls[i].key,perf.steps[step*controls.size()+i].load()!=0);
    if(before==perf.revision.load())return true;
  }
  return false;
}
