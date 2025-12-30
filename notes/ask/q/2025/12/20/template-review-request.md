# Expert Review Request: Game Mechanic Specification Template

## Context

We're building a C++ SDL3 platformer sandbox that implements characters from classic games (Mega Man X, Sonic, Knuckles, Street Fighter-style inputs, etc.). We want to achieve **frame-accurate recreation** of mechanics from source games.

To do this systematically, we've drafted a **Game Mechanic Specification Template** (attached) that aims to capture ANY game mechanic with enough precision to implement it identically. The template covers:

- Prerequisites (character state, resources, environment)
- Input specification (buttons, sequences, timing windows)
- Activation conditions (exact trigger logic)
- Execution (velocity, physics, phases)
- Termination (end conditions, interrupts, lockouts)
- Momentum preservation
- Edge cases

**We need your expertise to identify gaps, ambiguities, or missing dimensions in this template.**

---

## Motivating Example: A Gap We've Encountered

While implementing Mega Man X2/X3's **dash wall kick**, we hit an ambiguity our template doesn't adequately capture.

### The Scenario

Player is wall sliding (holding toward wall). They want to perform a dash wall kick (jump off wall with dash speed boost).

**Input sequence attempted:**
- Player presses K (dash) then J (jump) in rapid succession (~50-100ms apart)
- Player is holding ← (toward wall on right side)
- Player may have just left the wall (within ~100ms) or still be touching it

**Expected:** Boosted kick away from wall at 210 px/s
**Actual:** Normal wall kick (90 px/s) or no kick at all

### What Our Template Doesn't Capture Well

1. **"Held" vs "Just Pressed" ambiguity**
   - Our template asks if a button should be "held" but doesn't clearly distinguish:
     - `{D}` held for 500ms before jump
     - `{D}` held for 16ms (1 frame) before jump
     - `[D]` pressed on same frame as jump
     - `[D]` pressed 2 frames before jump (is it "held" on jump frame?)

2. **State transition timing**
   - When exactly does "wall contact" begin/end?
   - If I leave the wall and press jump within 50ms, am I "wall jumping" or "air jumping"?
   - How does the game determine which state I'm in at input time?

3. **Multi-button sequence atomicity**
   - If I press D then J 50ms apart, what's the state on each frame?
   - Frame 0: D pressed (not yet held?)
   - Frame 1: D held, J not yet pressed
   - Frame 3: D held, J pressed ← Is THIS the trigger frame?
   - Does the trigger check D's "held" state or "was pressed recently" state?

4. **Input causality direction**
   - Does "dash + jump triggers dash kick" mean:
     - Check jump first, then check if dash is active? OR
     - Check dash first, then check if jump was pressed? OR
     - Check both simultaneously on same frame?

5. **Near-miss windows**
   - What if I press jump 1 frame AFTER leaving wall contact?
   - What if I press dash 1 frame AFTER pressing jump?
   - Are there any "coyote time" style grace periods?

---

## Questions for Template Improvement

### A. Input State Model

Our template uses `[X]` for press and `{X}` for hold, but this may be insufficient.

**Questions:**
1. What additional input states should we track? (e.g., "pressed this frame", "held for N frames", "released this frame", "pressed within last N frames")
2. How should we specify the **exact frame** a condition is evaluated relative to input events?
3. Should we have a formal model for "button age" (frames since press)?

### B. State Transition Precision

**Questions:**
1. What's the best way to specify the **boundary conditions** for states like "wall contact"?
   - Pixel distance threshold?
   - Frame-precise entry/exit events?
   - Grace periods on exit?
2. How should we capture **state overlap** (e.g., "wall sliding" vs "falling near wall")?
3. Should states have explicit "entry frame" and "exit frame" semantics?

### C. Compound Input Sequences

**Questions:**
1. For multi-button sequences, how should we specify which button's timing is the "anchor"?
2. How do we capture "A must be active when B is pressed" vs "A and B pressed within N frames"?
3. Should we have notation for "button A starts the window, button B must occur within that window"?

### D. Evaluation Order

**Questions:**
1. Should the template explicitly specify the **order** conditions are checked?
2. How do we capture priority when multiple mechanics could trigger?
3. Is there a standard model (e.g., "all inputs processed, then all state checks, then all triggers") we should adopt?

### E. Edge Case Coverage

**Questions:**
1. What categories of edge cases do fighting games / platformers typically need to handle?
2. Are there standard "corner case patterns" we should have checkboxes for?
3. How do professional game specs handle "frame-perfect" vs "lenient" input windows?

### F. Missing Dimensions

**Questions:**
1. What major categories are we missing entirely?
2. Are there game genres (fighting, rhythm, action) with mechanics our template can't express?
3. What do professional game design documents include that we're missing?

---

## Specific Clarifications Needed

To help us understand where our template fails, please help us answer these **specific** questions about X2/X3 dash wall kick:

1. On the frame that triggers a dash wall kick, what is the **exact** state of the dash button?
   - [ ] Must be held (pressed on a previous frame, still down)
   - [ ] Can be pressed this same frame
   - [ ] Must have been held for minimum N frames
   - [ ] Other: ___

2. If dash is pressed on frame 0 and jump is pressed on frame 3, which frame triggers the kick?
   - [ ] Frame 0 (dash starts it)
   - [ ] Frame 3 (jump triggers it, dash state is checked)
   - [ ] Neither (too far apart)
   - [ ] Other: ___

3. What happens if the player presses dash **after** pressing jump (within a few frames)?
   - [ ] Normal kick (too late)
   - [ ] Converts to dash kick
   - [ ] Depends on specific timing: ___

4. If the player leaves wall contact and presses jump within 50ms:
   - [ ] No wall kick possible (must be touching)
   - [ ] Wall kick still triggers (grace period)
   - [ ] Depends on: ___

These answers will help us understand what template fields we need to capture this precisely.

---

## Deliverables Requested

1. **Template Critique:** What's missing, ambiguous, or over-specified in our template?
2. **Notation Suggestions:** Better ways to express input sequences and timing?
3. **Standard Patterns:** Common mechanic patterns we should have pre-built sections for?
4. **Example Fill-in:** If possible, fill in the template for ONE mechanic (your choice) to show where it works and where it breaks down.

---

## Attached

- `mechanic_spec_template.md` - Our current template (v1.0)

Thank you for your expertise. Our goal is a template that can specify any mechanic with zero ambiguity.
