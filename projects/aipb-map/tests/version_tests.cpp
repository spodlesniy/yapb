#include <iostream>
#include <string_view>

#include <aipb/map/version.h>

int main() {
    constexpr std::string_view expected = "0.1.0";
    if (aipb::map::version() != expected) {
        std::cerr << "Unexpected map core version.\n";
        return 1;
    }
    std::cout << "Map core version: " << aipb::map::version() << '\n';
    return 0;
}
