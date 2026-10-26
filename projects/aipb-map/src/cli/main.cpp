#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <aipb/map/analysis/inspector.h>
#include <aipb/map/analysis/report_json.h>
#include <aipb/map/importer/bsp_importer.h>
#include <aipb/map/io/artifact.h>
#include <aipb/map/version.h>

namespace {

std::ofstream logFile;

void log(
  std::string_view level,
  const std::string &message,
  bool console = true
) {
  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm time {};
#ifdef _WIN32
  localtime_s(&time, &now);
#else
  localtime_r(&now, &time);
#endif
  std::ostringstream line;
  line << std::put_time(&time, "%Y-%m-%d %H:%M:%S") << " [" << level << "] " << message << '\n';

  if (logFile) {
    logFile << line.str();
    logFile.flush();
  }

  if (console) {
    if (level == "ERROR") {
      std::cerr << line.str();
    } else {
      std::cout << line.str();
    }
  }
}

void openLog(const std::filesystem::path &path) {
  if (path.empty()) {
    return;
  }

  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }

  logFile.open(path, std::ios::app);

  if (!logFile) {
    throw std::runtime_error("cannot open log file: " + path.string());
  }

  log("INFO", "Log file: " + path.string(), false);
}

void help() {
  std::cout << "AiPB Map Tool\n"
            << "Usage:\n"
            << "  aipb-map --help | --version\n"
            << "  aipb-map import --format bsp --input <map.bsp> --output <map.aipbmap> [--log-file <log.txt>]\n"
            << "  aipb-map inspect --input <map.aipbmap> [--output <report.json>] [--log-file <log.txt>]\n";
}

int importCommand(
  int argc,
  char *argv[]
) {
  std::string format;
  std::filesystem::path logPath;
  std::filesystem::path input;
  std::filesystem::path output;
  bool seenFormat = false;
  bool seenInput = false;
  bool seenOutput = false;
  bool seenLog = false;

  if (argc != 8 && argc != 10) {
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
    } else if (key == "--log-file" && !seenLog) {
      logPath = argv[i + 1];
      seenLog = true;
    } else {
      throw std::invalid_argument("duplicate or unrecognized import option: " + std::string(key));
    }
  }

  if (!seenFormat || !seenInput || !seenOutput || format != "bsp" || input.empty() || output.empty()) {
    throw std::invalid_argument("import requires --format bsp, nonempty --input and --output");
  }
  openLog(logPath);
  log("INFO", "Import started: " + input.string());
  aipb::map::importer::BspImporter importer;
  const auto map = importer.importFile(input);
  log("DEBUG", "Parsed: " + std::to_string(map.meshes.size()) + " meshes, " + std::to_string(map.entities.size()) +
                  " entities, " + (map.partition ? std::to_string(map.partition->clipNodes.size()) : "no") + " clipnodes", false);

  for (const auto &entity : map.entities) {
    std::map<std::string, std::size_t> seen;

    for (const auto &[key, value] : entity.sourceProperties) {
      (void)value;
      ++seen[key];
    }

    for (const auto &[key, count] : seen) {
      if (count > 1) {
        log("WARN", "Entity " + std::to_string(entity.id) + " (" + entity.type + "): duplicate key '" + key +
                        "', occurrences=" + std::to_string(count), false);
      }
    }
  }

  const auto hash = aipb::map::io::saveMap(map, output);
  log("INFO", "Artifact saved: " + output.string());
  log("INFO", "Source SHA-256: " + map.sourceSha256, false);
  log("INFO", "Artifact SHA-256: " + hash, false);
  std::cout << "Imported " << map.name << ": " << map.meshes.size() << " meshes, " << map.entities.size() << " entities\n"
            << "Map artifact SHA-256: " << hash << '\n';

  log("INFO", "Import completed successfully", false);

  return 0;
}

int inspectCommand(
  int argc,
  char *argv[]
) {
  if (argc != 4 && argc != 6 && argc != 8) {
    throw std::invalid_argument("inspect requires --input and optional --output");
  }
  std::filesystem::path input;
  std::filesystem::path output;
  std::filesystem::path logPath;
  bool seenInput = false;
  bool seenOutput = false;
  bool seenLog = false;

  for (int i = 2; i < argc; i += 2) {
    const std::string_view key(argv[i]);

    if (key == "--input" && !seenInput) {
      input = argv[i + 1];
      seenInput = true;
    } else if (key == "--output" && !seenOutput) {
      output = argv[i + 1];
      seenOutput = true;
    } else if (key == "--log-file" && !seenLog) {
      logPath = argv[i + 1];
      seenLog = true;
    } else {
      throw std::invalid_argument("duplicate or unrecognized inspect option: " + std::string(key));
    }
  }

  if (!seenInput || input.empty() || (seenOutput && output.empty())) {
    throw std::invalid_argument("inspect requires nonempty --input and --output paths");
  }
  openLog(logPath);
  log("INFO", "Inspection started: " + input.string());
  const auto artifact = aipb::map::io::loadMap(input);
  const auto report = aipb::map::analysis::inspect(artifact.model, artifact.sha256);
  log("DEBUG", "Inspection report JSON: " + aipb::map::analysis::toJson(report), false);

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

  log("INFO", "Inspection completed successfully", false);

  return 0;
}

} // namespace

int main(
  int argc,
  char *argv[]
) {
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
    log("ERROR", error.what(), false);
    std::cerr << "aipb-map: " << error.what() << "\nRun aipb-map --help for usage.\n";

    return 2;
  }
}
