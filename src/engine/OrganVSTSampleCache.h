// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <vector>
#include <cstddef>
namespace organvst {
using SampleBytes=std::vector<unsigned char>;
std::shared_ptr<const SampleBytes> shareSampleBytes(const unsigned char*,size_t);
struct SampleCacheStats { size_t bytes=0,blocks=0,reused=0; };
SampleCacheStats sampleCacheStats();
}
