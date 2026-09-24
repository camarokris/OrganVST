// SPDX-License-Identifier: GPL-2.0-or-later
#include "OrganVSTSampleCache.h"
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
namespace organvst {
namespace {
struct Cache {
  std::mutex mutex;
  std::unordered_multimap<uint64_t,std::weak_ptr<const SampleBytes>> blocks;
  size_t reused=0,calls=0;
};
Cache& cache(){static Cache value;return value;}
void prune(Cache& c){for(auto i=c.blocks.begin();i!=c.blocks.end();)if(i->second.expired())i=c.blocks.erase(i);else ++i;}
}
// Loader-thread only. Hash is an index, never identity: compare full payloads.
std::shared_ptr<const SampleBytes> shareSampleBytes(const unsigned char* bytes,size_t size) {
  uint64_t hash=14695981039346656037ull;
  for(size_t i=0;i<size;++i){hash^=bytes[i];hash*=1099511628211ull;}
  auto& c=cache();std::lock_guard lock(c.mutex);
  if(++c.calls%256==0)prune(c);
  auto [first,last]=c.blocks.equal_range(hash);
  for(auto i=first;i!=last;++i)if(auto data=i->second.lock()) {
    if(data->size()==size && !std::memcmp(data->data(),bytes,size)){++c.reused;return data;}
  }
  auto data=std::make_shared<const SampleBytes>(bytes,bytes+size);
  c.blocks.emplace(hash,data);return data;
}
SampleCacheStats sampleCacheStats() {
  auto& c=cache();std::lock_guard lock(c.mutex);prune(c);
  SampleCacheStats result;result.reused=c.reused;
  for(const auto& [hash,weak]:c.blocks)if(auto data=weak.lock()){result.bytes+=data->size();++result.blocks;}
  return result;
}
}
