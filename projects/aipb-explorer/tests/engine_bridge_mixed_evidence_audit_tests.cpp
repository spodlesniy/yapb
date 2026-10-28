#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_mixed_evidence_audit.h>

using namespace aipb::explorer::bridge;

static void require(bool value) {
  if (!value) {
    throw std::runtime_error("mixed evidence audit regression");
  }
}

int main() {
  try {
    const char *source = "aipb_audit_source.csv";
    const char *indexPath = "aipb_audit_index.csv";
    std::remove(source);
    std::remove(indexPath);
    std::FILE *file = openTraceFile(source, "wt");
    require(file != nullptr);
    require(std::fputs("aipb-explorer-collection-v1,de_dust2,4\n", file) >= 0);
    require(std::fputs(kObservationCsvColumns, file) >= 0);
    require(std::fputs("0,3,0,Progress,direct,unknown,100,0,0,0,5,0,0,0,25,5,5,0\n", file) >= 0);
    require(std::fputs("4,7,4,BlockedCandidate,retrospective,unknown,100,0,0,0,0,0,0,0,25,0,0,0\n", file) >= 0);
    require(std::fputs("8,11,8,DeflectedCandidate,confirming,unknown,100,0,0,0,0,5,0,90,25,5,0,5\n", file) >= 0);
    require(std::fputs("12,15,12,Progress,direct,unknown,100,200,0,0,205,0,0,180,25,5,5,0\n", file) >= 0);
    require(std::fclose(file) == 0);
    SpatialIndexReport report {};
    require(writeSpatialIndex(source, indexPath, "de_dust2", report));
    SpatialIndex index {};
    require(readSpatialIndex(indexPath, "de_dust2", index));
    MixedEvidenceAudit audit {};
    require(auditMixedEvidence(index, 0, 0, 0, audit));
    require(audit.observations == 3 && audit.occupiedHeadings == 2);
    require(audit.progress == 1 && audit.blockedCandidates == 1);
    require(audit.deflectedCandidates == 1 && audit.allClassesPresent);
    require(audit.sameHeadingConflicts == 1 && audit.otherHeadingOpposition);
    require(audit.headings[0].progress == 1 && audit.headings[0].blockedCandidates == 1);
    require(audit.headings[2].deflectedCandidates == 1);
    require(auditMixedEvidence(index, 200, 0, 0, audit));
    require(audit.observations == 1 && !audit.allClassesPresent
      && audit.sameHeadingConflicts == 0);
    require(auditMixedEvidence(index, 500, 500, 0, audit));
    require(audit.observations == 0 && audit.occupiedHeadings == 0);
    audit.observations = 77;
    require(!auditMixedEvidence(index, 1e40, 0, 0, audit));
    require(audit.observations == 77);
    std::remove(source);
    std::remove(indexPath);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "audit test: %s\n", e.what());
    return 1;
  }
}
