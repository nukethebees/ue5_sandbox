# Runtime Flight Model System

Status: implementation in progress; Milestones 1-5 are complete and Unreal integration is next.

This file is both the implementation specification and the persistent progress record for the
player flight-model redesign. Update the progress ledger and any decisions changed by repository
evidence as work advances.

## Goal and boundaries

Replace the overlapping `SpaceShipFlightMode`/`SpaceShipControlMode` movement paths with one
runtime-configurable native flight-model system. The selected model interprets a common player
intent vocabulary; Enhanced Input remains responsible only for hardware/control schemes. Physical
state, flight-controller state, resource state, and presentation state remain conceptually and
structurally distinct.

The first usable system supplies four directly selectable profiles: Starfox, Fighter, Skater, and
Gunship. Profiles are ordinary data produced by native factory functions, can be copied into four
runtime slots, and can be edited without reconstructing the simulation. The design must support
unusual future combinations without virtual model subclasses, template policy machinery, or
special cases for the initial four profiles.

Weapons retain their current behavior. AI flight, networking, prediction, alternate weapons,
combat balancing, optimization, and a general preset asset ecosystem are out of scope.

## Current-state architecture

The current command path is:

1. `IMC_SpaceShip_Base` and its Enhanced Input profile overrides produce action values.
2. `FShipControlContext` binds those actions and calls `ATestSpaceShip` methods.
3. `ATestSpaceShip` converts Unreal values and forwards through generated
   `ioj::sim::player::CommandInterface` methods.
4. `ioj::sim::player::Sim` stores input fields and updates movement during its normal phased tick.
5. `PlayerReadView` feeds Unreal presentation and HUD code.

Important current behavior and debt:

- `SpaceShipFlightMode` selects a storage/integration shape (`ForwardSpeed` or `PlanarVelocity`),
  not a complete model.
- `SpaceShipControlMode` selects `Velocity` or `Power`; selecting `Power` forcibly selects
  `PlanarVelocity`, so the two enums are not independent concepts.
- `MovementState` mixes physical transform/velocity, visual body transform, thrust energy,
  response-controller state, target values, boost/brake state, and a presentation event sequence.
- The power controller is a target-velocity controller. It calls `move_towards` from current world
  velocity toward `ship_forward * maximum_speed`; it is not additive Skater thrust because it
  removes orthogonal velocity while accelerating.
- The old forward path uses one scalar `ShipFlightModel`; planar velocity uses one vector
  `ShipFlightModel` plus a second scalar forward-boost response.
- `ShipFlightModel` restarts an underdamped step curve from an old value toward a target. Its state
  is old value, target, elapsed time, and `DampedStepResponse` coefficients. The mathematical
  implementation assumes positive settling time and damping below one.
- Physical pitch/yaw/roll update the root transform. A separate `body_transform` supplies visual
  pitch/yaw/bank, but laser sockets and lock-on traces currently include that transform. Moving it
  wholly into Unreal presentation would therefore alter weapon behavior.
- `FShipControlContext` branches on `Power` to decide whether throttle means analog acceleration
  or immediate boost and whether double-tap brake means emergency brake. This is a physics leak
  into the hardware adapter.
- The repository already has one canonical base ship IMC and a broad action vocabulary. Control
  profiles are Enhanced Input overrides; a model change does not need an IMC change.
- Game settings expose only a three-value flight-control preset. `ASpaceGamePlayerController`
  reapplies it from the broad `settings_changed` delegate, including changes unrelated to flight.
- The runtime level config uses `FPlayerShipConfig`, converted to `PlayerSimConfig`. A separate
  legacy `UTestSpaceShipData`/`USimulationConfig` bundle exists but does not feed the current native
  player path and should not drive this redesign.
- The current workspace has pre-existing changes to
  `Content/Levels/FeatureTests/FT_soa_turrets/BP_TestSpaceShip.uasset` and `Sandbox.uproject`.
  Preserve them and do not overwrite asset state blindly.

## Target data model

### Identifiers and loadout

Add native strong enums:

