// SPDX-License-Identifier: GPL-2.0-or-later
#include "../engine/OrganInstance.h"
#include "../engine/OrganVSTSampleCache.h"
#include <wx/init.h>
#include <wx/log.h>
#include <array>
#include <cmath>
#include <iostream>
#include <set>
using namespace organvst;
static double energy(OrganInstance& organ,unsigned blocks=180) {
  std::array<float,127> l{},r{};double sum=0;
  for(unsigned j=0;j<blocks;++j){organ.render(l.data(),r.data(),127);for(unsigned i=0;i<127;++i){if(!std::isfinite(l[i])||!std::isfinite(r[i]))throw std::runtime_error("Non-finite audio");sum+=l[i]*l[i]+r[i]*r[i];}}
  return sum;
}
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
  if(argc<3)return 2;wxInitializer wx;if(!wx.IsOk())return 3;
  wxLog::SetActiveTarget(new wxLogStderr());
  try {
    auto progress=[](unsigned,const std::string&){return true;};
    auto first=std::make_unique<OrganInstance>(argv[1],argv[2],GO_RESOURCE_DIR,48000,progress);
    auto surface=first->surface();std::set<std::string> keys;
    for(unsigned i=0;i<surface->controls.size();++i){const auto& c=surface->controls[i];require(keys.insert(c.key).second,"Duplicate control key");std::cout<<i<<'\t'<<c.key<<'\t'<<c.group<<'\t'<<c.kind<<'\t'<<c.name<<'\t'<<c.channel+1<<'\t'<<surface->values[i].actual.load()<<'\n';}
    if(argc>3) {
      if(std::string(argv[3])=="audition") {
        for(unsigned i=0;i<surface->controls.size();++i)first->stop(i,false);
        for(int channel:{15,0,1,2,3,4,5,6,7,8}) {
          unsigned selected=unsigned(surface->controls.size());
          for(unsigned i=0;i<surface->controls.size();++i)
            if(surface->controls[i].channel==channel && surface->controls[i].kind=="Stop"){selected=i;break;}
          require(selected<surface->controls.size(),"Missing representative division");
          first->stop(selected,true);const auto& control=surface->controls[selected];
          const unsigned pitch=control.firstNote<=60&&control.lastNote>=60?60:control.firstNote;
          surface->audition.store(channel*128+pitch);first->applyCommands();
          double e=energy(*first);require(e>0,"Representative division audition is silent");
          energy(*first,2400);require(energy(*first)==0,"Representative audition tail did not end");
          first->stop(selected,false);
          std::cout<<"AUDITION PASS "<<surface->controls[selected].group<<" / "<<surface->controls[selected].name<<" energy="<<e<<'\n';
        }
        auto memory=sampleCacheStats();
        std::cout<<"SAMPLE CACHE bytes="<<memory.bytes<<" blocks="<<memory.blocks<<" reuses="<<memory.reused<<'\n';
      }
      return 0;
    } // Private packs are never required by CI.
    require(surface->controls.size()==134,"Authored catalog is incomplete or includes generated couplers");
    auto find=[&](const std::string& name){for(unsigned i=0;i<surface->controls.size();++i)if(surface->controls[i].name==name)return i;throw std::runtime_error("Missing control "+name);};
    require(surface->values[find("Default voice")].actual.load(),"Authored default was cleared");
    first->note(0,60,100);require(energy(*first)>0,"Default stop silent");first->panic();energy(*first,2400);
    first->stop(find("Default voice"),false);
    surface->values[find("Late voice")].requested.store(1);first->applyCommands();first->publishControls();
    require(surface->values[find("Late voice")].actual.load(),"Control past 128 did not engage");
    first->note(1,60,100);require(energy(*first)>0,"Second division is silent");first->panic();energy(*first,2400);
    first->note(0,60,100);require(energy(*first)==0,"Unexpected uncoupled voice");
    first->stop(find("Second to first"),true);require(energy(*first)>0,"Authored coupler did not route held note");first->panic();energy(*first,2400);
    first->stop(find("Late voice"),false);first->stop(find("Switch voice"),true);
    first->note(1,60,100);require(energy(*first)>0,"Switch-controlled rank silent");first->panic();energy(*first,2400);
    first->stop(find("Switch voice"),false);
    require(surface->controls[find("Aux voice")].firstNote==36 && surface->controls[find("Aux voice")].lastNote==37,"Auxiliary note range incorrect");
    surface->audition.store(2*128+36);first->applyCommands();require(energy(*first)>0,"Auxiliary division audition silent");energy(*first,2400);require(energy(*first)==0,"Audition did not release");
    auto before=sampleCacheStats();
    OrganInstance second(argv[1],argv[2],GO_RESOURCE_DIR,48000,progress);
    auto after=sampleCacheStats();require(after.bytes==before.bytes&&after.reused>before.reused,"Duplicate instance copied immutable payloads");
    first.reset();second.note(0,60,100);require(energy(second)>0,"Samples died with first instance");second.panic();
    std::cout<<"PASS catalog, defaults, >128, coupler, switch, auxiliary audition, shared sample lifetime; shared_bytes="<<after.bytes<<" blocks="<<after.blocks<<" reuses="<<after.reused<<'\n';
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
