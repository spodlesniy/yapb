#pragma once

#include <cmath>

#include <aipb/explorer/engine_bridge_mixed_evidence_audit.h>

namespace aipb::explorer::bridge {

inline constexpr int kMinimumDirectionEvidenceWindows = 2;

enum class DirectionAssessmentStatus {
  Unobserved,
  Insufficient,
  ProgressEvidence,
  RestrictionEvidence,
  Conflicted
};

inline const char *directionAssessmentName(DirectionAssessmentStatus status) {
  switch (status) {
  case DirectionAssessmentStatus::Unobserved:
    return "Unobserved";
  case DirectionAssessmentStatus::Insufficient:
    return "Insufficient";
  case DirectionAssessmentStatus::ProgressEvidence:
    return "ProgressEvidence";
  case DirectionAssessmentStatus::RestrictionEvidence:
    return "RestrictionEvidence";
  case DirectionAssessmentStatus::Conflicted:
    return "Conflicted";
  }

  return "Invalid";
}

// A window is not an independent trial; this is an evidence label, not a safe-route verdict.
inline DirectionAssessmentStatus classifyDirectionEvidence(const DirectionEvidence &evidence) {
  if (evidence.observations == 0) {
    return DirectionAssessmentStatus::Unobserved;
  }

  if (directionConflict(evidence)) {
    return DirectionAssessmentStatus::Conflicted;
  }

  if (evidence.observations < kMinimumDirectionEvidenceWindows) {
    return DirectionAssessmentStatus::Insufficient;
  }

  return evidence.progress > 0
    ? DirectionAssessmentStatus::ProgressEvidence
    : DirectionAssessmentStatus::RestrictionEvidence;
}

struct DirectionAssessment {
  SpatialCell cell {};
  int headingBin {};
  int occupiedHeadings {};
  int observations {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
  DirectionAssessmentStatus status { DirectionAssessmentStatus::Unobserved };
  bool otherHeadingOpposition {};
};

// Use only the requested sector for the assessment; other sectors are context, never votes.
inline bool assessDirectionalEvidence(
  const SpatialIndex &index,
  double x,
  double y,
  double z,
  double headingDegrees,
  DirectionAssessment &result
) {
  if (!std::isfinite(headingDegrees)) {
    return false;
  }

  MixedEvidenceAudit audit {};

  if (!auditMixedEvidence(index, x, y, z, audit)) {
    return false;
  }

  DirectionAssessment next {};
  next.cell = audit.cell;
  next.headingBin = directionBin(headingDegrees);
  next.occupiedHeadings = audit.occupiedHeadings;
  const DirectionEvidence &evidence = audit.headings[next.headingBin];
  next.observations = evidence.observations;
  next.progress = evidence.progress;
  next.blockedCandidates = evidence.blockedCandidates;
  next.deflectedCandidates = evidence.deflectedCandidates;
  next.status = classifyDirectionEvidence(evidence);

  const bool hasProgress = next.progress > 0;
  const bool hasRestriction = next.blockedCandidates > 0 || next.deflectedCandidates > 0;

  for (int bin = 0; bin < kDirectionBins; ++bin) {
    if (bin == next.headingBin) {
      continue;
    }

    const DirectionEvidence &other = audit.headings[bin];

    if ((hasProgress && (other.blockedCandidates > 0 || other.deflectedCandidates > 0))
      || (hasRestriction && other.progress > 0)) {
      next.otherHeadingOpposition = true;
      break;
    }
  }

  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