```cpp
enum class FlightModelPreset : std::uint8_t { Starfox, Fighter, Skater, Gunship };
enum class FlightModelSlot : std::uint8_t { Up, Right, Down, Left };
```

Use owned runtime profiles rather than references plus override layers:

```cpp
struct FlightModelProfile {
    FlightModelPreset base_preset{FlightModelPreset::Starfox};
    bool customized{};
    FlightModelConfig config{};
};

struct FlightModelLoadout {
    FlightModelProfile up{};
    FlightModelProfile right{};
    FlightModelProfile down{};
    FlightModelProfile left{};
    FlightModelSlot initial_slot{FlightModelSlot::Up};
};
```

`player::Sim` owns its loadout and active slot. The active slot's config is the sole native runtime
source of truth. Selecting a named preset replaces a slot's complete config and clears
`customized`; changing a field edits that slot's copy and marks it custom. Native factory functions
return the four defaults and the default Up/Right/Down/Left loadout.

Initial hard-coded assignment:

- Up: Starfox
- Right: Fighter
- Down: Skater
- Left: Gunship

### Configuration shape

Configuration is hybrid: common per-axis types, grouped under explicit forward/right/up and
pitch/yaw/roll members. Explicit names improve UI, diagnostics, and authoring while avoiding three
copies of each type.

Core enums:

```cpp
enum class TranslationSemantic : std::uint8_t {
    Disabled,
    TargetSpeed,
    TargetVelocity,
    Acceleration,
};

enum class RotationSemantic : std::uint8_t {
    Disabled,
    TargetAngularVelocity,
    AngularAcceleration,
};

enum class ReferenceFrame : std::uint8_t { Ship, World };
enum class ResponseMode : std::uint8_t { Direct, RateLimited, SecondOrder };
enum class FacingVelocityCoupling : std::uint8_t {
    Independent,
    AlignToFacing,
    LockedToFacing,
};
```

Camera, velocity, and target frames are intentionally deferred. Adding enum values later is
straightforward; unused values must not be added before the simulation can supply their reference
transforms.

Response data:

```cpp
struct RateLimitedResponseConfig {
    float increasing_rate{};
    float decreasing_rate{};
};

struct SecondOrderResponseConfig {
    float settling_time{3.f};
    float damping_ratio{0.5f};
};

struct ResponseConfig {
    ResponseMode mode{ResponseMode::Direct};
    RateLimitedResponseConfig rate_limited{};
    SecondOrderResponseConfig second_order{};
};
```

Translation data:

```cpp
inline constexpr float effectively_unlimited_speed{std::numeric_limits<float>::max()};

struct TranslationChannelConfig {
    TranslationSemantic semantic{TranslationSemantic::Disabled};
    ReferenceFrame reference_frame{ReferenceFrame::Ship};
    float automatic_value{};
    ResponseConfig response{};
};

struct TranslationDriveConfig {
    float positive_speed_limit{effectively_unlimited_speed};
    float negative_speed_limit{effectively_unlimited_speed};
    float positive_acceleration{};
    float negative_acceleration{};
};

struct TranslationAxisConfig {
    TranslationChannelConfig manual{};
    TranslationChannelConfig automatic{};
    TranslationDriveConfig normal{};
    TranslationDriveConfig boosted{};
    float passive_drag{};
    float active_stabilization_rate{};
};

struct TranslationAxesConfig {
    TranslationAxisConfig forward{};
    TranslationAxisConfig right{};
    TranslationAxisConfig up{};
};
```

Separate manual and automatic channels allow either or both on each axis. `TargetSpeed` consumes a
persistent target adjusted by explicit target-speed commands; `TargetVelocity` consumes the live
axis value; `Acceleration` adds thrust. Automatic channels supply their configured value without
player input. Contributions are evaluated separately: target controllers establish controlled
velocity, then acceleration contributions are added.

Rotation data:

