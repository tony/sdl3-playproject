# Game Mechanic Specification Template v1.0

## Purpose

This template provides a formal, exhaustive specification format for documenting game mechanics with frame-perfect precision. It is designed to capture ANY action in ANY game genre (platformers, fighting games, action games) with enough detail to implement identical behavior.

**Project Context:** This is for a C++ SDL3 platformer sandbox that implements characters from various games (Mega Man X, Sonic, Kirby, Knuckles, etc.). We run at 60fps (16.67ms/frame). Our input system tracks: pressed (edge), held (state), released (edge). Characters have configurable physics and actions via TOML files.

---

# MECHANIC SPECIFICATION: [Name]

## 1. Identity

| Field | Value |
|-------|-------|
| **Mechanic Name** | (e.g., "Dash Wall Kick", "Shoryuken", "Spin Dash") |
| **Source Game(s)** | (e.g., "Mega Man X2/X3", "Street Fighter II") |
| **Character(s)** | (e.g., "X, Zero", "Ryu, Ken") |
| **Category** | [ ] Movement | [ ] Attack | [ ] Defense | [ ] Special | [ ] Cancel |
| **Complexity** | [ ] Basic | [ ] Intermediate | [ ] Advanced | [ ] Expert |

---

## 2. Prerequisites (What must be true BEFORE input)

### 2.1 Character State Requirements

| Requirement | Value | Notes |
|-------------|-------|-------|
| **Grounded?** | [ ] Yes | [ ] No | [ ] Either | |
| **Airborne?** | [ ] Yes | [ ] No | [ ] Either | |
| **Wall Contact?** | [ ] Left | [ ] Right | [ ] Either | [ ] None | [ ] N/A | |
| **Ceiling Contact?** | [ ] Yes | [ ] No | [ ] N/A | |
| **Minimum Height AGL** | _____ pixels | (above ground level, if applicable) |
| **Velocity Constraints** | vx: _____ | vy: _____ | (min/max ranges) |
| **Required Prior State** | (e.g., "must be in wall slide", "must be crouching") |
| **Prohibited Prior States** | (e.g., "not during hitstun", "not during dash recovery") |

### 2.2 Resource Requirements

| Resource | Required Value | Consumed on Use? |
|----------|---------------|------------------|
| **Special Meter** | _____ | [ ] Yes | [ ] No |
| **Air Jumps Remaining** | _____ | [ ] Yes | [ ] No |
| **Air Dashes Remaining** | _____ | [ ] Yes | [ ] No |
| **Charges/Ammo** | _____ | [ ] Yes | [ ] No |
| **Cooldown Clear?** | _____ ms | [ ] Yes | [ ] No |
| **Other** | _____ | |

### 2.3 Environmental Requirements

| Requirement | Value |
|-------------|-------|
| **Surface Type** | [ ] Any | [ ] Solid only | [ ] Specific: _____ |
| **Wall Distance Range** | _____ to _____ pixels |
| **Platform Width Min** | _____ pixels |
| **Other Geometry** | |

---

## 3. Input Specification (CRITICAL)

### 3.1 Input Buttons/Directions

Define your input vocabulary first:

| Symbol | Meaning | Our Mapping |
|--------|---------|-------------|
| **←→↑↓** | Cardinal directions | Arrow keys / D-pad |
| **↖↗↙↘** | Diagonals | Combined arrows |
| **A** | Primary action | J key |
| **B** | Secondary action | K key |
| **J** | Jump | Spacebar |
| **D** | Dash | K key (or double-tap) |
| **N** | Neutral (no direction) | No arrow held |

### 3.2 Input Sequence

Describe the exact input sequence. Use this notation:
- `[X]` = Press X (edge trigger)
- `{X}` = Hold X (state)
- `~X` = Release X (edge trigger)
- `X+Y` = Simultaneous (same frame)
- `X,Y` = Sequential (X then Y)
- `X|Y` = Either X or Y
- `(Xms)` = Timing window in milliseconds

**Primary Input Pattern:**
```
Example: {D} + [J]  (Hold dash, press jump)
Example: [↓],[↘],[→]+[A]  (Quarter circle forward + punch)
Example: {←}(2000ms), [→]+[A]  (Charge back 2 sec, forward + punch)

YOUR INPUT: _________________________________________________
```

### 3.3 Timing Windows

| Timing Aspect | Value (ms) | Value (frames @60fps) | Notes |
|---------------|------------|----------------------|-------|
| **Input Buffer (before valid)** | _____ ms | _____ frames | How early can input be queued? |
| **Input Window (total)** | _____ ms | _____ frames | How long is input accepted? |
| **Simultaneous Tolerance** | _____ ms | _____ frames | How close must "same frame" be? |
| **Charge Time (if applicable)** | _____ ms | _____ frames | |
| **Sequence Max Gap** | _____ ms | _____ frames | Max time between sequence steps |

### 3.4 Input State Questions

