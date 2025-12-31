# SDL3 Sandbox Plan

This repo is a **sandbox**, not a general-purpose engine.

## Current status (reality check)

- Build is self-contained and pinned:
  - SDL3 is fetched via CMake `FetchContent` (`SANDBOX_SDL3_GIT_TAG` in `CMakeLists.txt`).
  - toml++ is pulled via `cmake/CPM.cmake`.
  - Optional deps (ImGui/fmt/spdlog/Tracy/glm/EnTT) are wired via `cmake/ThirdParty.cmake`.
  - Dev tooling includes IWYU (`just iwyu`) with a mapping file for common false-positives (see `cmake/iwyu.mapping`).
- Simulation uses a fixed timestep (1/120s) with a capped accumulator loop (`App::run`) and supports headless smoke runs:
  - `./build/sandbox --frames 120 --video-driver offscreen`
- CLI supports lighter-weight modes for automation:
  - `--no-ui` disables ImGui panels (still renders).
  - `--no-render` runs without creating a window/renderer (headless sim).
  - `--validate [--strict]` validates stage/character/input script TOMLs without starting SDL video.
  - `--no-prefs` disables loading/saving session prefs (useful for deterministic smoke runs).
  - `--input-script PATH` drives gameplay input from a TOML keyframe script (deterministic playback).
  - `--expect-grounded` (with `--frames`) fails if the player is not grounded at exit (useful for regression smoke tests).
  - `--expect-no-respawn` (with `--frames`) fails if the player respawned during the run (helps catch “fell through and got kill-plane reset” regressions).
  - `--expect-min-x X` (with `--frames`) fails if the player’s x at exit is less than X (useful for “did we actually traverse?” checks).
  - `--expect-max-x X` (with `--frames`) fails if the player’s x at exit is greater than X (useful for “did we actually move left?” checks).
  - `--expect-min-vx X` (with `--frames`) fails if the player’s vx at exit is less than X (useful for dashes/launches).
  - `--expect-max-vx X` (with `--frames`) fails if the player’s vx at exit is greater than X (useful for “should be stopped” checks).
  - `--expect-min-y Y` (with `--frames`) fails if the player’s y at exit is less than Y (useful for “did we drop down onto the floor?” checks).
  - `--expect-max-y Y` (with `--frames`) fails if the player’s y at exit is greater than Y (useful for catching “fell through and landed on the floor” cases).
  - `--expect-min-cam-x X` / `--expect-max-cam-x X` (with `--frames`) fail if the camera x at exit is outside the given range.
  - `--expect-min-cam-y Y` / `--expect-max-cam-y Y` (with `--frames`) fail if the camera y at exit is outside the given range.
  - `--expect-hurt-count N` (with `--frames`) fails if the number of hazard hurt events during the run is not N.
  - Convenience selectors: `--stage-id ID` / `--character-id ID`.
- Input supports keyboard and basic gamepad mapping (SDL gamepad subsystem), normalized into `InputState`.
- Gamepad left-stick deadzone is configurable from the Sandbox panel and persisted in session prefs (`gamepad_deadzone`).
- Stages are TOML-driven (`assets/stages/*.toml`) with solid rectangles, optional slopes, optional hazards, spawn points, and optional camera bounds.
- Stage metadata is parsed (stage `version`, `[stage].id`, `[stage].display`) and surfaced in the Stage panel.
- Stage visuals are TOML-driven via optional `[render]` (hex colors for background gradient + platform palette).
- Stage panel shows the active `[render]` palette (with swatches) and can copy TOML snippets for `[render]`, `[collision]`, `[camera]`, `[world]`, and individual `[spawns.*]`.
- Camera follow is stage-configurable via `[camera]` (`deadzone_w/deadzone_h`, `lookahead_x/lookahead_y`).
- Camera micro-behavior is stage-configurable via `[camera]`:
  - look-up/look-down: `look_hold_ms`, `look_up_y`, `look_down_y`
  - boss-room style clamps: `[[camera.locks]]` (trigger rect + locked bounds rect)
- Spawns can include metadata like `facing = -1|1` (`[spawns.*].facing`).
- Characters are TOML-driven (`assets/characters/*.toml`) with actions (dash/spin/spindash/fly/glide) and per-action input mapping.
- Characters can optionally disable mid-air horizontal control via `move.air_control = false` (commitment-style jumps).
- Jump feel includes a TOML-driven fall gravity multiplier (`[jump].fall_gravity_multiplier`) to tune descents.
- Rendering uses a **unified procedural puppet form** (pixel-grid parts + simple spring dynamics) driven by `AnimState`. Placeholder sprite sheets still exist in `assets/sprites/` and are validated, but they are not the default renderer.
- Debug UX is ImGui-first (enabled by default) with a minimal SDL debug-text overlay fallback (if ImGui is unavailable).
- Iteration support: optional auto-reload watches stage/character TOML mtimes and reloads on change (toggle in Sandbox panel; persisted in session prefs).
- Session state is persisted under SDL pref path (last stage/character/spawn, panels open, time scale).
- QoL: if a stage defines world bounds, falling below them triggers an auto-respawn (“kill plane”).
- Hazards (`[[hazards]]`) support iFrames + knockback, plus optional spike-like `ignore_iframes` and Castlevania-ish `lockout_ms`.
- CI runs on GitHub Actions (build + offscreen CTest smoke).

