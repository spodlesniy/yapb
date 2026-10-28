#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_area_statistics.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("area statistics regression");
  }
}

int main() {
  const char *source = "aipb_area_source.csv";
  const char *indexPath = "aipb_area_index.csv";
  const char *output = "aipb_area_output.csv";
  const char *invalid = "aipb_area_invalid.csv";
  std::remove(source); std::remove(indexPath);
  std::remove(output); std::remove(invalid);

  try {
    std::FILE *file = openTraceFile(source, "wt");
    require(file != nullptr);
    require(std::fputs("aipb-explorer-collection-v1,de_dust2,3\n", file) >= 0);
    require(std::fputs(kObservationCsvColumns, file) >= 0);
    require(std::fputs("0,3,0,Progress,direct,unknown,100,0,0,0,200,0,0,0,25,200,200,0\n", file) >= 0);
    require(std::fputs("4,7,4,BlockedCandidate,retrospective,unknown,100,10,20,0,10,20,0,0,25,0,0,0\n", file) >= 0);
    require(std::fputs("8,11,4,DeflectedCandidate,confirming,unknown,100,20,0,0,30,10,0,0,25,14,5,10\n", file) >= 0);
    require(std::fclose(file) == 0);

    SpatialIndexReport indexReport {};
    require(writeSpatialIndex(source, indexPath, "de_dust2", indexReport));
    AreaStatisticsReport report {};
    require(writeAreaStatistics(indexPath, output, "de_dust2", report));
    require(report.cells == 2 && report.observations == 4);
    require(report.progress == 2 && report.blockedCandidates == 1
      && report.deflectedCandidates == 1);
    require(!writeAreaStatistics(indexPath, output, "de_dust2", report));
    require(!writeAreaStatistics(indexPath, invalid, "de_inferno", report));

    AreaStatistics result {};
    require(readAreaStatistics(output, "de_dust2", result));
    AreaEvidence area {};
    require(queryAreaStatistics(result, 0, 0, 0, area));
    require(area.cell.x == 0 && area.observations == 3);
    require(area.progress == 1 && area.blockedCandidates == 1
      && area.deflectedCandidates == 1);
    require(queryAreaStatistics(result, 150, 0, 0, area));
    require(area.cell.x == 1 && area.observations == 1 && area.progress == 1);
    require(queryAreaStatistics(result, -140, 0, 0, area));
    require(area.observations == 0 && area.progress == 0);
    area.observations = 42;
    require(!queryAreaStatistics(result, 1e40, 0, 0, area));
    require(area.observations == 42);
    require(!readAreaStatistics(output, "de_inferno", result));

    file = openTraceFile(invalid, "wt");
    require(file != nullptr);
    require(std::fputs("aipb-explorer-area-statistics-v1,de_dust2,128,3,4,2\n", file) >= 0);
    require(std::fputs(kAreaStatisticsColumns, file) >= 0);
    require(std::fputs("0,0,0,3,3,1,0\n", file) >= 0);
    require(std::fputs("1,0,0,1,1,0,0\n", file) >= 0);
    require(std::fclose(file) == 0);
    require(!readAreaStatistics(invalid, "de_dust2", result));
    std::remove(source); std::remove(indexPath);
    std::remove(output); std::remove(invalid);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "area statistics test: %s\n", error.what());
    std::remove(source); std::remove(indexPath);
    std::remove(output); std::remove(invalid);
    return 1;
  }
}
