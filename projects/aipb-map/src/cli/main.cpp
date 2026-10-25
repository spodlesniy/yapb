#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <aipb/map/analysis/inspector.h>
#include <aipb/map/analysis/report_json.h>
#include <aipb/map/importer/bsp_importer.h>
#include <aipb/map/io/artifact.h>
#include <aipb/map/version.h>

namespace {

void help() {
  std::cout << "AiPB Map Tool\n"
            << "Usage:\n"
            << "  aipb-map --help | --version\n"
            << "  aipb-map import --format bsp --input <map.bsp> --output <map.aipbmap>\n"
            << "  aipb-map inspect --input <map.aipbmap> [--output <report.json>]\n";
}

int importCommand(int argc, char *argv[]) {
  std::string format;
  std::filesystem::path input;
  std::filesystem::path output;
  bool seenFormat = false;
  bool seenInput = false;
  bool seenOutput = false;

  if (argc != 9) {
    throw std::invalid_argument("import requires --format, --input and --output");
  }

  for (int i = 2; i < argc; i += 2) {
    const std::string_view key(argv[i]);

    if (key == "--format" && !seenFormat) {
      format = argv[i + 1];
      seenFormat = true;
    } else if (key == "--input" && !seenInput) {
      input = argv[i + 1];
      seenInput = true;
    } else if (key == "--output" && !seenOutput) {
      output = argv[i + 1];
      seenOutput = true;
    } else {
      throw std::invalid_argument("duplicate or unrecognized import option: " + std::string(key));
    }
  }

  if (!seenFormat || !seenInput || !seenOutput || format != "bsp" || input.empty() || output.empty()) {
    throw std::invalid_argument("import requires --format bsp, nonempty --input and --output");
  }
  aipb::map::importer::BspImporter importer;
  const auto map = importer.importFile(input);
  const auto hash = aipb::map::io::saveMap(map, output);
  std::cout << "Imported " << map.name << ": " << map.meshes.size() << " meshes, " << map.entities.size() << " entities\n"
            << "Map artifact SHA-256: " << hash << '\n';

  return 0;
}

int inspectCommand(int argc, char *argv[]) {
  if (argc != 4 && argc != 6) {
    throw std::invalid_argument("inspect requires --input and optional --output");
  }
  std::filesystem::path input;
  std::filesystem::path output;
  bool seenInput = false;
  bool seenOutput = false;

  for (int i = 2; i < argc; i += 2) {
    const std::string_view key(argv[i]);

    if (key == "--input" && !seenInput) {
      input = argv[i + 1];
      seenInput = true;
    } else if (key == "--output" && !seenOutput) {
      output = argv[i + 1];
      seenOutput = true;
    } else {
      throw std::invalid_argument("duplicate or unrecognized inspect option: " + std::string(key));
    }
  }

  if (!seenInput || input.empty() || (seenOutput && output.empty())) {
    throw std::invalid_argument("inspect requires nonempty --input and --output paths");
  }
  const auto artifact = aipb::map::io::loadMap(input);
  const auto report = aipb::map::analysis::inspect(artifact.model, artifact.sha256);

  if (seenOutput) {
    const auto relative = std::filesystem::weakly_canonical(output).lexically_relative(std::filesystem::weakly_canonical(input));

    if (!relative.empty() && *relative.begin() != "..") {
      throw std::invalid_argument("inspection report must be outside the map artifact directory");
    }

    if (std::filesystem::exists(output)) {
      throw std::runtime_error("inspection output already exists: " + output.string());
    }
    const auto json = aipb::map::analysis::toJson(report);
    std::ofstream stream(output, std::ios::binary);

    if (!stream || !stream.write(json.data(), static_cast<std::streamsize>(json.size()))) {
      stream.close();
      std::filesystem::remove(output);
      throw std::runtime_error("unable to write inspection report: " + output.string());
    }
  }
  std::cout << "Map: " << report.mapName << '\n'
            << "Artifact SHA-256: " << report.artifactSha256 << '\n'
            << "Render: " << report.meshCount << " meshes, " << report.vertexCount << " vertices, " << report.triangleCount
            << " triangles\n"
            << "Collision: " << report.collisionShapeCount << " triangle shapes, " << report.partition.models << " partition models, "
            << report.partition.renderNodes << " render nodes, " << report.partition.clipNodes << " clip nodes\n"
            << "Entities: " << report.entityCount << "\n"
            << "Findings: " << report.findings.size() << "\n";

  for (const auto &finding : report.findings) {
    std::cout << "  [" << finding.severity << "] " << finding.code << ": " << finding.detail << '\n';
  }

  return 0;
}

} // namespace

int main(int argc, char *argv[]) {
  try {
    if (argc == 2 && std::string_view { argv[1] } == "--version") {
      std::cout << "aipb-map " << aipb::map::version() << '\n';

      return 0;
    }

    if (argc == 1 || (argc == 2 && std::string_view { argv[1] } == "--help")) {
      help();

      return 0;
    }

    if (argc >= 2 && std::string_view { argv[1] } == "import") {
      return importCommand(argc, argv);
    }

    if (argc >= 2 && std::string_view { argv[1] } == "inspect") {
      return inspectCommand(argc, argv);
    }
    throw std::invalid_argument("unsupported command");
  } catch (const std::exception &error) {
    std::cerr << "aipb-map: " << error.what() << "\nRun aipb-map --help for usage.\n";

    return 2;
  }
}