## Debug UI (ImGui) — implemented

ImGui is treated as a **core-only debug layer**. It improves iteration speed, but it does not become gameplay logic.

**What it replaces (conceptually)**
- A separate “Esc menu” / debug-text menu (removed). The `Sandbox` panel is the single control surface now.
- Most of the current debug overlay in `src/core/App.cpp` (`renderDebugOverlay()`), turning it into inspectable panels.
- It does **not** replace simulation/ECS/character logic; ImGui stays in `core/` and never mutates gameplay state directly.

**Implementation rules (keep it “sandbox correct”)**
- ImGui is **core-only** and treated as a debug UI layer.
- UI produces explicit “requests” (reload/respawn/select), and `App` applies them.
- Keep SDL debug-text overlay as a **fallback** when ImGui is disabled/unavailable.
- Keep global hotkeys working: `Ctrl-H` toggles UI panels, `Ctrl-C` quits, `Ctrl-1`/`Ctrl-2` debug toggles.
- Don’t write `imgui.ini` into the repo: store it under `SDL_GetPrefPath()` (outside the repo).

**What we have today**
- DockSpace-based panels: `Sandbox`, `Stage`, `Player`, `Controls`, `Game` (`src/core/DebugUI.*`).
- Game is rendered to an SDL render-target texture using internal resolution presets and displayed inside the `Game` window (letterboxed/integer-scaled).
- Game window shows current internal resolution + scale mode and supports an optional “integer scale only” toggle (persisted).
- Gameplay input is gated when ImGui captures keyboard (and while it captures mouse with a button held) so UI interaction doesn’t move the player.
- ImGui layout is persisted under the SDL pref path.
- Simulation controls: pause, step 1 / step N, time scale.
- Hot reload: apply character TOML in-place (optional runtime reset) without respawning.
- Stage helpers: visualize spawns + world/camera bounds in debug collision, and teleport to the selected spawn.
- Stage panel surfaces collision knobs (`ground_snap`, `step_up`, `skin`) for quick sanity checks while tuning.
- Stage panel shows `[render]` palette values and can copy snippets for `[render]`, `[collision]`, `[camera]`, `[world]`, and per-spawn `[spawns.*]`.
- Player panel can copy repro-friendly spawn snippets from the current player state and shows key gravity multipliers.

## North star

- A small SDL3-based platformer sandbox with a **minimal ECS** and a **single, data-driven character controller**.
- Character differences come from **TOML** (numbers + feature flags + action configs), not subclasses.
- SDL3 is treated as a **platform API**:
  - SDL input is normalized into an `InputState` component.
  - Gameplay systems never poll SDL directly.
- Simulation is **deterministic** given `{input + config + timestep}`.
- Rendering is **separate** from simulation; render code never mutates gameplay state.

## Current structure (matches intended layout)

```text
sdl3-sandbox/
  .github/
    workflows/
      ci.yml
  CMakeLists.txt
  cmake/
    CPM.cmake
    ThirdParty.cmake
  assets/
    forms/
      puppet.toml
    characters/
      bolt.toml
      gale.toml
      nimbus.toml
      vex.toml
      forge.toml
    stages/
      skyway_run.toml
    dev/
      characters/
        *.toml
      stages/
        *.toml
    input_scripts/
      README.md
      *.toml
    sprites/
      *_placeholder.bmp
      README.md
  src/
    main.cpp
    core/
      App.h
      App.cpp
      DebugUI.h
      DebugUI.cpp
      Input.h
      Input.cpp
      SpriteCache.h
      SpriteCache.cpp
      Time.h
    ecs/
      World.h
      World.cpp
      Entity.h
      Components.h
      Systems.h
      Systems.cpp
    stage/
      Stage.h
      Stage.cpp
    character/
      CharacterConfig.h
      CharacterConfig.cpp
      CharacterController.h
      CharacterController.cpp
      Actions.h
      Actions.cpp
    util/
      Paths.h
      TomlUtil.h
  tools/
    gen_placeholder_sprites.py
    new_asset.py
  notes/
    plan.md
    classic_platformer_internal_mechanics.md
```

## Design gaps to close (known deviations from rules)

- We still use very simple AABB collision resolution (good for a sandbox, but it needs “platformer feel” refinements).
- The ImGui `Game` view renders into internal resolution presets and scales proportionally to the panel/window size (nearest for integer scale, linear otherwise).
- Slope support exists (`[[slopes]]`) and is intentionally minimal, but has basic seam/edge handling and regression coverage via `assets/dev/stages/slope_torture.toml`.
- Classic platformers mostly used fixed-point + quantized subpixels (often 8.8 / 1/256 px). We still simulate in floats, but we now support optional subpixel quantization (`[math].subpixel`) and collision sampling snap rules (`math.collision_snap`). A full fixed-point core remains an optional future direction if we want stricter lineage quirks.

## Engine Contract (v2.0 Spec Vocabulary)

This section documents the codebase using the frame-accurate mechanic spec v2.0 vocabulary
(see `notes/ask/templates/mechanic_spec_template_v2.md`).

### Tick / Step

