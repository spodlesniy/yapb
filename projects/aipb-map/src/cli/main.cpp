#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <aipb/map/analysis/inspector.h>
#include <aipb/map/analysis/report_json.h>
#include <aipb/map/collision/validation.h>
#include <aipb/map/heightfield/builder.h>
#include <aipb/map/importer/bsp_importer.h>
#include <aipb/map/io/artifact.h>
#include <aipb/map/navigation/candidates.h>
#include <aipb/map/navigation/connectivity.h>
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
            << "  aipb-map inspect --input <map.aipbmap> [--output <report.json>] [--log-file <log.txt>]\n"
            << "  aipb-map validate-collision --input <map.aipbmap> --output <report.json> [--log-file <log.txt>]\n"
            << "  aipb-map heightfield --input <map.aipbmap> --output <report.json>\n"
            << "  aipb-map navigation-candidates --input <map.aipbmap> --output <report.json>\n"
            << "  aipb-map preliminary-connectivity --input <map.aipbmap> --output <report.json>\n"
            << "  aipb-map export-navvis --input <map.aipbmap> --output <map.nviz>\n";
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
  const auto map = importer.importWithWarnings(input, [](const std::string &warning) {
    log("WARN", warning);
  });
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


int validationCommand(
  int argc,
  char *argv[]
) {
  if (argc != 6 && argc != 8) {
    throw std::invalid_argument("validate-collision requires --input, --output and optional --log-file");
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
      throw std::invalid_argument("duplicate or unrecognized validate-collision option: " + std::string(key));
    }
  }

  if (!seenInput || !seenOutput || input.empty() || output.empty() || (seenLog && logPath.empty())) {
    throw std::invalid_argument("validate-collision requires nonempty --input, --output and --log-file paths");
  }
  const auto artifactPath = std::filesystem::weakly_canonical(input);
  const auto insideArtifact = [&](const std::filesystem::path &path) {
    const auto relative = std::filesystem::weakly_canonical(path).lexically_relative(artifactPath);

    return !relative.empty() && *relative.begin() != "..";
  };

  if (insideArtifact(output) || (seenLog && insideArtifact(logPath))) {
    throw std::invalid_argument("collision validation output and log must be outside the map artifact directory");
  }

  if (std::filesystem::exists(output)) {
    throw std::runtime_error("validation report already exists: " + output.string());
  }
  openLog(logPath);
  log("INFO", "Collision validation started: " + input.string());
  log("INFO", "Validation report path: " + output.string(), false);
  const auto artifact = aipb::map::io::loadMap(input);
  log("INFO", "Source SHA-256: " + artifact.model.sourceSha256, false);
  log("INFO", "Artifact SHA-256: " + artifact.sha256, false);
  const auto report = aipb::map::collision::validateQueries(artifact.model, artifact.sha256);
  const auto json = aipb::map::collision::validationJson(report);
  log("DEBUG", "Collision validation report JSON: " + json, false);

  for (const auto &sample : report.samples) {
    log("DEBUG", std::string(sample.world ? "World" : "Brush") + " hull " + std::to_string(sample.hull) +
                     ": models=" + std::to_string(sample.models) + ", points=" + std::to_string(sample.points) +
                     ", solid=" + std::to_string(sample.solid) + ", empty=" + std::to_string(sample.empty) +
                     ", other=" + std::to_string(sample.other) +
                     ", stationary_traces=" + std::to_string(sample.stationaryTraces) +
                     ", segment_traces=" + std::to_string(sample.segmentTraces) +
                     ", clear_segments=" + std::to_string(sample.clearSegments) +
                     ", solid_starts=" + std::to_string(sample.solidStarts) +
                     ", impacts=" + std::to_string(sample.impacts), false);
  }

  for (const auto &metric : report.spawnProbes.hulls) {
    log("DEBUG", "Spawn hull " + std::to_string(metric.hull) +
                     ": origins=" + std::to_string(metric.origins) +
                     ", solid_origins=" + std::to_string(metric.solidOrigins) +
                     ", horizontal_clear=" + std::to_string(metric.horizontalClear) +
                     ", horizontal_impacts=" + std::to_string(metric.horizontalImpacts) +
                     ", horizontal_start_solid=" + std::to_string(metric.horizontalStartSolid) +
                     ", floor_hits=" + std::to_string(metric.floorHits) +
                     ", ceiling_hits=" + std::to_string(metric.ceilingHits), false);
  }
  log("DEBUG", "Spawn clearance differences: standing_blocked_crouching_free=" +
                   std::to_string(report.spawnProbes.standingBlockedCrouchingFree) +
                   ", standing_free_crouching_blocked=" +
                   std::to_string(report.spawnProbes.standingFreeCrouchingBlocked), false);

  for (const auto &example : report.examples) {
    log("WARN", "Collision validation violation: " + example, false);
  }
  log("INFO", "Collision validation summary: models=" + std::to_string(report.modelCount) +
                  ", spawn origins=" + std::to_string(report.sampledSpawnOrigins) +
                  ", violations=" + std::to_string(report.violations), false);
  std::ofstream stream(output, std::ios::binary);

  if (!stream || !stream.write(json.data(), static_cast<std::streamsize>(json.size()))) {
    stream.close();
    std::filesystem::remove(output);
    throw std::runtime_error("unable to write collision validation report: " + output.string());
  }
  std::cout << "Collision validation: " << report.mapName << ", models=" << report.modelCount
            << ", spawn origins=" << report.sampledSpawnOrigins << ", violations=" << report.violations << '\n';

  if (report.violations != 0) {
    log("WARN", "Collision validation completed with violations", false);

    return 1;
  }
  log("INFO", "Collision validation completed successfully", false);

  return 0;
}

