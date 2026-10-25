#pragma once

#include <filesystem>

#include <aipb/map/model.h>

namespace aipb::map::importer {

class IMapImporter {
public:
  virtual ~IMapImporter() = default;
  virtual MapModel importFile(const std::filesystem::path &path) const = 0;
};

} // namespace aipb::map::importer
