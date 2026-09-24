// SPDX-License-Identifier: GPL-2.0-or-later
#include "engine/Performance.h"
#include <stdexcept>
#include <iostream>
#include <tuple>
int main() {
  using namespace organvst;NoteRouter router;
  std::vector<std::tuple<unsigned,unsigned,unsigned>> messages;
  auto send=[&](unsigned ch,unsigned pitch,unsigned velocity){messages.emplace_back(ch,pitch,velocity);};
  auto check=[](bool ok){if(!ok)throw std::runtime_error("Layer note routing mismatch");};
  router.dispatch(3,7,60,100,send);check(messages.size()==2&&std::get<0>(messages[0])==0&&std::get<0>(messages[1])==1);
  router.dispatch(3,8,60,100,send);messages.clear();
  router.dispatch(3,7,60,0,send);check(messages.empty());
  router.dispatch(3,8,60,0,send);check(messages.size()==2&&std::get<2>(messages[0])==0);
  router.reset();messages.clear();router.dispatch(0,8,61,100,send);
  check(messages.size()==1&&std::get<0>(messages[0])==8);
  messages.clear();router.dispatch(3,16,60,100,send);router.dispatch(3,0,128,100,send);check(messages.empty());
  std::cout<<"PASS layered overlap, native-channel independence, reset and invalid input\n";
}
