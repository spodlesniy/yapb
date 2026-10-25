#include <aipb/map/version.h>

namespace aipb::map {

std::string_view version() noexcept {
    return AIPB_MAP_VERSION;
}

} // namespace aipb::map
