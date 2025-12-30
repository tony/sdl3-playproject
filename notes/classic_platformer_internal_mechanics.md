# Classic platformer internal mechanics: a technical reference guide

This file is **reference material**, intended to inform configuration knobs and micro-mechanics in
the sandbox. Treat numeric values as *approximate* and *version-dependent* unless backed by a
specific disassembly/ROM address for the exact revision you care about.

---

The hidden mechanics powering iconic 8-bit and 16-bit platformers share remarkable mathematical foundations while diverging in implementation philosophy. **Fixed-point math (typically 8.8 format)** underlies virtually all movement systems, gravity operates between **0.21–0.39 pixels/frame²**, and variable jump height—achieved by modifying gravity based on button hold—became the industry standard after Mario pioneered it. This reference compiles precise numeric values from disassemblies, RAM maps, and ROM hacking communities for engine configuration.

The data reveals three distinct design philosophies: Nintendo's Mario games emphasize **accessible, forgiving physics** with coyote time and generous air control; Sega's Sonic games implement **momentum-based locomotion** with complex slope interactions; and Capcom/Konami titles favor **precise, commitment-heavy movement** where jump trajectories are largely predetermined.

---

## Fixed-point math forms the universal foundation

All games examined use fixed-point arithmetic for subpixel precision, preventing the "sticky" movement that integer-only systems produce. The dominant format is **8.8 fixed-point** (8 bits integer, 8 bits fractional), yielding **256 subpixels per pixel**.

| Game | Format | Subpixels/Pixel | Notes |
|------|--------|-----------------|-------|
| Sonic 1-3 | 8.8 | 256 | Velocity and position both use this format |
| SMB1/SMB3 | 8.8 | 256 | Position/MoveForce stored in separate bytes |
| Super Mario World | 16-bit | 16 | Measured in sixteenths of a pixel |
| Mega Man (NES) | UQ8.8 | 256 | Unsigned; 64=0.25, 128=0.5, 192=0.75 |
| Mega Man X | 16-bit | Variable | Full 16-bit pixel coordinates |
| Castlevania | Integer | 1 | No subpixel (explains "stiff" feel) |

Sonic's implementation stores X/Y pixel positions as 2-byte words with separate 1-byte subpixel values. Velocity uses signed words, enabling smooth acceleration curves. The key insight: **subpixel precision accumulates** even when pixel position doesn't change, creating the "building momentum" sensation players feel.

---

## Acceleration, speed caps, and momentum curves define game feel

Movement physics vary dramatically across franchises. Sonic prioritizes momentum conservation; Mario emphasizes responsiveness; Mega Man opts for instant velocity changes.

### Speed values (subpixels/frame at 60fps)

**Sonic Series (Genesis):**
- Walking acceleration: Gradual curve building to maximum
- Maximum running speed: **1536 subpixels/frame (6 pixels/frame)**
- Rolling speed cap: **4096 subpixels/frame (16 pixels/frame)**
- Spin dash base: **2048 subpixels/frame (8 pixels/frame)**
- Spin dash maximum: **3072 subpixels/frame (12 pixels/frame)**, achieved with 6 button taps
- Air acceleration exceeds ground acceleration in Sonic 1

**Mario Series (NES/SNES):**
| State | SMB3 Speed | SMW Speed |
|-------|------------|-----------|
| Walking | $18 (24) | Oscillates 35-37 |
| Running (B held) | $28 (40) | Oscillates 35-37 |
| P-Speed/Dashing | $38 (56) | Oscillates 47-49 |

SMW's speed oscillation pattern (36→35→36→35→37) creates a characteristic "humming" acceleration feel. The P-meter fills to **112** before sprint activates.

**Mega Man Series:**
| Game | Walk Speed | Slide Speed | Dash Speed |
|------|------------|-------------|------------|
| MM1-2 | 1.375 px/frame | N/A | N/A |
| MM3-6 | 1.3125 px/frame | ~2.0 px/frame | N/A |
| Mega Man X | 1.5 px/frame | N/A | 3.5 px/frame |

**Castlevania (NES):** Uses a stark **1 pixel/frame** for walking, jumping, and knockback—no acceleration curves, creating the series' infamous "commitment" feel.

