// SPDX-License-Identifier: GPL-2.0-or-later
#include "../engine/OrganInstance.h"
#include <wx/init.h>
#include <wx/log.h>
#include <array>
#include <cmath>
#include <iostream>
static double energy(organvst::OrganInstance& instance) {
  std::array<float,127> left{},right{};double result=0;
  for(unsigned block=0;block<400;++block) {
    instance.render(left.data(),right.data(),left.size());
    for(unsigned i=0;i<left.size();++i) {
      if(!std::isfinite(left[i]) || !std::isfinite(right[i]))throw std::runtime_error("Non-finite audio");
      result+=left[i]*left[i]+right[i]*right[i];
    }
  }
  return result;
}
int main(int argc,char** argv) {
  if(argc!=3)return 2;
  wxInitializer wx;if(!wx.IsOk())return 3;
  wxLog::SetActiveTarget(new wxLogStderr());
  try {
    auto progress=[](unsigned,const std::string&){return true;};
    organvst::OrganInstance first(argv[1],argv[2],GO_RESOURCE_DIR,48000,progress);
    organvst::OrganInstance second(argv[1],argv[2],GO_RESOURCE_DIR,48000,progress);
    first.stop(0,true);first.note(0,60,100);
    if(energy(first)<=0 || energy(second)!=0)throw std::runtime_error("Instance isolation failed");
    second.stop(0,true);second.note(0,60,100);first.panic();
    energy(first); // allow release tail to finish
    if(energy(first)!=0 || energy(second)<=0)throw std::runtime_error("Panic affected the other instance");
    second.panic();
    std::cout<<"PASS two independent engines and release tails\n";
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
