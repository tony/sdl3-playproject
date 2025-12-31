# MECHANIC: Dash Wall Kick (MMX2/MMX3 Style)

Frame-accurate specification for the boosted wall kick when dashing.

---

## 0. Engine Contract (REQUIRED)

### 0.1 Tick / Step

| Parameter | Value |
|-----------|-------|
| `tick_rate_hz` | 60 |
| `fixed_timestep` | true |
| `sim_step_when_slow` | [x] drop_frames  [ ] run_multiple_sim_steps |
| `input_sampling` | [x] once_per_sim_tick  [ ] event_accumulate_then_latch |

### 0.2 Numeric Representation

| Parameter | Value |
|-----------|-------|
| `position_format` | [ ] fixed Q___.___  [x] int_subpixel(scale=256)  [ ] float |
| `velocity_format` | [x] int_subpixel(scale=256) |
| `rounding_mode` | [x] trunc_toward_zero  [ ] floor  [ ] round_nearest |
| `integration` | pos += vel; vel += accel |

### 0.3 Direction Normalization

| Parameter | Value |
|-----------|-------|
| `socd_resolution` | [x] neutral  [ ] last_input_wins  [ ] prefer_left  [ ] prefer_right |

### 0.4 Frame Pipeline (Authoritative Order)

This mechanic evaluates at **TriggerEval:PostCollision** (step 8).

---

## 1. Identity

| Field | Value |
|-------|-------|
| `name` | Dash Wall Kick |
| `source_games` | MMX2, MMX3 |
| `characters` | X, Zero |
| `category` | [x] Movement  [ ] Attack  [ ] Defense  [ ] Special  [ ] Cancel  [ ] System |
| `complexity` | [ ] Basic  [x] Intermediate  [ ] Advanced  [ ] Expert |

---

## 2. State/Signal Dictionary

| Signal | Definition |
|--------|------------|
| `wall_sliding` | Character is in wall slide state (touching wall + airborne + falling) |
| `wall_contact` | wall_probe overlaps solid within probe distance |
| `dashing` | Dash button is currently **held** (not just pressed this frame) |

### Probes/Sensors

| Probe | Position | Size | Purpose |
|-------|----------|------|---------|
| `wall_probe` | side-center | 1px x 8px | wall detection for slide/kick |

---

## 3. Input Model (REQUIRED)

### 3.1 Per-Button/Direction Signals

Standard signals available (see template).

### 3.2 Key Input Insight

**X2/X3 checks HELD state, not press events.** This is what makes it lenient:
- Hold dash while approaching wall ✓
- Keep holding while wall sliding ✓
- Press jump → dash kick triggers ✓

The simplicity comes from checking `Down(Dash)` not `Press(Dash)`.

### 3.3 Compound Timing Semantics

| Parameter | Value |
|-----------|-------|
| `anchor_event` | Press(Jump) |
| `simultaneous_tolerance` | 0 frames (no buffering) |
| `pre_buffer_frames` | 0 (no pre-buffering of jump input) |
| `post_grace_frames` | 0 (no coyote time for wall contact) |

### 3.4 Optional Modules

Not used for this mechanic.

---

## 4. Activation

### 4.1 Evaluation Hook (REQUIRED)

| Parameter | Value |
|-----------|-------|
| `trigger_eval_hook` | [x] PostCollision  [ ] PrePhysics  [ ] PostState  [ ] PostAnim |

### 4.2 Trigger Equation (Boolean)

```
wall_sliding AND Press(Jump) AND Down(Dash)
```

Note: `Down(Dash)` = held state. This fires whether dash was pressed 100 frames ago or this exact frame.

### 4.3 Captured Values (Latched on Activation)

| Value | Captured? | Used For |
|-------|-----------|----------|
| `facing` | [x] yes | Animation direction |
| `wall_side` | [x] yes | Determines kick direction (away from wall) |
| `input_dir` | [ ] no | |
| `velocity` | [ ] no | |

### 4.4 Arbitration / Consumption (REQUIRED)