### Friction and deceleration

Deceleration typically exceeds acceleration, enabling quick direction changes. Sonic's deceleration on slopes depends on a signed byte (-128 to 127) representing gradient, with values ≥16 actively propelling the player downhill. SMW's friction tables are modified for ice levels, and course headers in SMB3 contain a 3-bit friction factor field.

---

## Gravity and jump mechanics reveal design priorities

Gravity constants cluster around **0.21–0.39 pixels/frame²**, with variable jump height achieved through different methods across franchises.

### Gravity values

| Game | Gravity (px/frame²) | Terminal Velocity |
|------|--------------------|--------------------|
| Sonic 1-3 | ~0.21875 (56 subpixels) | Rolling: 16 px/frame |
| SMB3 | ~0.02 (rising), ~0.3 (falling) | $40-$45 (~64-69) |
| SMW | 0.11 (6.9× Earth gravity scaled) | Variable |
| Mega Man (NES) | ~0.25-0.39 | Not documented |
| Mega Man X | 0.25 | 5.75 px/frame |
| Castlevania | Fixed arc | N/A |

### Variable jump height implementations

**Sonic:** Releasing the jump button when Y-velocity is upward and <1024 subpixels (4 px/frame) immediately sets Y-velocity to 1024, creating an abrupt cutoff.

**Mario (SMB3):** Uses dual gravity values—**$01/frame while A held and rising**, **$05/frame otherwise**. Jump terminates when vertical velocity exceeds $E0 (signed -32) or A is released. Higher horizontal speed yields more powerful jumps via a lookup table at $A648.

**SMW Jump Speed Table (at $00D2BD):**
| X Speed Index | Normal Jump | Spin Jump |
|---------------|-------------|-----------|
| 0 (stationary) | $B0 (176) | $B6 (182) |
| 7 (maximum) | $9F (159) | $A6 (166) |

Lower values = higher jumps (negative = upward). Faster horizontal movement produces higher jumps.

**Mega Man (NES):** Jump height is **completely fixed**—releasing the button early has no effect. Initial velocity of ~4.87 px/frame upward, gravity reduces this by 0.25 px/frame² until apex.

**Mega Man X:** Introduced variable jump height to the series, with initial velocity of **5.0 px/frame** modified by slope angle (5.25-5.5 on upward slopes).

---

## Sonic's terrain sensor system enables complex slope physics

Sonic's collision detection uses a sophisticated sensor ray system rather than simple bounding boxes, enabling seamless loop traversal and slope physics.

### Sensor configuration

The system employs multiple raycast sensors:
- **A and B (floor sensors):** Positioned at left and right edges, cast downward
- **Ceiling sensors:** Cast upward for overhead collision
- **Wall sensors:** Cast horizontally for wall detection

Each 16×16 tile block contains a **height map** (16 values indicating pixel height per column). When traveling vertically up walls, heights function as widths; when upside-down, inverse tile heights apply.

### Collision types per block

Blocks can be flagged as: top-solid only, left/right/bottom solid, or all-solid. Sonic 2+ introduced the **path system** where each block has solidity for "path 0" and "path 1," with invisible path-switcher objects enabling seamless loop traversal.

### Slope angle storage

Angles are stored as a signed byte (-128 to 127), with **256 gradations**. Negative values indicate terrain rising rightward; positive values indicate terrain falling rightward. Slopes with absolute value ≥16 actively propel the player.

---

## Camera systems balance responsiveness with stability

Camera behavior profoundly affects perceived game feel. Three patterns dominate: locked horizontal scrolling (SMB1), smooth following with deadzone (SMW/Sonic), and screen-by-screen transitions (Mega Man NES).

### Horizontal camera patterns

**SMB1:** Camera follows player horizontally only with **no backward scrolling** (`ScrollLock` at $0723). `ScrollFractional` at $0768 handles subpixel scrolling.

**Sonic:** Camera attempts to keep player centered with configurable lag/smoothing. Look-up/look-down triggers after holding direction while standing for several seconds. Screen resolution: **320×224 pixels (NTSC)**.