int heightfieldCommand(
  int argc,
  char *argv[]
) {
  if (argc != 6 || std::string_view { argv[2] } != "--input" ||
      std::string_view { argv[4] } != "--output") {
    throw std::invalid_argument("heightfield requires --input <map.aipbmap> --output <report.json>");
  }
  const std::filesystem::path input(argv[3]);
  const std::filesystem::path output(argv[5]);

  if (input.empty() || output.empty()) {
    throw std::invalid_argument("heightfield paths must not be empty");
  }
  const auto relative = std::filesystem::weakly_canonical(output).lexically_relative(std::filesystem::weakly_canonical(input));

  if (!relative.empty() && *relative.begin() != "..") {
    throw std::invalid_argument("heightfield report must be outside the map artifact");
  }

  if (std::filesystem::exists(output)) {
    throw std::runtime_error("heightfield report already exists: " + output.string());
  }
  const auto artifact = aipb::map::io::loadMap(input);
  const auto result = aipb::map::heightfield::build(artifact.model, artifact.sha256);
  const auto json = aipb::map::heightfield::toJson(result);
  std::ofstream stream(output, std::ios::binary);

  if (!stream || !stream.write(json.data(), static_cast<std::streamsize>(json.size()))) {
    stream.close();
    std::filesystem::remove(output);
    throw std::runtime_error("unable to write heightfield report: " + output.string());
  }
  std::cout << "Heightfield: " << result.mapName << ", candidates=" << result.candidates.size()
            << ", multi-layer columns=" << result.multiLayerColumns << '\n';

  return 0;
}

int exportNavvisCommand(
  int argc,
  char *argv[]
) {
  if (argc != 6 || std::string_view { argv[2] } != "--input" ||
      std::string_view { argv[4] } != "--output") {
    throw std::invalid_argument("export-navvis requires --input <map.aipbmap> --output <map.nviz>");
  }
  const std::filesystem::path input(argv[3]);
  const std::filesystem::path output(argv[5]);

  if (input.empty() || output.empty() || output.extension() != ".nviz") {
    throw std::invalid_argument("export-navvis requires a nonempty .nviz output filename");
  }

  const auto relative = std::filesystem::weakly_canonical(output).lexically_relative(std::filesystem::weakly_canonical(input));

  if (!relative.empty() && *relative.begin() != "..") {
    throw std::invalid_argument("navigation viewer file must be outside the map artifact");
  }

  if (std::filesystem::exists(output)) {
    throw std::runtime_error("navigation viewer file already exists: " + output.string());
  }
  const auto artifact = aipb::map::io::loadMap(input);
  const auto heightfield = aipb::map::heightfield::build(artifact.model, artifact.sha256);
  const auto candidates = aipb::map::navigation::generateStaticBrushCandidates(artifact.model, heightfield);
  const auto connectivity = aipb::map::navigation::buildConnectivity(artifact.model, candidates);
  const auto &mapName = candidates.mapName;

  if (mapName.empty() || mapName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos ||
      connectivity.candidatesSha256.size() != 64) {
    throw std::runtime_error("invalid map name or candidate digest for navigation visualization");
  }
  std::ostringstream data;
  data << std::setprecision(17);
  data << "AIPBNAVVIS 1 " << mapName << ' ' << connectivity.candidatesSha256 << '\n';
  data << "COUNT " << candidates.candidates.size() << ' ' << connectivity.edges.size() << '\n';

  std::set<std::size_t> connectedIds;
  for (const auto &edge : connectivity.edges) {
    connectedIds.insert(edge.from);
    connectedIds.insert(edge.to);
  }

  for (const auto &candidate : candidates.candidates) {
    const int kind = candidate.conditionalSupport ? 3 :
                     candidate.supportEntityId ? 2 : candidate.refined ? 1 : 0;
    data << "V " << candidate.id << ' ' << candidate.position.x << ' ' << candidate.position.y << ' '
         << candidate.position.z << ' ' << kind << ' '
         << static_cast<int>(candidate.standingFree) << ' '
         << static_cast<int>(candidate.crouchingFree) << ' '
         << static_cast<int>(!connectedIds.contains(candidate.id)) << '\n';
  }

  for (const auto &edge : connectivity.edges) {
    const int kind = edge.traversal == aipb::map::navigation::TraversalKind::Walk ? 0 :
                     edge.traversal == aipb::map::navigation::TraversalKind::Jump ? 1 : 2;
    data << "E " << edge.from << ' ' << edge.to << ' ' << kind << '\n';
  }
  data << "END\n";
  const auto outputText = data.str();
  std::ofstream stream(output, std::ios::binary);

  if (!stream || !stream.write(outputText.data(), static_cast<std::streamsize>(outputText.size()))) {
    stream.close();
    std::filesystem::remove(output);
    throw std::runtime_error("unable to write navigation viewer file: " + output.string());
  }
  std::cout << "Navigation viewer export: " << mapName
            << ", nodes=" << candidates.candidates.size()
            << ", edges=" << connectivity.edges.size()
            << ", candidates_sha256=" << connectivity.candidatesSha256 << '\n';

  return 0;
}

