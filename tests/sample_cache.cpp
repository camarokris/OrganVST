// SPDX-License-Identifier: GPL-2.0-or-later
#include "OrganVSTSampleCache.h"
#include <array>
#include <thread>
#include <iostream>
#include <stdexcept>
using namespace organvst;
int main() {
  try {
    std::array<unsigned char,1000> a{};a.fill(7);auto b=a;b[500]=8;
    std::array<std::shared_ptr<const SampleBytes>,8> shared;
    std::array<std::thread,8> workers;
    for(unsigned i=0;i<8;++i)workers[i]=std::thread([&,i]{shared[i]=shareSampleBytes(a.data(),a.size());});
    for(auto& t:workers)t.join();
    for(const auto& p:shared)if(p.get()!=shared[0].get())throw std::runtime_error("Concurrent loads failed to share");
    auto changed=shareSampleBytes(b.data(),b.size());
    if(changed.get()==shared[0].get()||(*changed)[500]!=8||(*shared[0])[500]!=7)throw std::runtime_error("Changed sample aliased existing data");
    if(sampleCacheStats().bytes!=2000)throw std::runtime_error("Unexpected resident payload bytes");
    for(auto& p:shared)p.reset();changed.reset();
    if(sampleCacheStats().bytes||sampleCacheStats().blocks)throw std::runtime_error("Unused samples retained");
    std::cout<<"PASS concurrent sharing, changed content isolation, final-owner reclamation\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
