#pragma once

#include <aipb/map/analysis/inspector.h>

#include <string>

namespace aipb::map::analysis {

// The report is separate from the imported map artifact and does not change its hash.
std::string toJson(const MapReport& report);

} // namespace aipb::map::analysis