**Mega Man X:** Smooth horizontal/vertical scrolling with lookahead. Camera positions stored at 7E00B4-7E00B7 (X1).

### Vertical camera behavior

SMB1 lacks vertical scrolling in most areas. SMB3's course header byte $06 controls vertical scroll mode. SMW supports dual-axis scrolling with deadzone regions. Sonic uses player-relative vertical positioning with special handling for wrapping levels (Metropolis, Marble Garden).

### Boss room locking

All examined games lock camera boundaries during boss encounters. Mega Man's screen-by-screen structure makes this trivial; Sonic and Mario games set explicit scroll boundaries that prevent camera movement beyond boss arena edges.

## Enemy spawning uses screen-relative triggers with slot management

Memory constraints forced elegant solutions for enemy management, with most games using screen-relative spawn triggers and aggressive despawning.

### Spawn mechanisms

| Game | Spawn Trigger | Despawn Condition | Respawn Rule |
|------|--------------|-------------------|--------------|
| Sonic | Camera proximity | Scrolled off-screen | Re-entering spawn region |
| SMB1 | Screen scroll to X position | Scrolled past left edge | Never (no backtracking) |
| SMW | Camera proximity | Off-screen distance | Flag-dependent |
| Mega Man (NES) | Screen entry | Screen exit | Immediate on screen re-entry |

### Object slot limits

**SMB1:** Maximum **5 enemy sprites** simultaneously (slots $1E-$22 for Enemy_State).

