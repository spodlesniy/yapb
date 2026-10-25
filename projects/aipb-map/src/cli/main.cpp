#include <iostream>
#include <string_view>

#include <aipb/map/version.h>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "aipb-map " << aipb::map::version() << '\n';
        return 0;
    }

    if (argc == 1 || (argc == 2 && std::string_view{argv[1]} == "--help")) {
        std::cout << "AiPB Map Tool (foundation only)\n"
                  << "Usage: aipb-map [--help|--version]\n";
        return 0;
    }

    std::cerr << "Unsupported argument. Run aipb-map --help.\n";
    return 2;
}
