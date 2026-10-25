# Standalone AiPB projects

Independent projects live here and own their build configuration, source dependencies, tests, and CI.
The legacy YaPB game DLL stays in the repository root and is not a parent build target of these projects.
Projects may share documented file formats with YaPB without including YaPB headers or linking to its dependencies.

- [`aipb-map`](aipb-map/README.md): future offline map import, navigation generation, and export; D202 provides only a standalone C++ foundation.
