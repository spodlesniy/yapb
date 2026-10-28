#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_observation_collection.h>

using namespace aipb::explorer::bridge;

static void require(bool v) {
  if (!v) {
    throw std::runtime_error("collection regression");
  }
}

static void writeFile(const char *path, const char *map, const char *row) {
  std::FILE *f = openTraceFile(path, "wt");
  require(f != nullptr);
  require(std::fprintf(f, "aipb-explorer-observations-v2,%s,1,100\n", map) > 0);
  require(std::fputs(kObservationCsvColumns, f) >= 0);
  require(std::fputs(row, f) >= 0);
  require(std::fclose(f) == 0);
}

int main() {
  const char *a = "aipb_collection_a.csv";
  const char *b = "aipb_collection_b.csv";
  const char *out = "aipb_collection_out.csv";
  const char *next = "aipb_collection_next.csv";
  const char *row = "0,3,0,Progress,direct,unknown,100,0,0,0,5,0,0,0,25,5,5,0\n";
  std::remove(a); std::remove(b); std::remove(out); std::remove(next);

  try {
    writeFile(a, "de_dust2", row);
    writeFile(b, "de_dust2", row);
    ObservationCollectionReport report {};
    require(mergeObservationCollections(a, b, out, "de_dust2", report));
    require(report.inputRows == 2 && report.total == 1 && report.duplicates == 1);
    require(!mergeObservationCollections(a, b, out, "de_dust2", report));
    require(mergeObservationCollections(out, b, next, "de_dust2", report));
    require(report.total == 1 && report.duplicates == 1);
    std::FILE *f = openTraceFile(next, "rt");
    require(f != nullptr);
    char line[1024] {};
    require(std::fgets(line, sizeof(line), f) != nullptr);
    require(std::strcmp(line, "aipb-explorer-collection-v1,de_dust2,1\n") == 0);
    require(std::fclose(f) == 0);
    std::remove(b);
    writeFile(b, "de_inferno", row);
    std::remove(next);
    require(!mergeObservationCollections(a, b, next, "de_dust2", report));
    require(openTraceFile(next, "rt") == nullptr);
    std::remove(b);
    writeFile(b, "de_dust2", "0,3,0,Progress,direct,unknown,100,nan,0,0,5,0,0,0,25,5,5,0\n");
    require(!mergeObservationCollections(a, b, next, "de_dust2", report));
    std::remove(b);
    writeFile(b, "de_dust2", "0,3,0,BlockedCandidate,direct,unknown,100,0,0,0,0,0,0,0,25,0,0,0\n");
    require(!mergeObservationCollections(a, b, next, "de_dust2", report));
    std::remove(a); std::remove(b); std::remove(out); std::remove(next);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "collection test: %s\n", error.what());
    std::remove(a); std::remove(b); std::remove(out); std::remove(next);
    return 1;
  }
}
