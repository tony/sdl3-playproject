# Game Mechanic Specification Template v2.0 (Frame-Accurate Universal)

## Purpose

Specify mechanics so precisely that two independent implementations produce **identical frame-by-frame behavior** at 60 Hz. Primary units are **frames** and **px/frame** (or subpx/frame).

This spec maps cleanly to TOML data and automated test vectors.

---

# MECHANIC: [Name]

## 0. Engine Contract (REQUIRED)

These rules define what "Frame 0" means and prevent silent ambiguity.

### 0.1 Tick / Step

| Parameter | Value |
|-----------|-------|
| `tick_rate_hz` | 60 |
| `fixed_timestep` | true |
| `sim_step_when_slow` | [ ] drop_frames  [ ] run_multiple_sim_steps |
| `input_sampling` | [ ] once_per_sim_tick  [ ] event_accumulate_then_latch |

### 0.2 Numeric Representation

| Parameter | Value |
|-----------|-------|
| `position_format` | [ ] fixed Q___.___  [ ] int_subpixel(scale=___)  [ ] float |
| `velocity_format` | [ ] fixed Q___.___  [ ] int_subpixel(scale=___) |
| `accel_format` | [ ] fixed Q___.___  [ ] int_subpixel(scale=___) |
| `rounding_mode` | [ ] trunc_toward_zero  [ ] floor  [ ] round_nearest |
| `integration` | pos += vel; vel += accel (or specify) |
| `quantization_points` | (e.g., after integration, after collision, before render) |

### 0.3 Direction Normalization

| Parameter | Value |
|-----------|-------|
| `digital_from_analog` | deadzone, snap-to-8way rules |
| `diagonal_policy` | how diagonals are produced/recognized |
| `socd_resolution` | [ ] neutral  [ ] last_input_wins  [ ] prefer_left  [ ] prefer_right |

### 0.4 Frame Pipeline (Authoritative Order)

Mechanics must declare WHERE they evaluate. Default pipeline:

```
1) InputLatch              - raw -> logical buttons/dirs
2) DerivedInputUpdate      - pressed/released/ages/history
3) PrePhysicsStateUpdate   - timers, queued transitions
4) TriggerEval:PrePhysics  - <-- HOOK POINT
5) PhysicsIntegrate
6) CollisionResolve        - axis order + snapping rules
7) ContactFlagsUpdate      - grounded/wall/ceiling/platform
8) TriggerEval:PostCollision - <-- HOOK POINT
9) PostStateUpdate         - enter/exit state events
10) AnimationStep
11) Hitbox/HurtboxUpdate
12) TriggerEval:PostAnim   - <-- HOOK POINT (rare)
```

---

## 1. Identity

| Field | Value |
|-------|-------|
| `name` | |
| `source_games` | |
| `characters` | |
| `category` | [ ] Movement  [ ] Attack  [ ] Defense  [ ] Special  [ ] Cancel  [ ] System |
| `complexity` | [ ] Basic  [ ] Intermediate  [ ] Advanced  [ ] Expert |

---

## 2. State/Signal Dictionary

Define exactly what each prerequisite means in terms of probes or rules.

| Signal | Definition |
|--------|------------|
| `grounded` | true iff foot probes contact solid OR snap_tolerance <= N |
| `wall_contact_left` | true iff side probes overlap solid OR distance_to_wall <= N |
| `wall_contact_right` | true iff side probes overlap solid OR distance_to_wall <= N |
| `wall_grip` | state entered by rules (not just collision) |
| `near_wall_left` | distance_to_wall_left <= N pixels (spatial leniency) |
| `near_wall_right` | distance_to_wall_right <= N pixels |
| ... | |

### Probes/Sensors

| Probe | Position | Size | Purpose |
|-------|----------|------|---------|
| `foot_probe` | bottom-center | Wpx x Hpx | ground detection |
| `wall_probe_left` | left-center | Wpx x Hpx | left wall detection |
| `wall_probe_right` | right-center | Wpx x Hpx | right wall detection |
| ... | | | |

---

## 3. Input Model (REQUIRED)

### 3.1 Per-Button/Direction Signals

For each logical button/direction, these signals are available:

| Signal | Meaning |
|--------|---------|
| `down` | currently held |
| `pressed_this_frame` | rising edge (was up, now down) |
| `released_this_frame` | falling edge (was down, now up) |
| `held_frames` | consecutive frames held |
| `frames_since_pressed` | frames since last rising edge |
| `frames_since_released` | frames since last falling edge |

### 3.2 Input Predicates (Standard Vocabulary)

Use these in trigger equations:

