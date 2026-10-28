#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_direction_evidence.h>

using namespace aipb::explorer::bridge;

static void require(bool valid) {
  if (!valid) {
    throw std::runtime_error("directional evidence regression");
  }
}

int main() {
  const char *source = "aipb_d225_source.csv";
  const char *indexPath = "aipb_d225_index.csv";
  const char *path = "aipb_d225_directions.csv";
  std::remove(source); std::remove(indexPath); std::remove(path);

  try {
    require(directionBin(0) == 0 && directionBin(22.49) == 0);
    require(directionBin(22.5) == 1 && directionBin(359) == 0);
    require(directionBin(-90) == 6 && directionBin(180) == 4);
    std::FILE *file = openTraceFile(source, "wt");
    require(file != nullptr);
    require(std::fputs("aipb-explorer-collection-v1,de_dust2,4\n", file) >= 0);
    require(std::fputs(kObservationCsvColumns, file) >= 0);
    require(std::fputs("0,3,0,Progress,direct,unknown,100,0,0,0,5,0,0,0,25,5,5,0\n", file) >= 0);
    require(std::fputs("4,7,4,BlockedCandidate,retrospective,unknown,100,0,0,0,0,0,0,0,25,0,0,0\n", file) >= 0);
    require(std::fputs("8,11,8,DeflectedCandidate,confirming,unknown,100,0,0,0,0,5,0,90,25,5,0,5\n", file) >= 0);
    require(std::fputs("12,15,12,Progress,direct,unknown,100,200,0,0,205,0,0,180,25,5,5,0\n", file) >= 0);
    require(std::fclose(file) == 0);
    SpatialIndexReport spatial {};
    require(writeSpatialIndex(source, indexPath, "de_dust2", spatial));
    DirectionStatisticsReport report {};
    require(writeDirectionalEvidence(indexPath, path, "de_dust2", report));
    require(report.cells == 2 && report.bins == 3 && report.observations == 4);
    require(report.conflictedBins == 1 && report.crossHeadingMixedCells == 1);
    require(!writeDirectionalEvidence(indexPath, path, "de_dust2", report));
    DirectionStatistics stats {};
    require(readDirectionalEvidence(path, "de_dust2", stats));
    DirectionQuery query {};
    require(queryDirectionalEvidence(stats, 0, 0, 0, 0, query));
    require(query.evidence.observations == 2 && query.evidence.progress == 1);
    require(query.evidence.blockedCandidates == 1 && query.sameHeadingConflict);
    require(query.otherHeadingOpposition && query.headingsInCell == 2);
    require(queryDirectionalEvidence(stats, 0, 0, 0, 90, query));
    require(query.evidence.deflectedCandidates == 1 && !query.sameHeadingConflict);
    require(query.otherHeadingOpposition);
    require(queryDirectionalEvidence(stats, 0, 0, 0, 180, query));
    require(query.evidence.observations == 0 && query.headingsInCell == 2);
    require(queryDirectionalEvidence(stats, 500, 500, 0, 0, query));
    require(query.evidence.observations == 0 && query.headingsInCell == 0);
    require(!readDirectionalEvidence(path, "de_inferno", stats));
    require(!queryDirectionalEvidence(stats, 1e50, 0, 0, 0, query));
    std::remove(source); std::remove(indexPath); std::remove(path);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "directional evidence: %s\n", error.what());
    std::remove(source); std::remove(indexPath); std::remove(path);
    return 1;
  }
}
