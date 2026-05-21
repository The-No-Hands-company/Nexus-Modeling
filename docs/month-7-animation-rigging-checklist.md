# Month 7 Animation and Rigging Core Checklist

This checklist executes the Month 7 roadmap commitment in
[vision-and-roadmap.md](vision-and-roadmap.md).

Primary slice: Deterministic Playback and Retargeting Baseline.

## Scope Lock

In scope:

1. Skeleton, pose, and animation clip core contracts.
2. Skinning matrix packing helpers for future GPU upload wiring.
3. Name-based skeleton retargeting hooks.
4. Deterministic playback tests across frame rates and step sizes.
5. Scene binding application for animation output in headless-safe runs.

Out of scope:

1. Full authoring UI for timeline editing, dope sheets, or graph editors.
2. Advanced retargeting normalization beyond name-based mapping v0.
3. Animation compression and streaming codecs.

## Work Packages

## R7.1 Playback Determinism Baseline

Status: Complete

Delivered:

1. Added a deterministic playback regression to
   `tests/kernel/test_AnimationCore.cpp` that compares ClipStateMachine output
   across 60 Hz and 24 Hz tick streams.
2. Verified sampled pose output is stable when the same elapsed time is reached
   through different fixed-rate tick schedules.

Done when:

1. Identical elapsed time produces identical sampled poses across fixed-rate
   playback streams.

## R7.2 Rigging Core Contracts

Status: Complete

Delivered previously:

1. Public animation core contracts in `nexus/animation/AnimationCore.h`:
   - `TransformTrack`
   - `Skeleton`
   - `Pose`
   - `AnimationClip`
   - `ClipBlender`
   - `ClipStateMachine`
   - `AnimationSceneBinding`
   - `AnimationEvaluator`
   - `SkinningContract`
   - `AnimationStateGraph`
2. Name-based skeleton retargeting in `nexus/animation/SkeletonRetargeter.h`.

Delivered this increment:

1. Added `ClipStateMachineTransitionHonorsBoneMask` — verifies per-bone weight
   mask blocks blend on masked bones and blends unmasked bones correctly.
2. Added `ClipStateMachineActiveClipTimeContinuesAfterFadeCompletion` — verifies
   the promoted clip's elapsed time is not reset when the cross-fade completes.
3. Added `ClipStateMachineClampWrapModeSaturatesAtEnd` — verifies Clamp mode
   holds the final value after clip duration, regardless of additional ticks.
4. Added `SkinningContractJointMatrixReplayIsDeterministic` — verifies that two
   independent playback runs produce bit-identical packed joint matrices at the
   same elapsed-time checkpoints.
5. Added `StateGraphTransitionWithBoneMaskOnlyBlendsUnmaskedBones` — verifies
   state graph transition rules propagate bone-weight masks into the underlying
   ClipStateMachine blend path.

Done when:

1. Core rigging and animation contracts remain public, deterministic, and
   regression-tested.

## R7.3 Scene Application and Packaged Serialization

Status: Complete

Delivered previously:

1. Deterministic animation clip serialization in
   `tests/kernel/test_AnimationSerialization.cpp`.
2. Scene graph application tests for animation output.
3. Retargeting regression tests for mapped and unmapped bones.

Delivered this increment:

1. Added `RetargetThenSkinProducesExpectedJointMatrix` — integration test
   verifying that a retargeted pose produces the correct skinning joint matrix
   translation after applying an inverse bind offset.
2. Added `RetargetReplayProducesIdenticalPackedJointMatrices` — deterministic
   replay contract: two independent retarget + skin runs from the same source
   pose produce bit-identical packed joint matrix float arrays.

Done when:

1. Animation clips can be serialized, retargeted, and applied to scene nodes
   deterministically.

## Test Coverage

Implemented in:

1. `tests/kernel/test_AnimationCore.cpp`
   - transform track sampling and interpolation
   - skeleton hierarchy and pose evaluation
   - clip sampling, blending, and state machine transitions
   - skinning matrix packing
   - deterministic playback across frame-rate streams
   - per-bone mask enforcement during cross-fade transitions
   - active clip time continuity after fade completion
   - clamp wrap mode saturation at clip end
   - skinning joint matrix deterministic replay
   - state graph bone-mask propagation through transition rules
2. `tests/kernel/test_AnimationSerialization.cpp`
   - clip save/load round-trip behavior
   - serialization determinism checks
   - skeleton retargeting behavior
   - retarget → skin integration (joint matrix correctness)
   - retarget → skin deterministic replay (identical packed matrices)

## Month 7 Exit Gate (This Slice)

This Month 7 slice is complete only when all are true:

1. Skeleton, pose, clip, and retargeting contracts remain public and stable.
2. Playback output is deterministic across fixed-rate simulation streams.
3. Scene application and skinning helpers remain compatible with the animation
   core.
4. Tests cover the deterministic replay and retargeting baseline.
5. Build and test quality gates pass in the local workspace.

Status: COMPLETE