| Parameter | Current Value | Notes |
|-----------|---------------|-------|
| `tick_rate_hz` | 120 | Target: 60 for lineage accuracy |
| `fixed_timestep` | true | `App::run` uses fixed accumulator |
| `sim_step_when_slow` | drop_frames | Capped at 8 steps per render frame |
| `input_sampling` | once_per_sim_tick | `Input::consume()` called per tick |

### Numeric Representation

| Parameter | Current Value | Notes |
|-----------|---------------|-------|
| `position_format` | float | Optional subpixel quantization via `[math].subpixel` |
| `velocity_format` | float | Units: px/s (target: px/frame) |
| `accel_format` | float | Units: px/s² (target: px/frame²) |
| `rounding_mode` | (none) | Float precision |
| `integration` | `pos += vel * dt; vel += accel * dt` | Target: `pos += vel; vel += accel` |

### Frame Pipeline (Current Order)

The simulation pipeline executes in this order per tick:

```
1. InputLatch           - App::run: Input::consume() → world_.input[player]
2. CharacterController  - PrePhysics: actions, velocity changes, gravity
3. PhysicsIntegrate     - Systems::integrate: position += velocity * dt
4. CollisionResolve     - Stage::resolveAABBCollision (X then Y)
5. ContactFlagsUpdate   - Grounded detection from collision
6. HazardCheck          - Systems::hazards: damage, knockback, iFrames
7. AnimationDerive      - Systems::deriveAnimState: AnimStateId from state
```

Key evaluation points:
- **Wall kick**: Evaluated in CharacterController (step 2), before collision
- **Jump**: Evaluated in CharacterController (step 2), before collision
- **Grounded**: Updated in CollisionResolve (step 5), after collision

### Input Predicates Available

| Predicate | Available | Implementation | Notes |
|-----------|-----------|----------------|-------|
| `Down(X)` | ✅ | `inputState.XHeld` | Currently held |
| `Press(X)` | ✅ | `inputState.XPressed` | Rising edge this frame |
| `Release(X)` | ✅ | `inputState.XReleased` | Falling edge this frame |
| `HeldFor(X, N)` | ✅ | `inputState.XHeldFrames` | Frame counter |
| `PressedWithin(X, N)` | ✅ | `inputState.XPressedFrames` | Frame counter |

### Arbitration

Actions are evaluated in fixed order within `CharacterController::tick()`:
1. Wall interaction (slide/climb/kick)
2. Jump (with coyote time and buffering)
3. Dash
4. Spindash
5. Spin
6. Fly
7. Glide

No explicit priority system; first-match wins based on state flags.

### Units Reference

Current config units (v1):
- Velocities: **px/s**
- Accelerations: **px/s²**
- Timers: **ms**
- Tick: **1/120 s**

Target config units (v2):
- Velocities: **px/frame**
- Accelerations: **px/frame²**
- Timers: **frames**
- Tick: **1/60 s**

Conversion at 60Hz:
- `px/frame = px/s ÷ 60`
- `px/frame² = px/s² ÷ 3600`
- `frames = ms × 60 ÷ 1000`

---

## Classic platformer mechanics (reference → config targets)

This repo stays a **sandbox**, but we want to be able to *toggle in* tiny, authentic “feel tweaks”
from specific game lineages (Mario/Sonic/Mega Man/Castlevania/Kirby) **purely via TOML**.

### Source/reference material

We keep the raw “Classic platformer internal mechanics” reference text in:
- `notes/classic_platformer_internal_mechanics.md`

Treat numbers as **starting points**: values vary by region (NTSC/PAL), revision, and per-state tables.

### Units + conversions (so config stays coherent)

Our config values are expressed in “engine units”:
- speeds: **px/s**
- accelerations & gravity: **px/s²**
- timers: **ms**
- simulation tick: fixed **dt = 1/120 s**

Classic docs often report **px/frame** or **px/frame²** at 60fps, and/or 8.8 subpixels/frame.
Useful conversions:

- `px_per_second = px_per_frame * 60`
- `px_per_second2 = px_per_frame2 * 60 * 60`
- `subpixels/frame (8.8) → px/s = (subpx / 256) * 60`

Sanity ranges (60fps source values → our px/s²):
- Gravity ~`0.21–0.39 px/frame²` ⇒ **~`756–1404 px/s²`**

### “Tiny niche tweaks” we want to standardize (data-driven)

The goal is not to clone entire engines, but to expose the small switches/tables that create
recognizable “families” of feel.

**Math / subpixel rules**
- Optional **subpixel quantization** for position/velocity (e.g. 1/256 px steps).
- Optional **pixel-truncation** rules for collision sampling (common in classic implementations).

**Jump models (choose per character)**
- “Mario dual gravity”: reduced rise gravity while jump held, higher gravity otherwise.
- “Sonic clamp”: on early release, clamp upward velocity to a threshold instead of scaling.
- “Fixed jump”: ignore early release (Mega Man NES / commitment platformers).
- Optional table: jump impulse vs horizontal speed index (SMW/SMB3-style).

**Horizontal control models**
- “Momentum-ish” accel + separate brake + turn-resistance (Sonic-like).
- “Responsive” accel + strong friction/brake (Mario-like).
- “Instant” set-velocity / very high accel (Mega Man-like).
- Optional commitment modes: disable air-turn, fixed arcs (Castlevania-like).