**SMW:** **12 sprite slots** total—Division 1 (slots #00-#09) for regular sprites, Division 2 (#10-#11) reserved for goal tape and special sprites. Sprites spawn in descending order into Division 1. **Cluster sprites** (Boo rings) can have up to **20 simultaneously**.

**Sonic:** Object Status Table at $FFD000, with **$40 bytes (64 bytes) per entry**. Initialization bugs in Sonic 1 can cause invalid mapping displays.

### Memory-aware enemy design

Mega Man 1's Fire Storm Shooter spawns every 30 frames at fixed position. Edge-placed spawners can generate **16 instances over 2,255 frames** (35.58 seconds). The Ice Slasher exploit—freezing respawning enemies then scrolling away—permanently removes them due to state corruption.

## Hitbox and collision systems vary from simple to sophisticated

Collision detection ranges from basic bounding boxes to Sonic's multi-sensor raycasting system.

### Player hitbox dimensions

| Character | Standing | Ducking/Rolling | Special |
|-----------|----------|-----------------|---------|
| Sonic | ~19×39 pixels | Reduced height | Ducking doesn't reduce hitbox |
| Small Mario | 16×16 | N/A | Collision uses center point |
| Big Mario | 16×32 | 16×16 crouching | N/A |
| Mega Man | ~16×24 (1×1.5 blocks) | ~24×16 sliding | Origin at feet |
| Kirby | ~16×16-20×20 | Larger when puffed | Round approximation |
| Simon | ~32×32 (2×2 tiles) | N/A | Simplified rectangle |

### Collision methods

**Sonic:** Sensor raycasting from midpoint to hitbox edges in movement direction. Floor ignored while jumping up (Y-speed < 0). Objects can be passed through if midpoint is beyond object edge—critical for speed tech exploits.

**Mario:** AABB (Axis-Aligned Bounding Box) for all interactions. Stomp detection checks if Mario is moving downward during collision—a known glitch allows stomping enemies above Mario near jump apex.

**Mega Man:** AABB rectangle collision with tile-based environment detection. Hitbox lookup tables stored in ROM.

### Invincibility frame durations

| Game | Post-Damage iFrames | Notes |
|------|---------------------|-------|
| Sonic 1 | Several seconds (flashing) | **Spikes ignore iFrames** (fixed in REV 01) |
| SMB1 | Via `InjuryTimer` ($079E) | Star power: ~240 frames (4 seconds) |
| SMW | ~120 frames (2 seconds) | Flashing sprite effect |
| Mega Man | ~60-120 frames (1-2 seconds) | MM1 spikes ignore iFrames |
| Castlevania | ~96 frames (1.6 seconds) | Timer at $005B starts at 48, decrements every 2 frames |
| SCIV | 80 frames | RAM address $0000bc |

## Knockback mechanics reinforce game identity

Knockback design reflects each franchise's philosophy toward player punishment and recovery.

**Castlevania (NES):** The most punishing system. Simon is knocked backward **2 tiles (32 pixels)** with a fixed arc rising **14 Y coordinates**. Knockback speed matches walking speed (1 px/frame). Combined with no air control, this frequently results in pit deaths—an intentional design choice emphasizing enemy placement awareness.

**Sonic:** Rings scatter on hit; player bounces but retains position. Momentum largely preserved. The system punishes carelessness without creating compounding punishment.

**Mario:** Bounce backward and up when hit while powered up. Powerdown sequence via `PlayerChangeSizeFlag` ($070B). Relatively short knockback distance with quick recovery.

**Mega Man X:** Brief control loss with knockback direction away from damage source. Critically, **grabbing a wall during damage cancels the knockback animation**—a technique essential for speedrunning.

---

## Level streaming uses chunk-based architectures

Memory limitations required sophisticated level streaming, with all games using hierarchical tile systems.

### Tile hierarchy

| Level | Sonic 1 | Sonic 2+ | Mario | Mega Man |
|-------|---------|----------|-------|----------|
| Base tile | 8×8 | 8×8 | 8×8 | 8×8 |
| Block | 16×16 | 16×16 | 16×16 (metatile) | 16×16 (metatile) |
| Chunk | 256×256 | 128×128 | N/A | Screen (256×240) |

**Sonic's plane system:** Plane A (foreground/level) and Plane B (background) can both have blocks appear in front of or behind the character, enabling parallax and visual layering.

**Compression formats:** Sonic 1 uses Nemesis format; Sonic 2+ switched to Kosinski for faster loading. SMB1 uses run-length encoding for repeated objects.

**SMB3 metatile banks:** 11 banks with 256 slots each. Background tiles stored in 2×2 CHR metatile format (NW, SW, NE, SE corners).

### Screen transitions

**Mega Man:** Discrete screen-by-screen scrolling. Transition triggered at screen edge. Boss doors initiate animation sequence → camera scroll → room lock → boss spawn.

**Castlevania 3:** Tiles load in **32×32 pixel block columns** on alternating frames. Two failsafes prevent visual glitches: character continues moving 1 frame after D-pad release, and tiles load 1 additional frame after stopping.

---

## Boss AI follows state machine patterns with health-based phases

Boss behavior across all examined games uses finite state machines with phase transitions triggered by health thresholds or timers.

### Pattern structures

**Sonic:** Eggman bosses typically require **8 hits**. A Sonic 1 integer overflow bug makes hitting the boss twice on the "last hit" require 255 additional hits. Phase transitions based on hit count or timer.

**Mario (SMB1 Bowser):** `BowserHitPoints` at $0483 (typically 5 fireballs). `BowserFireBreathTimer` ($0790) controls flame timing. `MaxRangeFromOrigin` ($06DC) limits movement range. Alternates jump and fire breath attacks.

**Mega Man:** Robot Masters cycle through defined jump/shoot/special attack sequences. Weakness weapons deal increased damage. Standard boss invincibility of ~60 frames post-hit, with notable exceptions:
- Spark Mandrill: Remains vulnerable while frozen from Shotgun Ice
- Crush Crawfish (X3): 120 frames vulnerable during Triad Thunder stun
- Wire Sponge (X2): Extended invincibility during lightning if HP < 10

### Boss HP storage

Mega Man X stores enemy HP at sequential addresses: 7E0E8F (Enemy 1), 7E0ECF (Enemy 2), etc. X2/X3: 7E0D3F, 7E0D7F, 7E0DBF, 7E0DFF, 7E0E3F for enemies 1-5. No dedicated boss HP meter exists—bosses use standard enemy slots.

---

## Input handling separates responsive games from frustrating ones

Input systems implement buffering, coyote time, and charge mechanics to bridge the gap between player intention and on-screen action.

### Coyote time (grace period after leaving platform)

**Mario series:** Confirmed present in SMB1 and SMW. Typical window of **4-6 frames** allows jumping briefly after leaving a platform edge. Named after Wile E. Coyote cartoon physics.

**Sonic:** No traditional coyote time documented. However, jump can execute on the landing frame (treating slope angle as 0 degrees), creating similar responsiveness.

**Mega Man/Castlevania:** No coyote time—jump must be initiated while grounded.

### Input buffering

Mario games store jump inputs briefly, executing when possible. This prevents "missed jump" frustration from slightly early button presses. Mega Man has limited buffering, with actions processing on the frame input is detected.

### Charge mechanics

**Mega Man X charge timing (countdown from max):**
- 149 frames → Blue charge ready
- 79 frames → Yellow charge ready
- 1 frame → Pink/max charge ready

**Mega Man X2/X3 (counting up):**
- 20 frames → Blue
- 80 frames → Yellow
- 140 frames → Pink level 1
- 200 frames → Pink level 2/max

**Sonic Spin Dash:** Button presses can occur on consecutive frames. Each press adds **0.5 px/frame** to base speed of 8 px/frame, with 6 taps achieving maximum (12 px/frame). One frame of rolling deceleration applies before movement begins.

### Special input windows

**Mega Man slide (MM3+):** Down + Jump must be pressed within 1-2 frame tolerance (ideally same frame).

**Mega Man X wall kick:** Can initiate from **0-7 pixels away from wall**. Optimal climb timing: 19-21 frames per wall jump cycle.

**SCIV jump height:** Despite variable timing, jump height remains constant—difference is 1-frame (36 frames air time, 9 pixels lower) versus full jump (40 frames, 45 pixels height).

## Water physics consistently halve key values

Water zones apply consistent modifiers across most games examined.

**Sonic underwater values (halved):**
- Gravity
- Running acceleration
- Running braking/deceleration
- Air acceleration
- Rolling deceleration
- Jump strength reduced to **896 subpixels (3.5 px/frame)** vs normal ~1664

**Mega Man X:** Gravity and terminal velocity halved in water (exception: charged Triad Thunder maintains normal physics).

**Mario:** Swimming areas use reduced gravity with button presses providing upward thrust bursts. Frog Suit in SMB3 provides enhanced underwater control.

## Conclusion: patterns for engine configuration

These classic games reveal several universal patterns suitable for modern engine implementation:

**Fixed-point precision is essential.** Use at minimum 8.8 format (256 subpixels/pixel) for smooth movement. This single change transforms "programmer physics" into professional game feel.

**Gravity should operate in the 0.2–0.4 px/frame² range** at 60fps, with variable jump height achieved by modifying gravity (not initial velocity) based on button hold state. Mario's dual-gravity approach (low while rising with button held, high otherwise) feels most natural.

**Camera lag prevents motion sickness** without sacrificing responsiveness. Implement deadzone regions where the camera doesn't move, with smooth interpolation outside those zones.

**Coyote time (4-6 frames)** dramatically improves perceived fairness. Players remember the jumps they "should have made"; this grace period converts frustration into flow.

**Slot-based object management** remains relevant for performance optimization. Despawn aggressively based on camera distance; respawn based on player proximity to original spawn position.

The Sonic Physics Guide, SMW Central RAM maps, and Data Crystal ROM documentation remain the authoritative sources for implementation details beyond this reference. Values may vary between regional versions (NTSC vs PAL) due to frame rate differences.

---

## Appendix: monolithic reference block (verbatim)

The prompt that introduced this note also included the same material as a single monolithic text block. It is preserved here verbatim for copy/paste into specs.

```text
================================================================================
HIDDEN MECHANICS IN CLASSIC 2D PLATFORMERS (8-BIT & 16-BIT)
================================================================================

PURPOSE
-------
This document enumerates non-obvious, implementation-critical mechanics in
classic 2D platformers. These are the details that are *not visible to players*
but are *decisive* when attempting faithful reproduction.

Target use case: a configurable 2D engine where “Mario-like”, “Sonic-like”,
“Mega Man-like”, etc. behaviors are enabled purely via data and flags, not
hard-coded logic.

--------------------------------------------------------------------------------
GENERAL THEMES ACROSS ALL GAMES
--------------------------------------------------------------------------------

1. Fixed-point math, not floats
   - NES: commonly 1/16 px or 1/256 px subpixels
   - Genesis / SNES: almost always 1/256 px
   - Rendering truncates to integer pixels; collision usually ignores subpixel

2. Axis-separated collision resolution
   - Horizontal resolved first, then vertical (or vice versa)
   - This creates corner clips, wall jumps, zips, and forgiveness

3. Directional collision
   - Floors block downward motion only
   - Ceilings block upward only
   - Walls block lateral only
   - Rarely “solid from all directions”

4. State-driven physics
   - Rolling, sliding, dashing, floating, stairs, ladders, etc.
   - Each state overrides gravity, friction, acceleration

5. Camera-driven logic
   - Enemy spawning, events, bosses often keyed to camera position,
     not player position

--------------------------------------------------------------------------------
SONIC THE HEDGEHOG 1 / 2 / 3 (GENESIS)
--------------------------------------------------------------------------------

CORE IDENTITY
-------------
Momentum-first physics. Speed is conserved. Gravity projects along slopes.
Friction is conditional, not constant.

COORDINATE SYSTEM
-----------------
- Position: 16.8 fixed-point (1 pixel = 256 subpixels)
- Collision uses *pixel position only*
- Subpixel accumulation can push Sonic “through” corners

ACCELERATION & SPEED
--------------------
- Ground acceleration ≈ 0.046875 px/f²
- Ground friction ≈ 0.5 px/f² (only when not rolling)
- Max run speed ≈ 6 px/f
- Rolling downhill can exceed run cap (up to ~16 px/f internally)

JUMPING
-------
- Jump vector is perpendicular to ground slope
- No true variable jump height
- Jump release clamps upward velocity to fixed lower value
- Horizontal drag applied only while ascending

GRAVITY
-------
- Constant gravity ≈ 0.21875 px/f²
- No explicit terminal velocity

SLOPES
------
- Each tile has an angle
- Gravity splits into normal + tangential components
- Downhill acceleration increases speed automatically
- Uphill slows or halts Sonic if speed insufficient

COLLISION
---------
- Sample points: midpoint + feet
- One-way terrain
- Ejection routine pushes Sonic out of solids
- Failure → zips / wraps

CAMERA
------
- Look-ahead based on velocity
- Vertical dead zones
- Many triggers keyed to camera X, not player X

ENEMIES
-------
- Activated by camera proximity
- Deactivated off-screen
- Object count capped

KEY CONFIG FLAGS
----------------
- slope_physics = true
- gravity_projection = surface_normal
- rolling_friction = very_low
- air_control = conditional
- collision_samples = midpoint + feet

--------------------------------------------------------------------------------
SUPER MARIO BROS 1 / 2 / 3 (NES) + SUPER MARIO WORLD (SNES)
--------------------------------------------------------------------------------

CORE IDENTITY
-------------
Acceleration-based but forgiving. Precision platforming with controllable air.

COORDINATE SYSTEM
-----------------
- NES: ~1/16 px subpixels
- SNES: ~1/256 px
- Collision usually ignores subpixel

ACCELERATION
------------
- Accel ≈ 0.05 px/f²
- Decel (friction) ≈ 0.0625 px/f²
- Reverse input applies stronger decel (skid)

JUMPING
-------
- Variable height via gravity modulation
- Hold jump → reduced gravity
- Release → full gravity
- Mid-air control fully enabled

GRAVITY
-------
- ≈ 0.3125 px/f² normal
- Reduced while holding jump
- Terminal velocity capped

AIR CONTROL
-----------
- Full horizontal acceleration while airborne
- No air friction in SMB1
- Slight air friction in SMW

COLLISION
---------
- 16×16 tiles
- One-way head bumps
- Stomp detection prioritized over side hit if falling
- Small implicit “coyote” due to collision sampling

CAMERA
------
- NES: no backscroll
- SNES: free scroll
- Vertical dead zones
- Pipes/doors hard-load sublevels

KEY CONFIG FLAGS
----------------
- variable_jump = true
- air_control = full
- friction_ground = medium
- friction_air = low/none
- backscroll = false/true

--------------------------------------------------------------------------------
MEGA MAN 1–6 (NES) / MEGA MAN X (SNES)
--------------------------------------------------------------------------------

CORE IDENTITY
-------------
Immediate response. No inertia. Precision combat platforming.

COORDINATE SYSTEM
-----------------
- Early NES: 1/16 px
- Later NES: 1/256 px (stepped)
- SNES X: 1/256 px

MOVEMENT
--------
- Instant acceleration
- Fixed walk speed (~1.3 px/f NES)
- Dash (~3.4 px/f X)

JUMPING
-------
- Fixed initial velocity
- Variable height via release
- Gravity ≈ 0.25 px/f²
- No mid-air inertia

AIR CONTROL
-----------
- Full direction reversal instantly
- Jumping slightly reduces horizontal speed in early games

SPECIAL STATES
--------------
- Slide (NES): fixed-duration burst
- Dash (X): momentum-preserving
- Wall slide (X): gravity reduction
- Wall jump (X): velocity reset

COLLISION
---------
- Rigid tile collision
- Ladders override gravity
- Zips via ladder/ceiling edge cases

CAMERA
------
- Screen-by-screen or gated scrolling
- Boss rooms lock camera

KEY CONFIG FLAGS
----------------
- acceleration = infinite
- air_control = full
- gravity = constant
- special_moves = slide/dash/wall

--------------------------------------------------------------------------------
KIRBY (NES / SNES)
--------------------------------------------------------------------------------

CORE IDENTITY
-------------
Forgiveness, freedom, and state explosion.

MOVEMENT
--------
- Slow walk
- Dash via double-tap or state
- Instant accel/decel

JUMPING & FLOATING
------------------
- Normal jump (short)
- Infinite float state
- Float cancels gravity entirely
- Float rise speed fixed and slow

GRAVITY
-------
- Low
- Terminal velocity low

COLLISION
---------
- Smaller hitbox when crouched
- Slide lowers hitbox
- Slopes mostly decorative

ABILITIES
---------
- Each ability overrides physics
- Parasol: reduced gravity
- Stone: gravity increase
- Wheel: high speed, low control

CAMERA
------
- Free backtracking
- Room-based transitions
- Gentle centering

KEY CONFIG FLAGS
----------------
- float_enabled = true
- gravity_profiles = per_state
- ability_overrides_physics = true

--------------------------------------------------------------------------------
CASTLEVANIA (NES / SNES)
--------------------------------------------------------------------------------

CORE IDENTITY
-------------
Commitment, punishment, deterministic jumps.

MOVEMENT
--------
- Fixed walk speed (~1 px/f)
- No acceleration
- No dash (NES)

JUMPING
-------
- NES: fixed arc, no mid-air control
- SNES SCIV: variable height + air control
- Landing stun on high falls (NES)

STAIRS
------
- Separate movement mode
- Lock to diagonal path
- No jumping on NES stairs
- SCIV allows jump on/off

ATTACK LOCK
-----------
- Whip freezes movement (~23 frames)
- Subweapons lock briefly
- SCIV adds directional whip

COLLISION
---------
- Tall hitbox
- Knockback dominates physics
- Damage overrides state

CAMERA
------
- Forward-only progression
- Screen-locked segments
- Boss rooms static

KEY CONFIG FLAGS
----------------
- fixed_jump = true/false
- mid_air_control = false/true
- landing_stun = true/false
- stair_lock = true/false
- attack_locks_movement = true

--------------------------------------------------------------------------------
ENGINE DESIGN TAKEAWAY
--------------------------------------------------------------------------------

These games are not “physics engines”.
They are **collections of state machines with physics overrides**.

To replicate them:

- Use fixed-point math
- Separate movement into states
- Allow gravity, friction, and control to be swapped per state
- Tie spawning and events to camera
- Resolve collision per-axis
- Accept small numerical quirks as features, not bugs

A modern engine that exposes:
- acceleration
- friction
- gravity
- jump impulse
- air control
- slope behavior
- collision sampling points
- camera rules
- spawn rules

…can reproduce *every* one of these games by configuration alone.

================================================================================
```