```cpp
struct RotationStabilizationConfig {
    bool enabled{};
    ReferenceFrame reference_frame{ReferenceFrame::World};
    float target_angle{};
    float delay{};
    ResponseConfig response{};
};

struct RotationAxisConfig {
    RotationSemantic manual_semantic{RotationSemantic::Disabled};
    float maximum_rate{};
    float acceleration{};
    ResponseConfig response{};
    RotationStabilizationConfig stabilization{};
};

struct RotationAxesConfig {
    RotationAxisConfig pitch{};
    RotationAxisConfig yaw{};
    RotationAxisConfig roll{};
};
```

Manual rotation and automatic stabilization are orthogonal, allowing manual roll plus auto-level
or auto-level without manual roll.

Model-wide data:

```cpp
struct FacingVelocityConfig {
    FacingVelocityCoupling mode{FacingVelocityCoupling::Independent};
    float alignment_rate{};
    ResponseConfig response{};
};

struct BoostConfig {
    bool available{};
    float energy_drain_per_second{};
};

struct BrakeConfig {
    bool available{};
    float deceleration{};
    float energy_drain_per_second{};
    ResponseConfig response{};
};

struct FlightModelConfig {
    TranslationAxesConfig translation{};
    RotationAxesConfig rotation{};
    FacingVelocityConfig facing_velocity{};
    BoostConfig boost{};
    BrakeConfig brake{};
    BrakeConfig emergency_brake{};
    float energy_recharge_per_second{};
    float maximum_resultant_speed{effectively_unlimited_speed};
    float boosted_maximum_resultant_speed{effectively_unlimited_speed};
};
```

Normal and boosted axis limits/rates are explicit values. Never derive a boosted limit by
multiplying `effectively_unlimited_speed`. World velocity remains double precision, so magnitude
calculations with a maximum float remain finite; validate this with tests.

Provide `validate_flight_model_config` returning actionable errors. Reject invalid runtime edits
without replacing the previous config. Validate finite/non-negative limits and rates, positive
second-order settling time, and the damping interval supported by `DampedStepResponse`.

### State separation

Replace the current mixed `MovementState` with separate state:

```cpp
struct PhysicalMovementState {
    Transform3d transform{};
    ml::Vector3d velocity{};
};

struct PlayerFlightIntent {
    ml::Vector3d translation{}; // forward, right, up, each normalized
    ml::Vector3d rotation{};    // pitch, yaw, roll, each normalized
    float target_speed_adjustment{};
    bool boost{};
    bool brake{};
    bool emergency_brake{};
};
```

`FlightModelRuntimeState` owns persistent target speeds, per-channel scalar response controllers,
active/effective action state, and active slot. `PlayerResourceState` owns thrust energy.
`PlayerPresentationState` owns body transform and boost presentation sequences.

Keep current/planned copies for every tick-mutated state needed by the phased simulation. A command
that changes models must update the authoritative state and synchronize any planned state that
could otherwise overwrite the transition during `apply_movement`.

### Response runtime

Keep `DampedStepResponse` and retain `ShipFlightModel<float>` until equivalent behavior is covered.
Add a concrete scalar response runtime rather than a template policy framework. It stores the
current output and dispatches at runtime:

- Direct: immediately returns the target.
- RateLimited: moves toward the target using increasing/decreasing rates.
- SecondOrder: uses the existing damped step curve and restarts an impulse only when the target
  changes.

For target semantics the response output is controlled velocity/speed. For acceleration semantics
it is commanded acceleration, allowing thrust smoothing without turning Skater into a target-
velocity controller.

Migrate old behavior as follows:

- Forward scalar response -> Starfox automatic forward channel, normally SecondOrder.
- Planar vector response -> independent scalar target-velocity responses on the relevant axes.
- Planar forward boost response -> the forward axis's boosted drive/response, not a parallel hidden
  velocity component.
- Existing power `move_towards` behavior -> `TargetVelocity + RateLimited`, useful for Gunship or
  experiments, never the implementation of additive Skater thrust.

## Integration semantics

The generic movement tick order is:

1. Snapshot active config and live intent.
2. Update physical pitch/yaw/roll.
3. Evaluate automatic and manual translation channels.
4. Convert ship/world contributions into world space.
5. Apply target-speed/target-velocity response controllers.
6. Integrate acceleration contributions.
7. Apply configured facing/velocity coupling.
8. Apply passive drag or explicit active stabilization.
9. Resolve emergency brake, brake, boost, then normal drive.
10. Enforce per-axis and resultant speed limits.
11. Integrate position.
12. Update separately stored presentation state.

Emergency brake has priority over brake, which has priority over boost when inputs overlap. The
intent fields remain independent; brake is not represented as negative throttle.

### Presets

Starfox:

- automatic forward target velocity; right/up disabled;
- manual pitch/yaw; roll disabled or auto-levelled;
- locked or strongly aligned facing/velocity coupling;
- SecondOrder response for normal cruise, boost, and brake transitions;
- seed initial numbers from current cruise/boost/brake and `SpeedResponses` data.

Fighter:

- manual ship-forward acceleration;
- configurable passive drag after release;
- manual pitch/yaw;
- strong but independently tunable facing/velocity coupling;
- boost selects explicit higher acceleration and speed limits;
- initial values may be seeded from the current power rates, but drag is a tuning value, not baked
  behavior.

Skater:

- manual ship-forward acceleration integrated additively into world velocity;
- independent facing and velocity;
- no passive drag or neutral stabilization;
- Direct acceleration response by default;
- turning never rotates existing velocity;
- brake/emergency brake move the current world vector toward zero without reversal;
- boost selects stronger local-forward acceleration and an explicit higher resultant limit.

Gunship:

- manual ship-frame target velocity for forward/right/up;
- RateLimited response implements active counter-thrust and neutral position-hold behavior;
- no passive drag is required;
- manual pitch/yaw; roll initially disabled or auto-levelled;
- independent facing and translation velocity.

### Model switching

`select_flight_model_slot` performs an atomic controller transition:

- Preserve physical transform, physical orientation, world velocity, thrust energy, health, weapon
  state, and current presentation body offset.
- Preserve live raw translation/rotation and held action intent because it still describes what the
  player is doing.
- Clear persistent target-speed sampling/edits, effective boost/brake state, and all response
  histories from the previous model.
- Seed new velocity response states from current world velocity projected into each new channel's
  reference frame.
- Recompute effective boost/brake state from live held intent under the new config.
- Do not alter velocity inside the selection command. Locked/strong facing coupling starts on the
  following simulation tick as ordinary behavior of the selected model.
- Reset Unreal gesture history on every slot selection so taps cannot span models.

Tests must distinguish preserved raw intent from discarded derived controller state.

## Input and Unreal adapter

Keep one superset gameplay mapping context plus the existing global mapping. Normalize native
commands around intent rather than algorithm-specific operations:

- forward/right/up translation setters;
- pitch/yaw/roll rotation setters;
- held boost, brake, and emergency-brake setters;
- target-speed adjustment commands for models that use them;
- direct `select_flight_model_slot`;
- existing weapon commands unchanged.

Throttle always publishes normalized forward intent. Model-independent double-tap recognition may
publish boost or emergency-brake intent, but `FShipControlContext` must not query the current flight
model. A model ignores unavailable inputs.

Add four Boolean Enhanced Input actions and `FSpaceShipControllerInputs` fields. Bind D-pad actions
on `ETriggerEvent::Started`:

```text
D-pad action
  -> FShipControlContext::select_flight_model_slot
  -> ATestSpaceShip
  -> generated CommandInterface
  -> player::Sim::select_flight_model_slot
```

Repeated held input does not cycle slots, and no mapping context changes when the physics model
changes.

## Configuration, settings, and UI

`PlayerSimConfig` retains ship-wide native simulation data: weapon/laser behavior, thrust-energy
capacity, and other non-model resources. `FlightModelConfig` owns all movement interpretation,
rates, limiting, response, coupling, boost/brake policy, and recharge/consumption policy.

Split Unreal-side configuration conceptually:

