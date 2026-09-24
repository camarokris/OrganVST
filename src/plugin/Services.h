// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <fstream>
#include <wx/log.h>
namespace organvst {
std::filesystem::path dataDirectory();
std::filesystem::path resourceDirectory();
class Diagnostics final : public wxLog {
public:
  explicit Diagnostics(std::filesystem::path directory);
  void text(const std::string&);
  void exportBundle(const std::filesystem::path&,const std::string& status,const std::string& controls);
protected:
  void DoLogText(const wxString& message) override;
private:
  void rotate();
  std::filesystem::path directory_,file_;
  std::ofstream output_;
  size_t bytes_=0;
};
}
