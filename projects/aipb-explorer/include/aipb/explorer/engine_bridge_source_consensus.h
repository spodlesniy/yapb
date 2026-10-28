#pragma once

#include <cmath>

#include <aipb/explorer/engine_bridge_trial_corroboration.h>

namespace aipb::explorer::bridge {

// The quality gate is descriptive: two windows from one trace are correlated.
inline constexpr int kMinimumSourceWindows = 2;
inline constexpr int kMinimumCorroboratingSources = 2;

struct SourceConsensus {
  SpatialCell cell {};
  int headingBin {};
  int sources {};
  int observedSources {};
  int weakSources {};
  int strongProgressSources {};
  int strongRestrictionSources {};
  int mixedSources {};
  int progressSources {};
  int restrictionSources {};
  int windows {};
  int progress {};
  int blockedCandidates {};
  int deflectedCandidates {};
  DirectionAssessmentStatus status { DirectionAssessmentStatus::Unobserved };
};

// A mixed trace is not two independent votes. Weak traces cannot confirm an
// otherwise homogeneous outcome, but opposing evidence still blocks agreement.
inline DirectionAssessmentStatus classifySourceConsensus(const SourceConsensus &evidence) {
  if (evidence.observedSources == 0) {
    return DirectionAssessmentStatus::Unobserved;
  }

  if (evidence.mixedSources > 0
    || (evidence.progressSources > 0 && evidence.restrictionSources > 0)) {
    return DirectionAssessmentStatus::Conflicted;
  }

  if (evidence.strongProgressSources >= kMinimumCorroboratingSources) {
    return DirectionAssessmentStatus::ProgressEvidence;
  }

  if (evidence.strongRestrictionSources >= kMinimumCorroboratingSources) {
    return DirectionAssessmentStatus::RestrictionEvidence;
  }

  return DirectionAssessmentStatus::Insufficient;
}

inline bool consolidateSourceEvidence(
  const EvidenceTrialSet &sources,
  double x,
  double y,
  double z,
  double heading,
  SourceConsensus &result
) {
  if (sources.trials.empty() || sources.trials.size() > kMaxTrialSources
    || !spatialCoordinate(x) || !spatialCoordinate(y) || !spatialCoordinate(z)
    || !std::isfinite(heading)) {
    return false;
  }

  SourceConsensus next {};
  next.cell = { spatialCell(x), spatialCell(y), spatialCell(z) };
  next.headingBin = directionBin(heading);
  next.sources = static_cast<int>(sources.trials.size());

  for (const EvidenceTrial &trial : sources.trials) {
    SpatialIndex index {};

    if (!buildSpatialIndex(trial.rows, index)) {
      return false;
    }

    DirectionAssessment assessment {};

    if (!assessDirectionalEvidence(index, x, y, z, heading, assessment)) {
      return false;
    }

    next.windows += assessment.observations;
    next.progress += assessment.progress;
    next.blockedCandidates += assessment.blockedCandidates;
    next.deflectedCandidates += assessment.deflectedCandidates;

    if (assessment.observations == 0) {
      continue;
    }

    ++next.observedSources;

    const bool progress = assessment.progress > 0;
    const bool restriction = assessment.blockedCandidates > 0
      || assessment.deflectedCandidates > 0;

    if (progress) {
      ++next.progressSources;
    }

    if (restriction) {
      ++next.restrictionSources;
    }

    if (progress && restriction) {
      ++next.mixedSources;
    }
    else if (assessment.observations < kMinimumSourceWindows) {
      ++next.weakSources;
    }
    else if (progress) {
      ++next.strongProgressSources;
    }
    else {
      ++next.strongRestrictionSources;
    }
  }

  next.status = classifySourceConsensus(next);
  result = next;
  return true;
}

} // namespace aipb::explorer::bridge