- simulation/flight settings converted to native data;
- presentation settings such as engine visuals and visual body tilt/banking;
- combat/weapon settings unchanged.

Physical roll auto-level belongs in the flight model. Visual pitch/yaw/bank belongs in presentation
configuration. Initially retain native `PlayerPresentationState` and current socket composition so
lasers do not change; fully removing visual body offsets from weapon socket calculation is deferred
until weapon-aim semantics are explicitly designed.

First implementation UI/settings behavior:

- Persist the selected named default preset using the existing user-settings path.
- Migrate existing stored `ForwardSpeed`, `PlanarVelocity`, and `PlanarPower` values explicitly; do
  not reinterpret old integer values silently.
- Keep initial D-pad assignments hard-coded.
- Add a dedicated flight-model section to the normal controls page using existing settings row
  widgets.
- Expose the actual active configuration fields with mode-appropriate visibility/labels.
- Apply valid edits immediately to the active native slot and display `Custom (based on X)`.
- Keep custom numeric edits session-scoped for the first implementation.
- Replace the broad settings callback with targeted flight-setting synchronization so changing an
  unrelated setting cannot reset the active model.

Do not flatten the complete nested config into the generated scalar `FGameSettingsState`. Use a
small structured flight-model edit state owned by `UGameSettingsSubsystem`/the controls view while
retaining generated settings for the saved named default preset.

HUD should replace separate `CONTROL` and `FLIGHT MODE` labels with one active profile/model label.
Effective boost/brake status remains independently displayable.

## Concrete work areas

Native simulation:

- Add flight config, preset/loadout, validation, intent, scalar response, and runtime-state headers
  and implementations under `native/simulation/include/ioj/sim/player/` and
  `native/simulation/src/player/`.
- Refactor `player/sim.h` and `player/sim.cpp` around the generic evaluator and explicit transition.
- Split model fields out of `sim_config.h`; update reference fixture/export data and CMake sources.
- Update `lispb/schema/facades.lispb` for intent/config/slot commands. Remove old mode enum schema
  only at the cleanup milestone.
- Retain `step_response.*` and `ship_flight_model.*` until the scalar runtime provides tested
  equivalent SecondOrder behavior.

Unreal integration:

- Update `SpaceShipControllerInputs`, `ShipControlContext`, `TestSpaceShip`, and
  `SpaceGamePlayerController` for canonical intent and four direct selectors.
- Update `LevelActorSettings`/simulation config conversion only as needed to separate generic ship,
  flight, and presentation data.
- Add the four input actions, map D-pad keys in `IMC_SpaceShip_Base`, and wire both the canonical
  runtime controller Blueprint and authored feature-test controller without overwriting unrelated
  asset changes.
- Update HUD manager/widget data from the two legacy enum labels to the active profile label.

Settings/UI:

- Update the settings enum/schema/backend/user-settings validation and migration for four named
  presets.
- Add structured session edit state and a flight-model editor section in `SGameOptionsView`.
- Reuse existing toggle, choice, and slider widgets; conditionally omit fields that have no meaning
  for the selected semantics.

Tests:

- Replace `test_player_power_control.cpp` with behavior-oriented flight-model coverage or migrate
  its valuable cases into `test_player_flight_models.cpp`.
- Retain `step_response_tests.cpp`; add scalar response tests only for observable mode behavior.
- Adapt planar movement and scenario tests to named model setup.
- Update Unreal control-context, user-settings, settings edit-state, simulation-config, and player
  input smoke tests.

## Test plan

Native behavior must cover:

- all four presets are constructible, valid, assigned to direct slots, and selectable;
- runtime config replacement/editing takes effect without simulation reconstruction;
- switching preserves transform, orientation, world velocity, energy, health, and weapon state at
  the command boundary;
- switching clears derived targets/effective actions/response history and seeds the new response
  state from current velocity;
- Starfox converges to cruise, follows facing, brakes, and boosts with SecondOrder available;
- Fighter gains speed only under acceleration, loses it according to configured drag, and supports
  brake/boost;