| Predicate | Meaning |
|-----------|---------|
| `Down(X)` | X is currently held |
| `Press(X)` | X rising edge this frame |
| `Release(X)` | X falling edge this frame |
| `HeldFor(X, N)` | held_frames >= N |
| `PressedWithin(X, N)` | frames_since_pressed <= N |
| `ReleasedWithin(X, N)` | frames_since_released <= N |
| `DirIs(D)` | current direction == D |
| `DirWasWithin(D, N)` | D appeared in dir_history within N frames |

### 3.3 Compound Timing Semantics

| Parameter | Value |
|-----------|-------|
| `anchor_event` | The event that defines "Frame 0" (e.g., Press(Jump)) |
| `simultaneous_tolerance` | N frames (inputs within N frames of anchor count as "same time") |
| `pre_buffer_frames` | Input accepted up to N frames BEFORE valid state |
| `post_grace_frames` | Input accepted up to N frames AFTER state ends (coyote time) |

### 3.4 Optional Modules

#### MotionParser (Fighters)

| Parameter | Value |
|-----------|-------|
| `enabled` | [ ] yes  [ ] no |
| `buffer_depth_frames` | |
| `max_gap_between_steps` | |
| `direction_zone_leniency` | (angles or discrete equivalencies) |
| `shortcut_patterns` | list of allowed shortcuts |
| `negative_edge_allowed` | [ ] yes  [ ] no (Release(P) counts as press) |

#### ChargeMove (Fighters)

| Parameter | Value |
|-----------|-------|
| `enabled` | [ ] yes  [ ] no |
| `charge_dirs` | list of valid charge directions |
| `required_frames` | |
| `partition_grace_frames` | |
| `broken_by` | [ ] neutral  [ ] opposite  [ ] hitstun  [ ] jump  [ ] other |
| `persists_through` | list of states |

---

## 4. Activation

### 4.1 Evaluation Hook (REQUIRED)

| Parameter | Value |
|-----------|-------|
| `trigger_eval_hook` | [ ] PrePhysics  [ ] PostCollision  [ ] PostState  [ ] PostAnim |

### 4.2 Trigger Equation (Boolean)

```
(condition1) AND (condition2) AND (input_predicate)
```

Example:
```
wall_grip AND Press(Jump) AND Down(Dash)
```

### 4.3 Captured Values (Latched on Activation)

| Value | Captured? | Used For |
|-------|-----------|----------|
| `facing` | [ ] yes  [ ] no | |
| `wall_side` | [ ] yes  [ ] no | |
| `input_dir` | [ ] yes  [ ] no | |
| `velocity` | [ ] yes  [ ] no | |
| `charge_level` | [ ] yes  [ ] no | |
| `rng_roll` | [ ] yes  [ ] no | |

### 4.4 Arbitration / Consumption (REQUIRED)

| Parameter | Value |
|-----------|-------|
| `priority_group` | System > Damage > Special > Attack > Movement > Idle |
| `priority_value` | integer within group |
| `consumes_inputs` | list of inputs consumed (e.g., [Press(Jump), Press(Dash)]) |
| `conflicts_with` | list of mutually exclusive mechanics |
| `fallback_if_blocked` | [ ] RemainInState  [ ] ConvertToOther  [ ] Ignore |

---

## 5. Execution

### 5.1 Motion Model (REQUIRED)

| Parameter | Value |
|-----------|-------|
| `model_type` | [ ] Parametric  [ ] TableDriven  [ ] Hybrid |

#### Parametric Parameters

| Timing | Axis | Operation | Value (px/frame) | Notes |
|--------|------|-----------|------------------|-------|
| `on_enter` | X | [ ] set  [ ] add  [ ] mul | | |
| `on_enter` | Y | [ ] set  [ ] add  [ ] mul | | |
| `per_frame` | X | [ ] set  [ ] add  [ ] mul | | |
| `per_frame` | Y | [ ] set  [ ] add  [ ] mul | | |

| Parameter | Value |
|-----------|-------|
| `gravity_scale` | (0.0 = no gravity, 1.0 = normal) |
| `control_authority` | (0.0 = locked, 1.0 = full control) |
| `max_speed_x` | px/frame |
| `max_speed_y` | px/frame |
| `friction` | px/frame² |

#### TableDriven Parameters

| Frame | dx | dy | flags |
|-------|----|----|-------|
| 0 | | | |
| 1 | | | |
| ... | | | |

Flags: `input_lock`, `invuln`, `hitbox_id`, `sfx_event`

### 5.2 Phases

| Phase | Duration (frames) | Description |
|-------|-------------------|-------------|
| `startup` | | Before effect begins |
| `active` | | Effect is happening |
| `recovery` | | After effect, before next action |

### 5.3 Collision/Hitbox Semantics

