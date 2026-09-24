// SPDX-License-Identifier: GPL-2.0-or-later
#include "Services.h"
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <sstream>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace organvst {
std::filesystem::path dataDirectory() {
#ifdef _WIN32
  const char* base=std::getenv("LOCALAPPDATA");
  return (base?std::filesystem::path(base):std::filesystem::temp_directory_path())/"OrganVST";
#else
  const char* base=std::getenv("HOME");
  return (base?std::filesystem::path(base):std::filesystem::temp_directory_path())/"Library/Application Support/OrganVST";
#endif
}
std::filesystem::path resourceDirectory() {
#ifdef _WIN32
  HMODULE module=nullptr;wchar_t path[32768]{};
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    reinterpret_cast<LPCWSTR>(&resourceDirectory),&module);
  GetModuleFileNameW(module,path,32768);
  return std::filesystem::path(path).parent_path().parent_path()/"Resources";
#else
  Dl_info info{};
  if(!dladdr(reinterpret_cast<void*>(&resourceDirectory),&info))throw std::runtime_error("Cannot locate plugin resources");
  return std::filesystem::path(info.dli_fname).parent_path().parent_path()/"Resources";
#endif
}
Diagnostics::Diagnostics(std::filesystem::path directory):directory_(std::move(directory)) {
  std::filesystem::create_directories(directory_);
  static std::atomic<unsigned> sequence{0};
  auto timestamp=std::chrono::system_clock::now().time_since_epoch().count();
  file_=directory_/(std::to_string(timestamp)+"-"+std::to_string(sequence++)+".log");
  output_.open(file_);
  text("OrganVST 0.1.0 development session; GrandOrgue 85304331; VST3 SDK 3.8.1");
}
void Diagnostics::rotate() {
  output_.close();std::error_code error;
  for(int i=4;i>=1;--i) {
    auto source=file_.string()+(i==1?"":"."+std::to_string(i-1));
    auto target=file_.string()+"."+std::to_string(i);
    std::filesystem::remove(target,error);std::filesystem::rename(source,target,error);
  }
  output_.open(file_,std::ios::trunc);bytes_=0;
}
void Diagnostics::text(const std::string& value) {
  if(bytes_+value.size()>2*1024*1024)rotate();
  output_<<value<<'\n';output_.flush();bytes_+=value.size()+1;
}
void Diagnostics::DoLogText(const wxString& value) { text(value.ToStdString()); }
void Diagnostics::exportBundle(const std::filesystem::path& destination,const std::string& status,const std::string& controls) {
  wxFileOutputStream file(wxString::FromUTF8(destination.string()));
  if(!file.IsOk())throw std::runtime_error("Cannot create diagnostics bundle");
  wxZipOutputStream zip(file);
  const char* home=std::getenv("HOME");
#ifdef _WIN32
  home=std::getenv("USERPROFILE");
#endif
  auto add=[&](const std::string& name,std::string contents) {
    if(home && *home) {size_t pos=0;while((pos=contents.find(home,pos))!=std::string::npos){contents.replace(pos,std::strlen(home),"<user>");pos+=6;}}
    zip.PutNextEntry(wxString::FromUTF8(name));zip.Write(contents.data(),contents.size());zip.CloseEntry();
  };
  for(int i=0;i<5;++i) {
    std::ifstream input(file_.string()+(i?"."+std::to_string(i):""));
    if(input){std::ostringstream s;s<<input.rdbuf();add("session-"+std::to_string(i)+".log",s.str());}
  }
  add("status.txt",status+"\n\nControls:\n"+controls);
  add("reproduction.txt","Please describe your DAW/version, operating system, actions, expected result, and actual result.\nNo samples are included.\n");
  if(!zip.Close())throw std::runtime_error("Failed to finalize diagnostic ZIP");
}
}