| Question | Answer |
|----------|--------|
| **Can direction be held before action?** | [ ] Yes | [ ] No | [ ] Required |
| **Can action button be held before direction?** | [ ] Yes | [ ] No |
| **Does release matter?** | [ ] Yes - describe: _____ | [ ] No |
| **Is input consumed (blocks other actions)?** | [ ] Yes | [ ] No |
| **Can input be buffered during other states?** | [ ] Yes - which: _____ | [ ] No |

---

## 4. Activation (The Exact Frame It Triggers)

### 4.1 Trigger Condition

Write the EXACT boolean condition that must be true on a single frame:
```
Example: (walling == true) AND (jump_pressed == true) AND (dash_held == true)
Example: (grounded == true) AND (down_held == true) AND (action1_pressed == true)

YOUR CONDITION: _________________________________________________
```

### 4.2 Priority / Conflict Resolution

If multiple actions could trigger on same frame:

| Conflicting Action | Priority | Resolution |
|--------------------|----------|------------|
| _____ | Higher / Lower / Equal | _____ wins because _____ |
| _____ | Higher / Lower / Equal | _____ wins because _____ |

### 4.3 State at Activation

What is captured/locked at the moment of activation?

| Value | Captured? | Used For |
|-------|-----------|----------|
| **Facing direction** | [ ] Yes | [ ] No | |
| **Input direction** | [ ] Yes | [ ] No | |
| **Current velocity** | [ ] Yes | [ ] No | |
| **Wall side** | [ ] Yes | [ ] No | |
| **Charge level** | [ ] Yes | [ ] No | |
| **Other** | | |

---

## 5. Execution (What Happens During the Action)

### 5.1 Phase Breakdown

| Phase | Duration (ms) | Duration (frames) | Description |
|-------|---------------|-------------------|-------------|
| **Startup** | _____ | _____ | Before effect begins |
| **Active** | _____ | _____ | Effect is happening |
| **Recovery** | _____ | _____ | After effect, before next action |
| **Total** | _____ | _____ | Sum |

### 5.2 Velocity Changes

| Timing | Axis | Operation | Value | Notes |
|--------|------|-----------|-------|-------|
| **Frame 0** | X | Set / Add / Multiply | _____ px/s | |
| **Frame 0** | Y | Set / Add / Multiply | _____ px/s | |
| **Per Frame** | X | Set / Add / Multiply | _____ px/s | |
| **Per Frame** | Y | Set / Add / Multiply | _____ px/s | |
| **On End** | X | Set / Add / Multiply | _____ px/s | |
| **On End** | Y | Set / Add / Multiply | _____ px/s | |

**Velocity Direction:**
| Question | Answer |
|----------|--------|
| **Horizontal relative to?** | [ ] Facing | [ ] Input | [ ] Wall normal | [ ] Fixed |
| **Vertical relative to?** | [ ] Up always | [ ] Input | [ ] Other: _____ |

### 5.3 Physics Modifications During Action

| Property | Modification | Duration |
|----------|--------------|----------|
| **Gravity** | _____ % of normal | _____ ms |
| **Air Resistance** | _____ % of normal | _____ ms |
| **Max Fall Speed** | _____ px/s | _____ ms |
| **Control Authority** | _____ % | _____ ms |

### 5.4 Position/Hitbox Changes

| Change | Value | Duration |
|--------|-------|----------|
| **Position Snap** | _____ pixels toward _____ | Instant |
| **Hitbox Resize** | Width: _____ Height: _____ | _____ ms |
| **Invincibility Frames** | _____ frames | |
| **Intangibility Frames** | _____ frames | |

---

## 6. Termination (What Ends the Action)

### 6.1 Natural End Conditions

| Condition | Behavior on End |
|-----------|-----------------|
| **Duration expires** | Transition to: _____ |
| **Animation completes** | Transition to: _____ |
| **Touches ground** | Transition to: _____ |
| **Touches wall** | Transition to: _____ |
| **Velocity reaches zero** | Transition to: _____ |

### 6.2 Interrupt Conditions

| Interrupting Action | Window (frames) | Preserves Velocity? |
|---------------------|-----------------|---------------------|
| **Jump** | _____ | [ ] Yes | [ ] No | [ ] Partial |
| **Attack** | _____ | [ ] Yes | [ ] No | [ ] Partial |
| **Dash** | _____ | [ ] Yes | [ ] No | [ ] Partial |
| **Block** | _____ | [ ] Yes | [ ] No | [ ] Partial |
| **Taking Damage** | Always | [ ] Yes | [ ] No | [ ] Partial |
| **Other: _____** | _____ | [ ] Yes | [ ] No | [ ] Partial |

### 6.3 Lockouts After Action

| Action | Lockout Duration | Notes |
|--------|-----------------|-------|
| **Same action again** | _____ ms | |
| **Wall grab** | _____ ms | |
| **Dash** | _____ ms | |
| **Other: _____** | _____ ms | |

---

## 7. Momentum & Velocity Preservation

### 7.1 Velocity Inheritance

| From State | vx Preserved | vy Preserved | Modification |
|------------|--------------|--------------|--------------|
| **Standing** | _____ % | _____ % | |
| **Running** | _____ % | _____ % | |
| **Falling** | _____ % | _____ % | |
| **Dashing** | _____ % | _____ % | |
| **Wall sliding** | _____ % | _____ % | |
| **Other action** | _____ % | _____ % | |