- Skater coasts while rotating, adds local-forward velocity, progressively bends a trajectory, can
  fly backward relative to facing, and preserves perpendicular velocity under perpendicular thrust;
- Skater brake/emergency brake reduce the complete world vector without reversal;
- Gunship accepts three translation axes and actively converges to zero on neutral input while
  pitch/yaw remain usable;
- passive drag and active stabilization are observably distinct;
- Direct and RateLimited paths bypass SecondOrder; SecondOrder remains selectable per channel;
- finite-unlimited settings do not create NaN/infinity in normal gameplay ranges;
- action conflict precedence and energy exhaustion/fallback rules are explicit;
- existing laser firing behavior and projectile base velocity remain unchanged.

Unreal behavior must cover:

- canonical input/action references and the base IMC remain complete and player-mappable;
- four D-pad mappings select exact slots on `Started`, without cycling or IMC changes;
- gesture state cannot cross a model transition;
- unbind/modal transitions neutralize all intent and stop weapon fire;
- control profiles remain hardware mappings only;
- preset settings migrate, validate, reset, apply, and round-trip;
- session edits mark `Custom (based on X)` and valid edits reach the native simulation;
- input smoke coverage exercises direct selection and unchanged fire behavior.

Retain the existing power tests' useful guarantees, but change the old target-vector redirection
expectation to additive component expectations for Skater.

## Validation

Use repository-coordinated workflows and the smallest useful gate at each stage:

```powershell
cmake --build --preset native --target native-simulation-tests
ctest --preset native-simulation-tests
cmake --workflow --preset format-code
cmake --workflow --preset debug-game-unit-tests
cmake --workflow --preset debug-game-tests
```

Run `native-simulation-tests` during native milestones. Run Unreal unit tests after adapter,
settings, or UI changes. Run the broader debug-game feature/smoke gate once against the completed
Unreal-facing candidate. Code generation output changes do not by themselves require `tool-tests`
unless generator/tool implementation is changed.

## Cleanup/deletion criteria

Delete legacy paths only when all presets use the generic evaluator, settings/assets no longer
serialize the old enums, and replacement behavioral tests pass. Then remove:

- `SpaceShipFlightMode`, `SpaceShipControlMode`, their generated Unreal projections/conversions,
  and enum tests;
- `uses_power_controller`, `integrate_power_velocity`, control-mode cycling, and legacy mode setter
  commands;
- `planar_velocity`, `planar_boost_speed`, legacy target fields, and the three response objects from
  `MovementState`;
- old split movement tuning fields after every value is represented in preset/config data;
- separate control/flight HUD labels;
- unused manual bank settings if repository-wide search still proves they have no behavior.

Retain `DampedStepResponse`. Rename or reduce `ShipFlightModel` only after SecondOrder behavior is
represented and covered. Do not delete old standalone IMC/profile assets unless reference and
profile tests prove they are obsolete independently of this redesign.

## Risks and rollback points

- Power-to-Skater semantic mismatch: never preserve target-vector convergence in the Skater preset.
- One-tick state rollback: transition code must synchronize current/planned controller and movement
  state.
- Reference-frame projection: seed responses from current world velocity using the newly selected
  model's frame.
- Weapon regression: body banking currently changes sockets and lock-on traces; preserve composition
  in this work.
- Persisted setting compatibility: version and migrate old integers before replacing enum members.
- Input regressions: held axes/buttons and gesture history require different transition handling.
- Extreme limit arithmetic: keep vector math in double precision and avoid multiplying max-float
  limits.
- Asset conflicts: preserve the existing modified Blueprint/project state and make explicit,
  reviewed asset edits.
- Rotation feel: current yaw magnitude is effectively squared; characterize it before preserving or
  deliberately changing it.

Rollback after each milestone should restore the previous runtime path without reverting data/type
preparation from earlier passing commits. Keep the old evaluator callable until Starfox/Fighter,
Skater, Gunship, transitions, and Unreal command routing have their replacement tests.

## Explicitly deferred

