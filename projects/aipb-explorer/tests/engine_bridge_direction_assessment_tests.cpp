#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_direction_assessment.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("direction assessment regression");
  }
}

int main() {
  const char *source = "aipb_assess_source.csv";
  const char *path = "aipb_assess_index.csv";
  std::remove(source);
  std::remove(path);

  try {
    std::FILE *f = openTraceFile(source, "wt");
    require(f != nullptr);
    require(std::fputs("aipb-explorer-collection-v1,de_dust2,6\n", f) >= 0);
    require(std::fputs(kObservationCsvColumns, f) >= 0);
    require(std::fputs("0,3,0,Progress,direct,unknown,100,0,0,0,5,0,0,0,25,5,5,0\n", f) >= 0);
    require(std::fputs("4,7,4,BlockedCandidate,retrospective,unknown,100,0,0,0,0,0,0,0,25,0,0,0\n", f) >= 0);
    require(std::fputs("8,11,8,DeflectedCandidate,confirming,unknown,100,0,0,0,0,5,0,90,25,5,0,5\n", f) >= 0);
    require(std::fputs("12,15,12,Progress,direct,unknown,100,200,0,0,205,0,0,180,25,5,5,0\n", f) >= 0);
    require(std::fputs("16,19,16,Progress,direct,unknown,100,200,0,0,206,0,0,180,25,6,6,0\n", f) >= 0);
    require(std::fputs("20,23,20,BlockedCandidate,confirming,unknown,100,400,0,0,400,0,0,45,25,0,0,0\n", f) >= 0);
    require(std::fclose(f) == 0);
    SpatialIndexReport writeReport {};
    require(writeSpatialIndex(source, path, "de_dust2", writeReport));
    SpatialIndex index {};
    require(readSpatialIndex(path, "de_dust2", index));
    DirectionAssessment result {};
    require(assessDirectionalEvidence(index, 0, 0, 0, 0, result));
    require(result.status == DirectionAssessmentStatus::Conflicted);
    require(result.observations == 2 && result.otherHeadingOpposition);
    require(assessDirectionalEvidence(index, 0, 0, 0, 90, result));
    require(result.status == DirectionAssessmentStatus::Insufficient);
    require(result.observations == 1 && result.deflectedCandidates == 1);
    // Restriction in this heading opposes Progress in heading bin 0.
    require(result.otherHeadingOpposition);
    require(assessDirectionalEvidence(index, 200, 0, 0, 180, result));
    require(result.status == DirectionAssessmentStatus::ProgressEvidence);
    require(result.observations == 2 && !result.otherHeadingOpposition);
    require(assessDirectionalEvidence(index, 400, 0, 0, 45, result));
    require(result.status == DirectionAssessmentStatus::Insufficient);
    require(result.blockedCandidates == 1 && !result.otherHeadingOpposition);
    require(assessDirectionalEvidence(index, 0, 0, 0, 180, result));
    require(result.status == DirectionAssessmentStatus::Unobserved);
    require(result.observations == 0 && !result.otherHeadingOpposition);
    require(assessDirectionalEvidence(index, 1000, 0, 0, 0, result));
    require(result.status == DirectionAssessmentStatus::Unobserved);
    require(result.observations == 0 && !result.otherHeadingOpposition);
    result.observations = 123;
    require(!assessDirectionalEvidence(index, 1e40, 0, 0, 0, result));
    require(result.observations == 123);
    DirectionEvidence restriction {};
    restriction.observations = 2;
    restriction.blockedCandidates = 1;
    restriction.deflectedCandidates = 1;
    require(classifyDirectionEvidence(restriction) == DirectionAssessmentStatus::RestrictionEvidence);
    std::remove(source);
    std::remove(path);
    return 0;
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "assessment: %s\n", error.what());
    std::remove(source);
    std::remove(path);
    return 1;
  }
}