**State-based physics overrides**
- Rolling/ducking hitbox changes (Sonic rolling, Mario crouch, slide states).
- Water/ice modifiers (per-zone multipliers for gravity, accel, friction, terminal speed).

**Slopes (two tiers)**
- Tier 1 (already): simple slope support (from above).
- Tier 2 (future): sensor-based sampling + tangent/normal handling for momentum + “downhill helps”.

**Camera micro-behaviors**
- No-backscroll (SMB1), deadzone follow (SMW), look-ahead, look-up/down, boss-room locks.

**Combat-ish staples (future)**
- iFrames timers + exceptions (e.g. “spikes ignore iFrames” as an optional quirk flag).
- Knockback profiles (distance/arc/lockout) per franchise.

### How we implement this without “engine-ifying” the sandbox

Principles:
- Every tweak is **named**, **documented**, and **configurable** (no hidden magic).
- Prefer additive enums/tables over new subclasses.
- Every new tweak gets at least one deterministic **input-script smoke**.

Proposed schema direction (additive; examples, not final):
- Character:
  - `[math] subpixel = 256; collision_snap = "pixel"|"subpixel"`
  - `[jump] model = "impulse"|"dual_gravity"|"clamp_vy"|"fixed"` + per-model fields
  - `[move] model = "approach"|"instant"` + brake/turn knobs
  - `[states.rolling]`, `[states.slide]` hitbox + friction overrides
  - `[environment]` multipliers (water/ice), ideally driven by stage zones
- Stage:
  - `[[zones]]` with `type = "water"|"ice"|...` and modifier payloads (pure data)
  - camera micro flags (no-backscroll, boss locks) as stage config, not hard-coded

---

## Current leftovers (next)

- Enemy + combat baseline is underway (configs + spawns + basic AI + stomp are in).
- Next: shooter projectiles + character attack hitboxes (innate + powerups) + deterministic smokes.
- After that: return to the "tiny niche tweaks" library expansion (reference stage + smokes).

### PixelLab assets — integration status

**Configured but NOT used in any stage:**

| Asset Type | Assets | Notes |
|------------|--------|-------|
| Enemies | `slime_basic`, `bat_basic`, `skeleton_basic` | TOML configs + sprite sheets exist in `assets/enemies/` |
| Collectibles | `coin`, `gem_red`, `gem_blue`, `gem_green` | TOML configs exist in `assets/collectibles/` |

`skyway_run.toml` currently uses only `walker_basic`, `spiky_basic`, `hopper_basic`, `shooter_basic`.
No `[[collectibles]]` entries exist in any stage.

**NOT integrated (raw PNGs only in `assets/sprites/pixellab/objects/`):**

| Category | Available |
|----------|-----------|
| Containers | `chest_closed`, `chest_open`, `item_box` |
| Decorations | `barrel`, `crate`, `torch` |
| Hazards | `spikes_floor`, `spikes_ceiling`, `pipe_horizontal`, `pipe_vertical`, `spring_compressed`, `spring_extended` |
| Weapons | `sword`, `wand_fire`, `wand_ice`, `wand_lightning` |
| Tilesets | `grass_dirt`, `stone_brick` (in `assets/sprites/pixellab/tilesets/`) |

**Fully integrated:**

Hero characters (`bolt`, `gale`, `vex`, `forge`, `nimbus`) have TOML configs with PixelLab sprite sheets.

- Puppet art direction (in flight):
  - Gather external expert feedback using the brief below.
  - Re-evaluate anchor deltas vs accessory variants once feedback arrives (schema + tooling).
  - Expand accessory motifs (pixel grids) and face swaps per character (playable roster done).
  - Add missing named colors + variant toggles to match expert guidance (playable roster done).

## Recent low-hanging fruits (done)

- **Rise gravity multipliers:** add `micro_knobs_ref` smokes for held vs released dual-gravity jumps.
- **Rise gravity off control:** add a `micro_knobs_ref` smoke when held/released rise gravity multipliers are set to `1.0`.
- **Release-drop jump:** add a short-hop smoke for `jump.release_drop_after_frames` / `jump.release_drop`.
- **Release-drop off control:** add a `micro_knobs_ref` smoke for `jump.release_drop = 0` behavior.
- **Dash hold positive:** add a long-hold smoke that asserts the dash triggers after the hold threshold.
- **Spindash same-frame window:** add a smoke for Down+Jump on the same frame (within window).
- **Spindash down-early window:** add a negative smoke where Down is held before the window and no launch occurs.
- **Drop-through hold config:** add `states.drop_through.hold_frames` and a smoke that asserts a 0-frame hold drops cleanly.
- **Dash hold gate:** add `actions.dash.hold_frames` and a smoke that proves short holds don’t trigger.
- **Spindash late-window smoke:** add a negative test where Down arrives after the window and no launch occurs.
- **New-asset template knobs:** `tools/new_asset.py` now includes drop-through hold + dash hold + spindash down window (and updates character template units/fields).
- **Input timing counters:** `InputState` now tracks held/pressed frame counts (and input scripts preserve them).
- **Slide + spindash input windows:** optional `states.slide.input_window_frames` and
  `actions.spindash.down_window_frames` support “pressed within N frames” triggers (with smokes).