| Parameter | Value |
|-----------|-------|
| `priority_group` | Movement |
| `priority_value` | 10 (higher than normal wall kick = 5) |
| `consumes_inputs` | [Press(Jump)] |
| `conflicts_with` | [WallKick] (if dash kick triggers, normal kick doesn't) |
| `fallback_if_blocked` | [x] ConvertToOther (fall back to normal wall kick) |

---

## 5. Execution

### 5.1 Motion Model (REQUIRED)

| Parameter | Value |
|-----------|-------|
| `model_type` | [x] Parametric  [ ] TableDriven  [ ] Hybrid |

#### Parametric Parameters

| Timing | Axis | Operation | Value (px/frame) | Notes |
|--------|------|-----------|------------------|-------|
| `on_enter` | X | [x] set | 3.5 | Away from wall (sign from wall_side) |
| `on_enter` | Y | [x] set | -5.0 | Standard jump impulse (negative = up) |
| `per_frame` | X | add | 0.0 | No modification during kick |
| `per_frame` | Y | add | 0.0 | Gravity handled separately |

| Parameter | Value |
|-----------|-------|
| `gravity_scale` | 1.0 (normal gravity applies immediately) |
| `control_authority` | 1.0 (full air control after kick) |
| `max_speed_x` | 3.5 px/frame |
| `max_speed_y` | 10.0 px/frame |

### 5.2 Phases

| Phase | Duration (frames) | Description |
|-------|-------------------|-------------|
| `startup` | 0 | Instant |
| `active` | 1 | Just the velocity set |
| `recovery` | 0 | Immediately transitions to airborne |

### 5.3 Collision/Hitbox Semantics

| Parameter | Value |
|-----------|-------|
| `hitbox_model` | [x] AABB(feet) |
| `axis_resolve_order` | [ ] XThenY  [x] YThenX |

---

## 6. Termination

### 6.1 Natural End Conditions

| Condition | Transition To |
|-----------|---------------|
| `touches_ground` | Idle |
| `touches_wall` | WallSlide |

### 6.2 Interrupts / Cancels

| Source | Window (frames) | Preserves Velocity? |
|--------|-----------------|---------------------|
| `dash` | all | [x] partial (can air dash) |
| `attack` | all | [x] full |
| `damage` | always | [ ] none |

### 6.3 Post-Action Lockouts

| Lockout | Duration (frames) | Notes |
|---------|-------------------|-------|
| `wall_regrab` | 9 (~150ms at 60fps) | Prevents immediate re-grab |

---

## 7. Momentum & Inheritance

### 7.1 Velocity Inheritance Formulas

```
vx_out = vx  (preserve horizontal momentum)
vy_out = vy  (preserve vertical momentum)
```

### 7.2 Inheritance Timing

| Parameter | Value |
|-----------|-------|
| `when_inherited` | [x] OnExit |

### 7.3 Persistence

| Parameter | Value |
|-----------|-------|
| `velocity_persists_until` | [x] ground_contact  [ ] wall_contact  [ ] new_action |

---

## 8. Edge Cases

### 8.1 Standard Edge Case Checklist

| Edge Case | Handling |
|-----------|----------|
| Input early buffer | 0 frames (no jump buffering) |
| Input late grace | 0 frames (no coyote time) |
| Wall coyote | 0 frames (must be touching on exact frame) |
| Ground + wall same frame | Wall kick takes priority |
| Wall + ceiling same frame | Ceiling stops upward velocity |
| Re-entry jitter prevention | regrab_lock_frames = 9 |

### 8.2 Hooked Exception Rules

```
PostCollision: if (touched_ground_this_frame) then transition_to_grounded_before_kick_check
```

---

## 9. Verification Artifacts (REQUIRED)

### 9.1 Golden Trace

Expected per-frame values for canonical scenario (kick from left wall):

| Frame | State | pos.x | pos.y | vel.x | vel.y | Flags | Notes |
|-------|-------|-------|-------|-------|-------|-------|-------|
| -2 | WallSlide | 8.0 | 100.0 | 0.0 | 1.5 | wall_contact, dash_held | Sliding down, holding dash |
| -1 | WallSlide | 8.0 | 101.5 | 0.0 | 1.5 | wall_contact, dash_held | Still sliding |
| 0 | DashWallKick | 8.0 | 103.0 | 3.5 | -5.0 | jump_pressed, dash_held | **TRIGGER FRAME** |
| 1 | Airborne | 11.5 | 98.0 | 3.5 | -4.85 | | Momentum applied |
| 2 | Airborne | 15.0 | 93.15 | 3.5 | -4.7 | | Continuing arc |

### 9.2 Test Vectors

```toml
[[test_vector]]
name = "basic_dash_wall_kick"
inputs = [
  { frame = -10, buttons = ["dash"] },
  { frame = -5, buttons = ["dash", "left"] },
  { frame = 0, buttons = ["dash", "left", "jump"] },
]
expect_trigger_frame = 0
expect_vx = 3.5
expect_vy = -5.0

[[test_vector]]
name = "dash_pressed_same_frame_as_jump"
inputs = [
  { frame = 0, buttons = ["left", "dash", "jump"] },
]
expect_trigger_frame = 0
expect_vx = 3.5  # Down(Dash) is true even on press frame
expect_vy = -5.0

[[test_vector]]
name = "dash_not_held_normal_kick"
inputs = [
  { frame = 0, buttons = ["left", "jump"] },
]
expect_trigger_frame = 0
expect_vx = 1.5  # Normal wall kick, not dash kick
expect_vy = -5.0

[[test_vector]]
name = "dash_released_before_jump"
inputs = [
  { frame = -5, buttons = ["dash", "left"] },
  { frame = -1, buttons = ["left"] },  # Released dash
  { frame = 0, buttons = ["left", "jump"] },
]
expect_trigger_frame = 0
expect_vx = 1.5  # Normal kick, dash not held on frame 0
expect_vy = -5.0
```

### 9.3 Reference Source

| Source Type | Reference |
|-------------|-----------|
| `disassembly` | MMX2 ROM analysis |
| `video` | TAS wall climb optimization footage |
| `capture_conditions` | X wall sliding, holding dash, presses jump |

---

## 10. Open Questions

1. Does MMX2/X3 have any sub-frame input priority? (dash+jump same frame)
2. Exact gravity value in original (estimated 0.15 px/frame²)
3. Does wall_probe distance vary by character hitbox width?
4. Is there a minimum wall slide duration before kick is allowed?

---

## Comparison: Dash Wall Kick vs Normal Wall Kick

| Parameter | Dash Wall Kick | Normal Wall Kick |
|-----------|----------------|------------------|
| Horizontal velocity | 3.5 px/frame | 1.5 px/frame |
| Trigger condition | Down(Dash) AND Press(Jump) | Press(Jump) only |
| Priority | 10 | 5 |
| All other parameters | Same | Same |

---

## Template Version

Filled using mechanic_spec_template_v2.md (2025-12-20)