### 7.2 Velocity Persistence After Action

| Question | Answer |
|----------|--------|
| **Does velocity persist after action ends?** | [ ] Yes | [ ] No | [ ] Until: _____ |
| **What cancels preserved velocity?** | [ ] Ground touch | [ ] Wall touch | [ ] New action | [ ] Other: _____ |
| **Is there velocity decay?** | [ ] No | [ ] Yes: _____ px/s² |

---

## 8. Edge Cases & Special Interactions

### 8.1 Corner Cases

| Scenario | Expected Behavior |
|----------|-------------------|
| **Input 1 frame before valid state** | |
| **Input 1 frame after valid state ends** | |
| **Both walls touched simultaneously** | |
| **Ground and wall touched same frame** | |
| **Action interrupted frame 1** | |
| **Ceiling collision during action** | |
| **Exiting screen boundaries** | |
| **Zero velocity edge case** | |

### 8.2 Interaction with Other Mechanics

| Other Mechanic | Interaction |
|----------------|-------------|
| **Double jump** | |
| **Air dash** | |
| **Damage/hitstun** | |
| **Power-ups/upgrades** | |
| **Underwater/special zones** | |

---

## 9. Visual/Audio Feedback

| Feedback Type | Trigger Point | Description |
|---------------|---------------|-------------|
| **Animation Start** | Frame _____ | |
| **Animation Peak** | Frame _____ | |
| **Sound Effect** | Frame _____ | |
| **Particle Effect** | Frame _____ | |
| **Screen Shake** | Frame _____ | Intensity: _____ |
| **Hit Flash** | On hit | |

---

## 10. Reference Examples

### 10.1 Successful Execution

Describe a frame-by-frame example of successful execution:

```
Frame -10: Player approaching wall, holding right, dash held
Frame -5:  Wall contact detected, wall slide begins
Frame 0:   Jump pressed while dash still held → DASH WALL KICK triggers
Frame 1:   vx set to -210 px/s (away from wall), vy set to -300 px/s (up)
Frame 2+:  Normal air physics, velocity preserved
Frame 60:  Ground contact, velocity zeroed, action complete
```

### 10.2 Failed Execution (Edge Case)

Describe a frame-by-frame example of a common failure:

```
Frame -5:  Wall contact, wall slide active
Frame 0:   Dash pressed (edge), Jump pressed (edge) - SAME FRAME
Frame 0:   Dash is "pressed" not "held" - does this count?
Result:    [SPECIFY: Should this work? Why/why not?]
```

---

## 11. Implementation Checklist

- [ ] Prerequisites validated correctly
- [ ] Input detection matches specification
- [ ] Timing windows implemented
- [ ] Velocity values correct
- [ ] Direction handling correct
- [ ] Termination conditions work
- [ ] Momentum preservation works
- [ ] Edge cases handled
- [ ] Visual feedback synced
- [ ] Test cases pass

---

## 12. Open Questions for Expert

List any ambiguities that need clarification:

1. _____
2. _____
3. _____

---

# EXAMPLES

Below are partially filled examples for reference.

---

## Example A: Mega Man X2/X3 Dash Wall Kick

### 1. Identity

| Field | Value |
|-------|-------|
| **Mechanic Name** | Dash Wall Kick |
| **Source Game(s)** | Mega Man X2, Mega Man X3 |
| **Character(s)** | X, Zero |
| **Category** | [x] Movement |
| **Complexity** | [x] Intermediate |

### 3.2 Input Sequence

```
{D} + [J] while wall sliding
(Hold dash, press jump while in contact with wall)
```

### 4.1 Trigger Condition

```
(wall_contact == true) AND (jump_pressed == true) AND (dash_held == true)
```

### 5.2 Velocity Changes

| Timing | Axis | Operation | Value | Notes |
|--------|------|-----------|-------|-------|
| **Frame 0** | X | Set | 210 px/s | Away from wall |
| **Frame 0** | Y | Set | -300 px/s | Upward |

### 7.2 Velocity Persistence

- Velocity persists until: ground contact, wall contact, or air dash used

---

## Example B: Street Fighter Shoryuken

### 3.2 Input Sequence

```
[→], [↓], [↘]+[P]
(Forward, Down, Down-Forward + Punch - within 15 frames)
```

### 5.1 Phase Breakdown

| Phase | Duration (frames) |
|-------|-------------------|
| **Startup** | 3 |
| **Active** | 12 |
| **Recovery** | 25 |

---

## Example C: Sonic Spin Dash

### 3.2 Input Sequence

```
{↓} + [A]...([A])...([A])... ~↓
(Hold down + tap action repeatedly to charge, release down to launch)
```

### 5.2 Velocity Changes

| Timing | Axis | Operation | Value |
|--------|------|-----------|-------|
| **On Release** | X | Set | 200-900 px/s (based on charge) |

---

# Template Version History

- v1.0: Initial comprehensive template