- **Jump hold cap knob:** `[jump].max_hold_frames` caps variable jump hold duration, with a micro-knobs ref smoke.
- **No-buffer edge-case smoke:** Simon’s `jump_buffer_frames = 0` now has a regression smoke that
  ensures buffered jumps do not trigger.
- **Micro-knobs reference stage:** `micro_knobs_ref` stage plus smokes that pin jump models, instant
  move, and zone multipliers in one place (using deterministic input scripts).
- **Micro-knobs follow-ups:** add `micro_knobs_ref` smokes for ice sliding and subpixel quantization
  (x-range assertion with `test_subpixel256`).
- **Named color follow-up:** puppet variants now expose `hair` for Sonic/Knuckles and `overalls`
  for Mario/Luigi/Wario to improve palette control.
- **Named color follow-up (tails):** tail tips now use a `tail_tip` key for white tips.
- **Playable roster variants:** Bolt/Gale/Nimbus/Vex/Forge now have custom variant toggles and
  named colors with small accessory/face motifs in the shared puppet form.
- **Procedural puppet form:** unified procedural character rendering with pixel-grid parts and springy secondary motion.
- **Form TOML workflow:** procedural form is TOML-driven, auto-reloadable, and validated in `--validate`.
- **Canon-ish roster tuning:** speeds/jumps/climb feel were brought closer to classic ratios (without changing the controller architecture).
- **New presets:** `kirby`, `test_instant`, `test_subpixel256`.
- **New big stage:** `marathon_huge` (wide traversal + water/ice segments).
- **Fallback stage parity:** `Stage::loadTestStage()` matches `assets/dev/stages/test_stage.toml` spawns/knobs (including `right`).
- **Deterministic automation:** CTest smoke runs use `--no-prefs` to avoid persisted session state influencing results.
- **Stronger smoke assertions:** smoke runs now assert grounded-at-exit and “no respawns” so the kill-plane can’t mask fall-through.
- **Fall-through guard:** `--expect-max-y` lets traversal smokes fail if the player drops below an expected height and lands on the floor.
- **Floor/drop assertions:** `--expect-min-y` lets smokes assert we ended low enough (useful for drop-through checks).
- **One-way drop-through regression:** a `mechanics_torture` stage + scripted smoke verifies Down+Jump drops through one-way platforms.
- **Coyote-time regression:** a `mechanics_torture` scripted smoke jumps after leaving a ledge and still clears the gap.
- **Jump-buffer regression:** a `mechanics_torture` scripted smoke presses jump before landing and expects a buffered jump.
- **Dash regression:** a `mechanics_torture` scripted smoke asserts MegaMan X dash sets a high vx (and stays grounded/no-respawn).
- **Spindash regression:** `mechanics_torture` smokes verify Sonic’s spindash charge holds vx near 0, then release launches to high vx.
- **Glide regression:** a `mechanics_torture` smoke holds jump for Knuckles while falling and asserts he doesn’t hit the floor within 1s.
- **Variable jump regression:** `mechanics_torture` full-hop vs short-hop smokes assert clearly different heights at a fixed frame.
- **Fall-gravity regression:** a `mechanics_torture` smoke drops Sonic from a fixed height and asserts a y range (guards `fall_gravity_multiplier` feel tweaks).
- **More left coverage:** a left-moving seam-tie traversal smoke uses `--expect-max-x` to assert “we actually went left”.
- **More reverse-direction coverage:** one-way seam + subpixel seam traversal smokes cover the solid->slope direction using `--expect-max-x`.
- **Steep-slope stress:** a steep-slope descend smoke catches high-gradient fall-through / snap regressions.
- **Valley seam coverage:** the valley is lifted above the floor and has a traversal smoke to catch fall-through.
- **Slope step-up:** step-up onto slopes is supported (and covered by a regression smoke).
- **Actionable smoke output:** expectation failures print stage/character/spawn plus player pos/vel/grounded/respawns.
- **Scripted smoke traversal:** deterministic input scripts (see `assets/input_scripts/*.toml`) enable movement-based regression tests.
- **Input script hygiene:** `--validate` also checks `assets/input_scripts/*.toml`, and `assets/input_scripts/README.md` documents the format.
- **More slope coverage:** movement-based smokes cover seam ties, one-way seams, peaks, and sub-pixel slope->solid transitions.
- **Slope torture spawn clearance:** `slope_torture` `right_edge` spawn was adjusted to avoid initial overlap with the default 32px-tall character.
- **Stage hygiene:** stage load warns on degenerate/out-of-bounds/overlapping slopes.
- **Hazard follow-ups:** `ignore_iframes` (spike-like) + `lockout_ms` (damage lockout), with deterministic smokes.
- **Hazard hygiene:** stage load warns when hazards extend outside world bounds.
- **Commitment-style air control:** `move.air_control = false` disables mid-air steering, with a deterministic smoke.
- **Wall-slide regression:** `wall_torture` has a slide smoke that holds into the wall and asserts a
  slow fall.
- **Tooling hygiene:** `tools/new_asset.py` templates were updated to match current stage/character TOML schema.
- **IWYU ergonomics:** mapping file exists for common IWYU false-positives (avoid including toml++ `impl/*` directly in project code).

