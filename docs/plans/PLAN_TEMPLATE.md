# PLAN: <concise-title>

APPROVED: <name> <YYYY-MM-DD>

## 1) Scope
- What changes and why (one-liner)
- What *won't* change (non-goals)

## 2) Context
- Links to related tickets/bench results/PRs
- User impact and host matrix (DAWs, sample rates, buffers)

## 3) Risks & Mitigations
- RT safety risks (allocations, locks, denorms)
- DSP risks (stability, morph continuity, saturation)
- UI/UX risks (threading, repaint, allocations)
- Packaging risks (signing, VST3 metadata)

## 4) Acceptance Criteria
- Functional: <what should happen>
- Audio quality: thresholds below (mag/phase/THD, zipper noise)
- Performance: processBlock avg/max µs under targets
- Compat: host matrix instantiation + param automation

## 5) Tests & Bench Deltas
- Unit tests to add/update
- Offline renders: impulse, sweep, multitone at 44.1/48/96k
- Fixture policy: new or re-bless? (include reason)

## 6) Bench Thresholds
- Magnitude Δ ≤ 0.3 dB avg, ≤ 0.8 dB max
- Phase Δ ≤ 8° avg, ≤ 25° max
- THD Δ ≤ 0.2 dB; DC ≤ −80 dBFS
- Zipper at morph edges ≤ 0.3 dB peak

## 7) Rollback Strategy
- Files to revert, feature flags to disable, config toggles

## 8) Change Plan (High-Level Steps)
1. ...
2. ...
3. ...

## 9) Affected Codepaths
- src/dsp/...
- src/plugin/...
- assets/zplane/banks/...

## 10) Open Questions
- ...

## 11) Sign-Off
- Author: <name>
- Reviewer: <name>
- Approval keyword: `APPROVED: <name> <date>`

## 12) Changelog Note
- Short line for release notes