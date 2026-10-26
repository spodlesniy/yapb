#pragma once

#include <aipb/map/importer/map_importer.h>

#include <functional>
#include <string>

namespace aipb::map::importer {

// GoldSrc BSP version 30. Collision hull extraction is explicitly deferred.
class BspImporter final : public IMapImporter {
public:
  MapModel importFile(const std::filesystem::path &path) const override;
  using WarningHandler = std::function<void(const std::string &)>;
  MapModel importWithWarnings(
    const std::filesystem::path &path,
    const WarningHandler &onWarning
  ) const;
};

} // namespace aipb::map::importer
