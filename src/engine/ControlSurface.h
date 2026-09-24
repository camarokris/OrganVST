// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>
namespace organvst {
struct ControlDescriptor {
  std::string key, name, group, kind;
  int channel=-1; // zero-based; -1 for global controls
};
struct ControlValue {
  std::atomic<int> requested{-1};
  std::atomic<bool> actual{false};
};
// Constructed on the loader, shared with the message thread. Audio owns the model
// and only exchanges atomic values here; it never takes/drops shared ownership.
struct ControlSurface {
  unsigned generation=0;
  std::vector<ControlDescriptor> controls;
  std::unique_ptr<ControlValue[]> values;
  std::atomic<int> audition{-1}; // MIDI channel, one bounded half-second note
  std::atomic<bool> ready{false};
  std::string metadata() const;
};
inline std::string ControlSurface::metadata() const {
  std::string result;
  auto clean=[](std::string s){for(char& c:s)if(c=='\t'||c=='\n'||c=='\r')c=' ';return s;};
  for(const auto& c:controls)result+=clean(c.key)+'\t'+clean(c.group)+'\t'+clean(c.kind)+'\t'+std::to_string(c.channel)+'\t'+clean(c.name)+'\n';
  return result;
}
}
