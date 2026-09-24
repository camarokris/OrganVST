// SPDX-License-Identifier: GPL-2.0-or-later
#include "OrganAssets.h"
#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <fstream>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>
namespace organvst {
namespace {
std::string lower(std::string value) {
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
  return value;
}
std::filesystem::path safeName(std::string name) {
  std::replace(name.begin(),name.end(),'\\','/');
  if(name.empty() || name[0]=='/' || name.find('\0')!=std::string::npos)
    throw std::runtime_error("ZIP contains an invalid or absolute path");
  std::filesystem::path result;
  for(const auto& part:std::filesystem::path(name)) {
    auto value=part.string();
    if(value.empty())continue;
    if(value=="." || value==".." || value.back()=='.' || value.back()==' ' ||
       value.find_first_of(":<>\"|?*")!=std::string::npos ||
       std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32;}))
      throw std::runtime_error("ZIP contains an unsafe cross-platform path: "+name);
    auto base=lower(value.substr(0,value.find('.')));
    if(base=="con" || base=="prn" || base=="aux" || base=="nul" ||
       (base.size()==4 && (base.substr(0,3)=="com" || base.substr(0,3)=="lpt") && base[3]>='1' && base[3]<='9'))
      throw std::runtime_error("ZIP contains a reserved Windows filename: "+name);
    result/=part;
  }
  if(result.empty())throw std::runtime_error("ZIP entry has no filename");
  return result;
}
}
OrganAssets::OrganAssets(const std::filesystem::path& source,
 const std::filesystem::path& cache,const Progress& progress) {
  if(lower(source.extension().string())!=".zip") {
    definition_=std::filesystem::absolute(source);return;
  }
  try {
    if(progress && !progress(0,"Extracting ZIP pack"))throw std::runtime_error("Loading cancelled");
    std::filesystem::create_directories(cache);
    std::random_device random;
    bool created=false;
    for(unsigned attempt=0;attempt<32 && !created;++attempt) {
      const auto candidate=cache/("pack-"+std::to_string(random())+"-"+std::to_string(random()));
      created=std::filesystem::create_directory(candidate);
      if(created)extraction_=candidate;
    }
    if(!created){extraction_.clear();throw std::runtime_error("Cannot create managed extraction folder");}
    std::unique_ptr<archive, decltype(&archive_read_free)> input(archive_read_new(),archive_read_free);
    if(!input)throw std::runtime_error("Cannot initialize ZIP reader");
    archive_read_support_filter_none(input.get());
    archive_read_support_format_zip(input.get());
    if(archive_read_open_filename(input.get(),source.string().c_str(),65536)!=ARCHIVE_OK)
      throw std::runtime_error("Cannot open ZIP pack");
    std::array<char,65536> buffer;
    std::set<std::string> names;
    std::vector<std::filesystem::path> definitions;
    std::uint64_t total=0;unsigned entries=0;
    constexpr std::uint64_t byteLimit=64ull*1024*1024*1024;
    archive_entry* entry=nullptr;
    for(;;) {
      const auto result=archive_read_next_header(input.get(),&entry);
      if(result==ARCHIVE_EOF)break;
      if(result!=ARCHIVE_OK)throw std::runtime_error("ZIP directory is corrupt or unsupported");
      if(++entries>200000)throw std::runtime_error("ZIP exceeds 200,000 entry limit");
      if(archive_entry_symlink(entry) || archive_entry_hardlink(entry) ||
         (archive_entry_filetype(entry)!=AE_IFREG && archive_entry_filetype(entry)!=AE_IFDIR))
        throw std::runtime_error("ZIP links and special files are not supported");
      if(archive_entry_is_encrypted(entry))throw std::runtime_error("Encrypted ZIP packs are not supported");
      const char* name=archive_entry_pathname_utf8(entry);
      if(!name)name=archive_entry_pathname(entry);
      if(!name)throw std::runtime_error("ZIP entry has no filename");
      const auto relative=safeName(name);
      const auto destination=extraction_/relative;
      if(!names.insert(lower(relative.generic_string())).second)
        throw std::runtime_error("ZIP has duplicate or case-colliding entries");
      if(progress && !progress(0,"Extracting "+relative.generic_string()))throw std::runtime_error("Loading cancelled");
      if(archive_entry_filetype(entry)==AE_IFDIR) {std::filesystem::create_directories(destination);continue;}
      if(archive_entry_size(entry)>static_cast<la_int64_t>(byteLimit))throw std::runtime_error("ZIP entry exceeds extraction size limit");
      std::filesystem::create_directories(destination.parent_path());
      if(std::filesystem::exists(destination))throw std::runtime_error("ZIP entry collides with an existing path");
      std::ofstream out(destination,std::ios::binary);
      if(!out)throw std::runtime_error("Cannot create extracted file");
      for(;;) {
        if(progress && !progress(0,"Extracting "+relative.generic_string()))throw std::runtime_error("Loading cancelled");
        const auto count=archive_read_data(input.get(),buffer.data(),buffer.size());
        if(count<0)throw std::runtime_error("ZIP is corrupt or failed its integrity check");
        if(!count)break;
        total+=static_cast<std::uint64_t>(count);
        if(total>byteLimit)throw std::runtime_error("ZIP exceeds 64 GiB extraction limit");
        out.write(buffer.data(),count);
        if(!out)throw std::runtime_error("Cannot write extracted file (disk may be full)");
      }
      out.close();if(!out)throw std::runtime_error("Cannot finalize extracted file");
      const auto extension=lower(relative.extension().string());
      if(extension==".organ" || extension==".odf")definitions.push_back(destination);
    }
    if(definitions.empty())throw std::runtime_error("ZIP contains no organ definition");
    if(definitions.size()!=1)throw std::runtime_error("ZIP contains multiple definitions; extract it and select the desired definition");
    definition_=definitions.front();
  } catch(...) {
    if(!extraction_.empty()){std::error_code error;std::filesystem::remove_all(extraction_,error);extraction_.clear();}
    throw;
  }
}
OrganAssets::~OrganAssets() {
  if(!extraction_.empty()){std::error_code error;std::filesystem::remove_all(extraction_,error);}
}
}
