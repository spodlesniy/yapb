#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <aipb/explorer/engine_bridge_spatial_index.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("spatial index regression");
  }
}

static void writeInput(const char *path, const char *map) {
  std::FILE *file = openTraceFile(path, "wt");
  require(file != nullptr);
  require(std::fprintf(file, "aipb-explorer-collection-v1,%s,3\n", map) > 0);
  require(std::fputs(kObservationCsvColumns, file) >= 0);
  require(std::fputs("0,3,0,Progress,direct,unknown,100,0,0,0,400,0,0,0,25,400,400,0\n", file) >= 0);
  require(std::fputs("4,7,4,BlockedCandidate,retrospective,unknown,100,500,200,0,500,200,0,0,25,0,0,0\n", file) >= 0);
  require(std::fputs("8,11,4,DeflectedCandidate,confirming,unknown,100,-200,0,10,-180,40,10,0,25,45,20,40\n", file) >= 0);
  require(std::fclose(file) == 0);
}

int main() {
  const char *source = "aipb_spatial_source.csv";
  const char *path = "aipb_spatial_index.csv";
  const char *bad = "aipb_spatial_bad.csv";
  std::remove(source); std::remove(path); std::remove(bad);

  try {
    writeInput(source, "de_dust2");
    SpatialIndexReport report {};
    require(writeSpatialIndex(source, path, "de_dust2", report));
    require(report.records == 3 && report.postings >= 6 && report.cells >= 5);
    require(!writeSpatialIndex(source, path, "de_dust2", report));
    require(!writeSpatialIndex(source, bad, "de_inferno", report));
    SpatialIndex index {};
    require(readSpatialIndex(path, "de_dust2", index));
    require(index.records.size() == 3);
    SpatialQueryResult hit {};
    // The middle of a long segment must be found even if endpoints lie outside the query cell.
    require(searchSpatialIndex(index, 200, 0, 0, 10, hit));
    require(hit.matches == 1 && hit.nearestRecord == 0 && hit.nearestDistance == 0.0);
    require(searchSpatialIndex(index, 500, 200, 0, 10, hit));
    require(hit.matches == 1 && hit.nearestRecord == 1);
    require(searchSpatialIndex(index, -190, 20, 10, 5, hit));
    require(hit.matches == 1 && hit.nearestRecord == 2);
    require(searchSpatialIndex(index, 900, 900, 0, 10, hit));
    require(hit.matches == 0 && hit.nearestRecord == -1);
    hit.matches = 99;
    require(!searchSpatialIndex(index, 0, 0, 0, 0, hit));
    require(hit.matches == 99);
    require(!readSpatialIndex(path, "de_inferno", index));
    std::FILE *f = openTraceFile(bad, "wt");
    require(f != nullptr);
    require(std::fputs("aipb-explorer-spatial-index-v1,de_dust2,128,3,1\n", f) >= 0);
    require(std::fclose(f) == 0);
    require(!readSpatialIndex(bad, "de_dust2", index));
    std::remove(source); std::remove(path); std::remove(bad);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "spatial index test: %s\n", error.what());
    std::remove(source); std::remove(path); std::remove(bad);
    return 1;
  }
}