int connectivityCommand(
  int argc,
  char *argv[]
) {
  if (argc != 6 || std::string_view { argv[2] } != "--input" ||
      std::string_view { argv[4] } != "--output") {
    throw std::invalid_argument("preliminary-connectivity requires --input <map.aipbmap> --output <report.json>");
  }
  const std::filesystem::path input(argv[3]);
  const std::filesystem::path output(argv[5]);

  if (input.empty() || output.empty()) {
    throw std::invalid_argument("preliminary-connectivity paths must not be empty");
  }
  const auto relative = std::filesystem::weakly_canonical(output).lexically_relative(std::filesystem::weakly_canonical(input));

  if (!relative.empty() && *relative.begin() != "..") {
    throw std::invalid_argument("connectivity report must be outside the map artifact");
  }

  if (std::filesystem::exists(output)) {
    throw std::runtime_error("connectivity report already exists: " + output.string());
  }
  const auto artifact = aipb::map::io::loadMap(input);
  const auto heightfield = aipb::map::heightfield::build(artifact.model, artifact.sha256);
  const auto candidates = aipb::map::navigation::generateStaticBrushCandidates(artifact.model, heightfield);
  const auto report = aipb::map::navigation::buildConnectivity(artifact.model, candidates);
  const auto json = aipb::map::navigation::connectivityJson(report);
  std::ofstream stream(output, std::ios::binary);

  if (!stream || !stream.write(json.data(), static_cast<std::streamsize>(json.size()))) {
    stream.close();
    std::filesystem::remove(output);
    throw std::runtime_error("unable to write connectivity report: " + output.string());
  }
  std::cout << "Preliminary connectivity: " << report.mapName << ", candidates=" << report.candidateCount
            << ", edges=" << report.edges.size() << ", components=" << report.weakComponents << '\n';

  return 0;
}

int navigationCandidatesCommand(
  int argc,
  char *argv[]
) {
  if (argc != 6 || std::string_view { argv[2] } != "--input" ||
      std::string_view { argv[4] } != "--output") {
    throw std::invalid_argument("navigation-candidates requires --input <map.aipbmap> --output <report.json>");
  }
  const std::filesystem::path input(argv[3]);
  const std::filesystem::path output(argv[5]);

  if (input.empty() || output.empty()) {
    throw std::invalid_argument("navigation-candidates paths must not be empty");
  }
  const auto relative = std::filesystem::weakly_canonical(output).lexically_relative(std::filesystem::weakly_canonical(input));

  if (!relative.empty() && *relative.begin() != "..") {
    throw std::invalid_argument("navigation candidates report must be outside the map artifact");
  }

  if (std::filesystem::exists(output)) {
    throw std::runtime_error("navigation candidates report already exists: " + output.string());
  }
  const auto artifact = aipb::map::io::loadMap(input);
  const auto heightfield = aipb::map::heightfield::build(artifact.model, artifact.sha256);
  const auto report = aipb::map::navigation::generateStaticBrushCandidates(artifact.model, heightfield);
  const auto json = aipb::map::navigation::candidatesJson(report);
  std::ofstream stream(output, std::ios::binary);

  if (!stream || !stream.write(json.data(), static_cast<std::streamsize>(json.size()))) {
    stream.close();
    std::filesystem::remove(output);
    throw std::runtime_error("unable to write navigation candidates report: " + output.string());
  }
  std::cout << "Navigation candidates: " << report.mapName << ", points=" << report.candidates.size()
            << ", covered spawns=" << report.coveredSpawns << "/" << report.spawnCount << '\n';

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
    if (argc >= 2 && std::string_view { argv[1] } == "validate-collision") {
      return validationCommand(argc, argv);
    }
    if (argc >= 2 && std::string_view { argv[1] } == "heightfield") {
      return heightfieldCommand(argc, argv);
    }
    if (argc >= 2 && std::string_view { argv[1] } == "navigation-candidates") {
      return navigationCandidatesCommand(argc, argv);
    }
    if (argc >= 2 && std::string_view { argv[1] } == "export-navvis") {
      return exportNavvisCommand(argc, argv);
    }
    if (argc >= 2 && std::string_view { argv[1] } == "preliminary-connectivity") {
      return connectivityCommand(argc, argv);
    }
    throw std::invalid_argument("unsupported command");
  } catch (const std::exception &error) {
    log("ERROR", error.what(), false);
    std::cerr << "aipb-map: " << error.what() << "\nRun aipb-map --help for usage.\n";

    return 2;
  }
}
