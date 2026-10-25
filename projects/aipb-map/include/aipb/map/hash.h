#pragma once

#include <string>
#include <string_view>

namespace aipb::map {

std::string sha256Hex(std::string_view bytes);

} // namespace aipb::map
