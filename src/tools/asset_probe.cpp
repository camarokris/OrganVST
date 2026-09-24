// SPDX-License-Identifier: GPL-2.0-or-later
#include "../assets/OrganAssets.h"
#include <wx/init.h>
#include <iostream>
int main(int argc,char** argv) {
  if(argc<3)return 2;
  wxInitializer wx;if(!wx.IsOk())return 3;
  try {
    unsigned calls=0;
    organvst::OrganAssets assets(argv[1],argv[2],[&](unsigned,const std::string&){return argc<4 || ++calls<3;});
    if(!std::filesystem::is_regular_file(assets.definition()))return 4;
    std::cout<<assets.definition()<<'\n';return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
