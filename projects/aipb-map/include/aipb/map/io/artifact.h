#pragma once

#include <aipb/map/model.h>

#include <filesystem>
#include <string>

namespace aipb::map::io {

struct MapArtifact {
    MapModel model;
    std::string sha256;
};

struct NavigationArtifact {
    NavigationModel model;
    std::string sha256;
};

// Writes or reads an artifact directory consisting of a manifest and JSONL streams.
// The digest covers all canonical file names and byte contents, excluding itself.
// Writers reject an existing destination to prevent mixing artifact generations.
std::string saveMap(const MapModel& model, const std::filesystem::path& directory);
MapArtifact loadMap(const std::filesystem::path& directory);
std::string saveNavigation(const NavigationModel& model, const std::filesystem::path& directory);
NavigationArtifact loadNavigation(const std::filesystem::path& directory, const MapArtifact* map = nullptr);

} // namespace aipb::map::io
