#pragma once

#include <string>
#include <string_view>

namespace aipb::map::io {
std::string sha256Hex(std::string_view data);
}
