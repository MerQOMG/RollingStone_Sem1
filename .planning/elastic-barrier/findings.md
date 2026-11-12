# Findings

- 2D ElasticBarrier stores M/k/c, q/velocity, damping and numerical energy; current 3D barrier is a static BoxShape.
- Existing contact callbacks choose IsStatic, so a dynamic plate requires explicit object-ID material/metrics routing.
- Current Trial quiet criterion observes only rock: extend it to avoid ending while an elastic support still moves.
- Jolt SliderConstraint locks all rotations and two translations; its spring motor has special k=0 behavior, so evaluate zero-stiffness handling before adopting it.
- Old scene hashes use canonical schema=1 text. Adding defaults blindly changes old replay hashes; compatibility must be explicit.

- Chosen solver: Jolt dynamic finite-mass box with horizontal SliderConstraint, no gravity and no board/terrain collision (fixed-height anchored guide, as 2D). k>0 uses Jolt stiffness/damping position motor to zero; k=0 uses exact exponential damping before each collision substep, free when c=0. No positive epsilon stiffness substituted.
- Substep listener captures actual q/v, motor impulse and net axial contact impulse from M*delta_v - support_impulse. Damper energy is backward-Euler quadrature for k>0; signed support-work residual is reported separately, never mislabeled material dissipation or exact instantaneous contact peak.
- Full scene output includes new barrier settings; physical canonical text omits default rigid extension to retain old hashes. Missing mode in legacy schema=1 INI means rigid. UI exposes mode, M/k/c and initial q/v; defaults of legacy inputs remain unchanged.
- Acceptance: analytic damped oscillator, energy/impulse, yaw/guide drift, zero-k/c, high stiffness, finite extent, coupled termination, CSV/replay and actual moving display/picking.

## Current solver (supersedes initial motor selection)
- Separate Jolt soft motor failed extreme mass/stiffness/damping tests even with80 iterations. Production now directly eliminates implicit support equation into contact effective inverse mass: M_eff=M+h*c+h*h*k and free velocity=(M*v_old-h*k*q_old)/M_eff at each of4 existing substeps. Jolt solves contact impulses with this response; physical kinetic/spring energies use user M/k. This is the backward-Euler equation coupled to contact, not changing physical M to fit a result.
- All rotations and vertical DOF are disabled on board; a free SliderConstraint fixes lateral horizontal motion. No spring motor remains. Any q can be +/-; initialq/v optional; board not colliding terrain represents fixed-height support guide.
- Retain elastic-only80 velocity iterations (10 failed2.405mm hardest boundary), speculative margin.05 (old.02 first contact too late at50m/s), constraint warm-start=false (otherwise energy growth1.03854). Position iterations2 and dt=.002/4 unchanged. Lowering CCD threshold .25->.01 did not fix failure; restoring .25 is under test.
- State support_work_residual_J is signed diagnostic from discrete support impulse midpoint work minus spring change and damper quadrature; NOT material energy dissipation. Damper is h*c*v_end^2 for all k, including k0. Contact impulse inferred from actual effective solver mass and velocity change only when barrier contact callback occurred; force is substep average, not instantaneous peak.
- At present all27 range impacts passed with80 iterations/warmstart off/margin.05, maximum geometric depth .0002311m, maxenergy ratio1. More complete checks pending; no final commit yet.

- Frame instrumentation reproduced400-550ms stalls in BeginMode3D or EndMode3D batch flushing; modelcalls .002-.02ms, helpers .001ms, UI/present <1ms. Jolt continues, windowfocused/notminimized. Pinnedrlgl.h defaults1buffer and repeatedly glBufferSubData to sharedbuffer. Probe public rlLoadRenderBatch(3,8192) with RAII; no dependency patch, physics/pacing/vertices/budgets unchanged. Need actual proof.


## Final findings
- Restored CCD threshold .25 passed all numerical tests; no motor or glFinish remains. Final solver and diagnostics are documented in docs/3d/ELASTIC_BARRIER.md.
- Actual Debug GUI caught pinned raylib conflictingMAX9/7 build issue. Project target now fixes single9, unchanged dependencycommits/compiler; actualGUI teardown andCLlinechecks passed.
- Three-buffer intermediateperformance passed then failed; unusedcapture-sessionreset correlated with recovery but no proved GPUinternalcause. Finalreleasecomplete sixroundpassed on normalrenderpath; retain faileddata and distinguishmeasurement frominference.
- Firstpackagedeployment computed but directorycommitfailedWin32=5; noACLdenyobserved, externalholderunknown. Actualdirectory/childfilehandles reproduce32/5. Bounded4attempts10+20+40ms recovertransientlocks, sustainedrefusalremainsio_error andinvalidprobabilities. No permissionchanges, overwriteortrialretrychanges.
- Final12Release selftests, previousDebug12 pluschangedbatch/session2, GUI5matrix13CSVidentity, actualDebugGUI, legacy/newfullstatereplay, performance6round andordinarycmddeployment passed. Finalpackage has same13executablesasindependentselftestpackage. Sourcehead b82885c is pre-finalcommit; sourcebytehash6936044c... matchesactualmanifests.
