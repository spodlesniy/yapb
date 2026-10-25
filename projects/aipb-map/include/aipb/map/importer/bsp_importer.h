#pragma once

#include <aipb/map/importer/map_importer.h>

namespace aipb::map::importer {

// GoldSrc BSP version 30. Collision hull extraction is explicitly deferred.
class BspImporter final : public IMapImporter {
public:
  MapModel importFile(const std::filesystem::path &path) const override;
};

} // namespace aipb::map::importer