| Parameter | Value |
|-----------|-------|
| `collider_id` | per phase or static |
| `hitbox_model` | [ ] AABB(feet)  [ ] AABB(center)  [ ] Capsule  [ ] Probes |
| `resize_anchor` | [ ] Bottom  [ ] Center  [ ] Top |
| `resize_ordering` | [ ] ResizeThenResolve  [ ] ResolveThenResize |
| `clearance_check_on_expand` | [ ] yes  [ ] no |
| `ground_snap_px` | |
| `wall_snap_px` | |
| `ledge_snap_px` | |
| `axis_resolve_order` | [ ] XThenY  [ ] YThenX |

### 5.4 Hitstop / Freeze (Optional)

| Parameter | Value |
|-----------|-------|
| `hitstop_frames` | |
| `freeze_input_aging` | [ ] yes  [ ] no |
| `freeze_physics` | [ ] yes  [ ] no |
| `freeze_animation` | [ ] yes  [ ] no |
| `freeze_timers` | [ ] yes  [ ] no |

### 5.5 Commitment / Armor (Optional)

| Parameter | Value |
|-----------|-------|
| `commitment_type` | [ ] Cancellable  [ ] SoftCommit  [ ] HardCommit |
| `super_armor_frames` | range |
| `damage_during_armor` | [ ] yes  [ ] no |
| `knockback_during_armor` | [ ] immediate  [ ] delayed  [ ] none |

---

## 6. Termination

### 6.1 Natural End Conditions

| Condition | Transition To |
|-----------|---------------|
| `duration_expires` | |
| `animation_completes` | |
| `touches_ground` | |
| `touches_wall` | |
| `velocity_reaches_zero` | |

### 6.2 Interrupts / Cancels

| Source | Window (frames) | Preserves Velocity? |
|--------|-----------------|---------------------|
| `jump` | | [ ] none  [ ] partial  [ ] full |
| `attack` | | [ ] none  [ ] partial  [ ] full |
| `dash` | | [ ] none  [ ] partial  [ ] full |
| `damage` | always | [ ] none  [ ] partial  [ ] full |

### 6.3 Post-Action Lockouts

| Lockout | Duration (frames) | Notes |
|---------|-------------------|-------|
| `same_action` | | |
| `wall_regrab` | | |
| `dash` | | |
| `jump` | | |

---

## 7. Momentum & Inheritance

### 7.1 Velocity Inheritance Formulas

```
vx_out = ...
vy_out = ...
```

### 7.2 Inheritance Timing

| Parameter | Value |
|-----------|-------|
| `when_inherited` | [ ] PrePhysics  [ ] PostCollision  [ ] OnExit |
| `clamps_applied` | list |

### 7.3 Persistence

| Parameter | Value |
|-----------|-------|
| `velocity_persists_until` | [ ] ground_contact  [ ] wall_contact  [ ] new_action  [ ] timer |
| `decay_rate` | px/frame² (if applicable) |

---

## 8. Edge Cases

### 8.1 Standard Edge Case Checklist

| Edge Case | Handling |
|-----------|----------|
| Input early buffer (N frames) | |
| Input late grace (N frames) | |
| Ground coyote (N frames) | |
| Wall coyote (N frames) | |
| Ground + wall same frame | |
| Wall + ceiling same frame | |
| Re-entry jitter prevention | regrab_lock_frames = |
| Subpixel boundary rounding | |

### 8.2 Hooked Exception Rules

Deterministic rules attached to pipeline hooks:

```
OnEnter(StateX): if (...) then ...
PostCollision: if (...) then ...
```

---

## 9. Verification Artifacts (REQUIRED)

### 9.1 Golden Trace

Expected per-frame values for canonical scenario:

| Frame | State | pos.x | pos.y | vel.x | vel.y | Flags | Notes |
|-------|-------|-------|-------|-------|-------|-------|-------|
| -2 | | | | | | | |
| -1 | | | | | | | |
| 0 | | | | | | | Trigger frame |
| 1 | | | | | | | |
| ... | | | | | | | |

### 9.2 Test Vectors

Automatable input/output pairs:

```toml
[[test_vector]]
name = "basic_execution"
inputs = [
  { frame = 0, buttons = ["right", "dash"] },
  { frame = 5, buttons = ["right", "dash", "jump"] },
]
expect_trigger_frame = 5
expect_vx = 3.5
expect_vy = -5.0
```

### 9.3 Reference Source

| Source Type | Reference |
|-------------|-----------|
| `video` | URL or filename |
| `tas` | URL or filename |
| `disassembly` | URL or filename |
| `capture_conditions` | describe setup |

---

## 10. Open Questions

List ambiguities requiring clarification:

1.
2.
3.

---

## Template Version

- v2.0 (2025-12-20): Frame-accurate universal template
  - Added: Engine contract, frame pipeline, input predicates
  - Added: TOML-compatible structure
  - Changed: Primary units to frames/px-per-frame
  - Added: Golden trace and test vector requirements