---

## Puppet art direction — external expert brief (ready to send)

Canonical shareable copy: `notes/puppet_art_brief.md`.

We need feedback from an external character artist to make the shared puppet read as iconic 8/16‑bit characters.
This brief is tuned to our actual data model (anchors + pixel grids + variants + palette roles).

### System summary (what exists)
- A single shared CharacterForm (procedural, no sprite sheets).
- Skeleton anchors: head/body/arms/elbows/hands/legs/knees/feet/eyes/mouth.
- Shapes: primitives + pixel_grid parts attached to anchors with offsets and z-order.
- Poses: per-pose anchor overrides + lean + squash/stretch.
- Animations: sequences of poses with timing or velocity-blend.
- Palette: base + accent → base/light/dark/accent + outline.
- Named colors: optional keys (skin, glove, metal, etc.).
- Variants: toggle shapes via only_in_variants / hidden_in_variants.
- Dynamics: springy secondary motion per anchor.
- IK: simple 2-bone IK for arms/legs.

### Constraints you must design within
- One shared skeleton across all characters (no per-character skeleton scale yet).
- Recognition must come from proportion cues + 2–3 iconic accessories.
- Prefer separate anchored shapes for hats/quills/helmets/mustache, not baked into torso.
- Pixel-grid motifs should be 6–10 px wide for key cues, not full art.
- Faces are assembled from simple eye/mouth/brow swaps via pose filters.

### Output you should deliver (per character)
Use the exact template below for each character.

Characters:
- Sonic
- Mario
- Luigi
- Wario
- Mega Man X
- Mega Man (NES)
- Zero
- Kirby
- Tails
- Knuckles
- Simon (Castlevania)
- Ninja (generic)

Template (copy/paste):

Character: <name>

A) Anchor deltas (relative)
- <anchor>: (Δx, Δy) — reason — priority (1–5)

B) Must-have features (separate vs baked)
- <feature>: separate/baked, type, anchor, always/variant, z intent

C) Pixel-grid motifs (3–5)
- Motif: <name>
  - size: WxH
  - pivot: (px, py)
  - grid: <ASCII>
  - legend: <char>=<role/key> …

D) Z-order rules
- <rule list>

E) Palette usage
- base role goes on: …
- light role goes on: …
- dark role goes on: …
- accent role goes on: …
- named colors: key #RRGGBB — usage

F) Face swaps (minimal)
- neutral: …
- smile: …
- angry: …
- surprised: …
- yawn: …

G) Best single improvement (A/B/C)
- pick: …
- why: …

H) Sprite box vs free proportions
- recommendation: …
- why: …

### Questions for the expert (self-contained)
1. Which anchors must move per character to hit iconic silhouette? Provide relative deltas + priority.
2. Which identity features must be separate accessory shapes vs baked into torso/head grids for 8–12px readability?
3. Provide pixel-grid patterns (size + pivot + ASCII grid + legend) for the 3–5 most iconic details per character.
4. Which z-order relationships are critical to prevent identity breakage?
5. How should base/light/dark/accent roles be allocated per character under top-left lighting? Include named colors if needed.
6. Which face swaps (neutral/smile/angry/surprised/yawn) are mandatory, and what should they look like in minimal pixel-grid terms?
7. Forced choice: A anchor deltas vs B accessory grids vs C face swaps — which single change yields the largest recognizability gain?
8. Should we tie proportions to canonical sprite boxes or keep proportions free and focus on silhouette only?

---

## Appendix A — Puppet anchors + motif map (current)

### Anchors (assets/forms/puppet.toml)
- root: [0, 0]
- body: [0, -12]
- head: [0, -24]
- eye_l: [-3, -25]
- eye_r: [3, -25]
- mouth: [0, -21]
- arm_front: [6, -16]
- arm_back: [-6, -16]
- elbow_front: [7, -14]
- elbow_back: [-7, -14]
- hand_front: [8, -12]
- hand_back: [-8, -12]
- leg_front: [3, -5]
- leg_back: [-3, -5]
- knee_front: [3, -2]
- knee_back: [-3, -2]
- foot_front: [3, 0]
- foot_back: [-3, 0]

### Core motifs (always-on shapes)
- torso: pixel_grid, anchor=body, size=8x8, pivot=(3.5, 3.5)
- head: pixel_grid, anchor=head, size=8x8, pivot=(3.5, 3.5)
- arms/forearms/legs/calves: pixel_grid 3x4 / 3x3 (anchors arm/leg/elbow/knee)
- hands: pixel_grid 3x3 (anchors hand_front/back)
- feet: pixel_grid 4x2 (anchors foot_front/back)
- eyes: rects (eye_l/eye_r) + pupils (pose-gated for surprise)
- mouths/brows: pose-gated rects/pixel grids