- alternate, turreted, or independently aimed weapons;
- changing whether lasers follow visual banking;
- custom preset disk persistence, naming, import/export, and asset/config formats;
- user-editable D-pad slot assignment;
- camera-, velocity-, and target-relative reference frames;
- advanced automatic roll and target-facing controllers not needed by the initial presets;
- AI/capital/fighter flight conversion;
- networking, rollback, prediction, replay work, save-game architecture, SIMD, and optimization;
- broad UI redesign outside the flight-model editor.

## Ordered implementation milestones

1. **Characterize and scaffold**
   - Add/adjust tests that lock down current SecondOrder, transition, rotation, and laser/body socket
     behavior.
   - Add runtime config/enums, validation, preset/loadout factories, and CMake wiring without
     changing active movement.
   - Completion: native simulation builds; factory/validation tests pass; old behavior remains.

2. **Response and state separation**
   - Add scalar response runtime and split physical, intent, controller, resource, and presentation
     state while retaining the legacy evaluator behind an internal path.
   - Completion: existing native player tests pass with no externally visible behavior change.

3. **Generic evaluator and Starfox**
   - Implement the explicit movement pipeline and express forward cruise/boost/brake through the
     generic config.
   - Completion: Starfox behavior replaces the old ForwardSpeed path with equivalent tests.

4. **Fighter and Skater**
   - Add drag/coupling Fighter behavior and genuinely additive inertial Skater behavior.
   - Migrate power energy/brake tests and correct the old target-vector expectation.
   - Completion: both presets pass their native behavioral matrix without legacy power dispatch.

5. **Gunship and transitions**
   - Add three-axis target-velocity control, neutral stabilization, direct slot selection, response
     seeding, and current/planned synchronization.
   - Completion: all four presets and every pairwise transition invariant pass natively.

6. **Canonical Unreal intent and D-pad**
   - Remove model branching from `FShipControlContext`, add held intent commands, four selectors,
     actions/mappings, adapters, and asset wiring.
   - Completion: Unreal unit tests and input smoke tests prove direct selection, gesture reset,
     superset IMC behavior, and unchanged fire input.

7. **Settings, HUD, and runtime editor**
   - Migrate saved preset selection, add targeted synchronization, replace HUD labels, and add the
     session-scoped structured editor with conditional rows.
   - Completion: named preset persistence and runtime custom editing tests pass; unrelated settings
     no longer reselect a model.

8. **Legacy removal and final cleanup**
   - Remove obsolete enums, fields, commands, evaluator branches, generated projections, and unused
     tuning fields after repository-wide reference checks.
   - Format and review the effective patch.
   - Completion: focused native tests, Unreal unit tests, and final debug-game smoke/feature tests
     pass; no old runtime decision path or duplicate source of truth remains.

## Progress ledger

- [x] Read-only architecture audit completed.
- [x] Target architecture and migration decisions recorded in this file.
- [x] Milestone 1: characterize and scaffold (runtime config, four preset factories, loadout,
  validation, and focused tests pass; existing response/power tests retain behavioral coverage).
- [x] Milestone 2: response and state separation (runtime Direct/RateLimited/SecondOrder scalar
  response added; physical, controller, resource, and presentation state split with legacy
  behavior retained and focused native tests passing).
- [x] Milestone 3: generic evaluator and Starfox (the generic runtime pipeline now drives cruise,
  boost, brake, facing coupling, rotation, limiting, and position integration; focused native tests
  cover Starfox convergence and actions).
- [x] Milestone 4: Fighter and Skater (Fighter acceleration/drag and genuinely additive inertial
  Skater movement are expressed entirely through runtime data and pass behavioral tests).
- [x] Milestone 5: Gunship and transitions (three-axis target velocity, neutral stabilization,
  direct slots, runtime profile replacement, action precedence, pairwise velocity preservation,
  and response reseeding pass the native simulation suite).
- [ ] Milestone 6: canonical Unreal intent and D-pad.
- [ ] Milestone 7: settings, HUD, and runtime editor.
- [ ] Milestone 8: legacy removal and final cleanup.
- [ ] Completion audit against every requirement in this plan.
