// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <functional>
#include <string>
namespace organvst {
// All methods run off the audio thread. An extracted pack lives as long as its
// engine; failures and cancellation remove only the newly created cache folder.
class OrganAssets final {
public:
  using Progress = std::function<bool(unsigned,const std::string&)>;
  OrganAssets(const std::filesystem::path& source,
              const std::filesystem::path& cache, const Progress& progress);
  ~OrganAssets();
  OrganAssets(const OrganAssets&) = delete;
  OrganAssets& operator=(const OrganAssets&) = delete;
  const std::filesystem::path& definition() const { return definition_; }
private:
  std::filesystem::path definition_, extraction_;
};
}