### Variant motifs (only_in_variants)
- Sonic: `sonic_spines` (pixel_grid, head, z=2)
- Tails: `tails_tail_l/r` (pixel_grid, body, z=-7/-8), `tails_chest`
- Knuckles: `knuckles_dreads` (head), `knuckles_spikes` (hand_front), `knuckles_chest`
- Mario/Luigi/Wario: `cap`, `cap_brim`, `cap_emblem_*`, `moustache_*`, `overalls`
- Mega Man X/NES: `helmet_gem` (head), `blaster` (hand_front)
- Zero: `zero_hair` (head), `zero_saber` (hand_back)
- Kirby: `kirby_cheek_l/r`
- Ninja: `ninja_mask`, `ninja_headband`, `ninja_headband_tail`, `ninja_katana`
- Simon: `simon_headband`, `simon_headband_tail`, `simon_whip`

### Pose-gated face motifs
- Mouths: neutral, yawn, smile, angry, surprised (pose filters)
- Eyes: normal + surprise variants (pose filters)
- Brows: angry brows (pose filters)

# Roadmap

Each milestone is meant to be shippable and keep the sandbox playable.

## Milestone 1 — Input normalization (single source of truth)

**Outcome**
- SDL input is handled in one place and written into an `InputState` component deterministically.

**Work**
- **Status: DONE.**
- Implemented in `src/core/Input.*` and `src/core/App.cpp`.

**Acceptance**
- No non-core code includes or calls SDL input APIs.
- Inputs behave identically across runs for the same event stream.

## Milestone 2 — Deterministic fixed timestep

**Outcome**
- Simulation is stable and deterministic; rendering is decoupled from simulation step rate.

**Work**
- **Status: DONE.**
- Implemented in `App::run` (fixed `dt = 1/120`, clamped accumulator via `SDL_GetTicksNS()`, `--frames N` smoke mode).

**Acceptance**
- Player movement feels consistent at low/high FPS.
- Simulation step count is predictable and capped.

## Milestone 3 — Stage data (TOML-driven solids + spawns)

**Outcome**
- Stages are runtime data in `assets/` (no level geometry baked into code).

**Work**
- **Status: DONE (baseline).**
- Stage TOML format:
  - Solid rectangles list.
  - Optional slope rectangles list (`[[slopes]]`).
  - Named spawn points.
  - Optional camera bounds.
  - Optional camera follow tuning: `[camera] deadzone_w/deadzone_h/lookahead_x/lookahead_y`.
  - Optional render palette: `[render]` with hex `"RRGGBB"` keys (`bg_top`, `bg_bottom`, `platform_*`).
  - Stage metadata: `version`, `[stage].id`, `[stage].display`.
- Implemented `Stage::loadFromToml(path)`; `loadTestStage()` remains as fallback.
- CLI args to pick stage + character TOML exist in `src/main.cpp`.

**Acceptance**
- Can create a new stage by adding a TOML file only.

## Milestone 4 — Collision/physics stability pass

**Outcome**
- Collision resolution is stable and “platformer-feel first,” with fewer sticky corners.

**Work**
- **Status: DONE (baseline).**
- Implemented:
  - Resolve per-axis (X then Y).
  - Track “grounded” from downward contacts.
  - Ground snap tolerance (stage TOML: `[collision].ground_snap`).
  - Optional step-up for low obstacles (stage TOML: `[collision].step_up`).
  - Optional collision “skin” inset to reduce corner snagging (stage TOML: `[collision].skin`).
  - Optional one-way platforms (stage TOML: `[[solids]].one_way = true`).
  - Drop-through (Down+Jump) temporarily ignores one-way platforms.
  - Baseline slopes (stage TOML: `[[slopes]]`) with simple ground resolution from above.

**Acceptance**
- Grounding is reliable (no flicker).
- Wall/floor contacts don’t jitter under typical movement.

## Milestone 5 — Camera and view transforms

**Outcome**
- Camera follows player; stage and entities are rendered in world space through a camera transform.

**Work**
- **Status: DONE (baseline).**
- Camera follows player and is clamped to stage camera/world bounds when present.

**Acceptance**
- Player remains framed while moving through larger stages.

## Milestone 6 — Action framework maturity (dash/spin/fly)

**Outcome**
- Actions are optional, parameterized, deterministic, and evaluated from `InputState`.

**Work**
- **Status: DONE (baseline).**
- Actions are optional, parameterized, deterministic, and evaluated from `InputState`.
- Per-action input mapping is configurable in TOML (`input = "action1"|"action2"|"jump"`).

**Acceptance**
- Sonic/Mario feel changes are TOML-only.
- Adding a new action is additive and keeps controller generic.

## Milestone 7 — Debug UX (overlay + toggles)

**Outcome**
- Fast iteration: see what the simulation thinks (grounded, coyote, buffers, cooldowns).

**Work**
- **Status: DONE (baseline).**
- Debug overlay exists (SDL debug-text fallback + ImGui overlay).
- Debug toggles exist for collision visualization and camera bounds.
- ImGui provides DockSpace panels, embedded game view, and extra iteration controls:
  - Pause/step/time-scale.
  - Apply character config in-place.
  - Teleport to spawn.
  - Auto reload TOML (stage + character) on file change.
- Optional overlay content:
  - Position/velocity.
  - Grounded + timers (coyote/jump buffer/action cooldown).
  - Current character id/display name.

**Acceptance**
- Can diagnose “why didn’t I jump?” without printf-chasing.

## Milestone 8 — Rendering placeholders (forms/anim states)

**Outcome**
- Rendering remains separate, but we can express character “states” visually.

