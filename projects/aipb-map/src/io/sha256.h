#pragma once

#include <aipb/map/hash.h>

namespace aipb::map::io {

inline std::string sha256Hex(std::string_view bytes) {
    return aipb::map::sha256Hex(bytes);
}

} // namespace aipb::map::io
