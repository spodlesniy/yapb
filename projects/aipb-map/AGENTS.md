# AiPB Map Tool project rules

These project-specific C++ rules apply under `projects/aipb-map/`.
The repository's Git workflow, one-D-step-one-commit policy, protected upstream, English code comments, and documentation rules still apply.

- Build independently from `projects/aipb-map/` using its own CMake project; do not include it as a subdirectory of the legacy YaPB build.
- Use C++20 and the standard library, including STL containers and `std::array` when appropriate.
- Follow the shared `projects/.clang-format` and `projects/.clang-tidy` configurations for all standalone C++ project files; use a formatter version that supports their full option set.
- Do not include `yapb.h`, crlib, linkage, GoldSrc SDK headers, or legacy AI headers in the Map Tool core.
- Do not link the Map Tool against the game DLL or ONNX Runtime.
- Keep the map and navigation data models independent of importer, exporter, builder, and serialization implementations.
- Add third-party dependencies only to a module that directly needs them, and document their license and purpose.
- Run the standalone CMake/CTest checks for changes here; retain tests independent of CS 1.6 and the legacy YaPB build.
- Keep source comments in English and user-facing CLI errors actionable.

Legacy YaPB-specific limits on fixed arrays and `<yapb.h>` include ordering do not apply to this standalone project's sources.
Do not change the legacy runtime's own compilation behavior while working on the Map Tool.