**Work**
- **Status: DONE (baseline).**
- `AnimState` is derived in `Systems::deriveAnimState`, and characters are drawn via `Visual::CharacterForm` (procedural shapes) selected by character id.

**Acceptance**
- Rendering changes do not affect gameplay determinism.

## Milestone 9 — CI and smoke tests

**Outcome**
- Regressions are caught early; headless runs are reliable (CI-ready).

**Work**
- **Status: DONE (baseline).**
- CTest smoke tests run:
  - Offscreen (`--video-driver offscreen`).
  - Headless (`--no-render`).
  - Offscreen without UI (`--no-ui`).
  - Per-stage and per-character TOML loads (each file in `assets/stages/*.toml` and `assets/characters/*.toml`).
- GitHub Actions builds and runs the smoke test on Ubuntu (`.github/workflows/ci.yml`).

**Acceptance**
- CTest verifies the binary starts and runs deterministic frames without a display.

## Milestone 10 — Classic “micro-mechanics” library (config-first)

**Outcome**
- The sandbox can reproduce “family” feel differences via **small, named, TOML-only** knobs:
  - jump model variants (dual gravity / clamp / fixed)
  - subpixel quantization modes
  - water/ice modifiers
  - basic knockback/iFrames (once combat exists)

**Work**
- **Status: DONE (foundation).**
- Implemented the first set of “micro-mechanics” knobs (each with deterministic smoke coverage):
  - Character jump model variants via `[jump].model`: `impulse`, `dual_gravity`, `clamp_vy`.
  - Optional subpixel quantization via `[math].subpixel` + collision sampling snap via `math.collision_snap`.
  - Stage `[[zones]]` with water/ice multipliers applied to movement/jump/gravity caps.
  - Stage `camera.no_backscroll` (SMB1-style) for camera micro-behavior.

**Acceptance**
- A character can switch between “Mario-ish” / “Sonic-ish” / “MegaMan-ish” jump feel without code changes.
- Each new micro-mechanic is covered by at least one deterministic smoke test.

---

## Milestone 11 — More micro-mechanics (lineage knobs)

**Outcome**
- Add a few more high-leverage toggles that “lock in” classic feel families.

**Work**
- **Status: DONE (phase 1).**
- Added the next two high-leverage toggles (with deterministic smokes):
  - `[jump].model = "fixed"` (ignore early release; commitment jumps).
  - `[move].model = "instant"` (set-velocity style horizontal control).

**Acceptance**
- Two characters can share the same base numbers but feel meaningfully different via model switches.

## Milestone 12 — Micro-mechanics follow-ups

**Outcome**
- Expand the library with a few more optional, lineage-flavored “quirks” that stay config-first.

**Work**
- **Status: DONE.**
- **DONE:** Jump impulse table vs horizontal speed index (SMW/SMB3-style) via `[[jump.impulse_by_speed]]` with deterministic smokes.
- **DONE:** Camera look-up/look-down + boss-room camera locks via stage `[camera]` with deterministic smokes.
- **DONE:** iFrames + knockback hazards via `[[hazards]]` with deterministic smokes.

**Acceptance**
- Each added quirk has a clear knob + a regression smoke.

## Milestone 13 — Enemies + combat baseline (data-driven)

**Outcome**
- Enemies exist as ECS entities driven by TOML configs (no subclasses).
- Combat is deterministic, uses explicit damage data, and supports stomp + hitbox actions.

**Work**
- **Status: DONE.**
- DONE: enemy/component data + configs (walker/spiky/shooter/hopper).
- DONE: stage spawns via `[[enemies]]` entries.
- DONE: basic AI (walker/hopper) + stomp/touch damage + iFrames/knockback.
- DONE: shooter projectiles (timer + projectile entities).
- DONE: character attack hitboxes (innate + powerup-granted), driven by TOML.
- DONE: deterministic smokes for stomp + attack interactions.

**Acceptance**
- A stage can spawn at least one enemy of each archetype via TOML.
- Stompable enemies can be defeated by jumping on them; spiky enemies cannot.
- Shooter spawns projectiles on a timer; projectiles damage the player.
- One character can defeat a non-stompable enemy via a configured attack action.

# Optional libraries

These are **not required** for the sandbox.

**Wired via `cmake/ThirdParty.cmake`**

- **Dear ImGui** — immediate-mode debug UI (menus, inspectors, live tuning): https://github.com/ocornut/imgui
- **fmt** — safer string formatting (if we outgrow `printf`/SDL debug format text): https://github.com/fmtlib/fmt
- **spdlog** — structured logging with sinks/levels (pairs well with fmt): https://github.com/gabime/spdlog
- **Tracy** — profiling/tracing once performance matters: https://github.com/wolfpld/tracy
- **glm** — math types if we expand beyond simple 2D vectors: https://github.com/g-truc/glm
- **EnTT** — full ECS library (likely *not* aligned with our “minimal ECS” goal, but good reference): https://github.com/skypjack/entt
- **SDL3** — upstream SDL (already used via FetchContent): https://github.com/libsdl-org/SDL

**Future candidates (not wired)**
- **SDL_shadercross** — shader translation toolchain if/when we move to a shader-based renderer: https://github.com/libsdl-org/SDL_shadercross
