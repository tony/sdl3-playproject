# Puppet art direction — external expert brief

This brief is extracted from `notes/plan.md` so it can be sent as-is.

## System summary (what exists)

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

## Constraints you must design within

- One shared skeleton across all characters (no per-character skeleton scale yet).
- Recognition must come from proportion cues + 2–3 iconic accessories.
- Prefer separate anchored shapes for hats/quills/helmets/mustache, not baked into torso.
- Pixel-grid motifs should be 6–10 px wide for key cues, not full art.
- Faces are assembled from simple eye/mouth/brow swaps via pose filters.

## Output you should deliver (per character)

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

## Questions for the expert (self-contained)

1. Which anchors must move per character to hit iconic silhouette? Provide relative deltas + priority.
2. Which identity features must be separate accessory shapes vs baked into torso/head grids for 8–12px readability?
3. Provide pixel-grid patterns (size + pivot + ASCII grid + legend) for the 3–5 most iconic details per character.
4. Which z-order relationships are critical to prevent identity breakage?
5. How should base/light/dark/accent roles be allocated per character under top-left lighting? Include named colors if needed.
6. Which face swaps (neutral/smile/angry/surprised/yawn) are mandatory, and what should they look like in minimal pixel-grid terms?
7. Forced choice: A anchor deltas vs B accessory grids vs C face swaps — which single change yields the largest recognizability gain?
8. Should we tie proportions to canonical sprite boxes or keep proportions free and focus on silhouette only?
