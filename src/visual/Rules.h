#pragma once

namespace Visual {

// Global visual rules - the grammar everything obeys
struct Rules {
  // Lighting
  float lightAngle = 315.0F;  // degrees, 0=right, 90=down (top-left light)
  bool highlightWarm = true;  // highlights push toward yellow
  bool shadowCool = true;     // shadows push toward blue

  // Outlines
  bool outlineSilhouetteOnly = true;  // vs all edges
  bool outlineBreaksOnLight = true;   // outline thins on lit side
  float outlineScale = 1.12F;         // how much larger outline shapes are

  // Motion
  float squashStretchMax = 1.3F;  // maximum distortion factor
  bool preserveMass = true;       // width * height stays constant
  float smearThreshold = 400.0F;  // velocity before smear kicks in
  int afterimageCount = 3;
  float afterimageDecay = 0.25F;  // seconds

  // Constraints
  int maxColorsPerObject = 4;
};

// Default + global instance (tweakable at runtime via Debug UI).
inline constexpr Rules kDefaultRules{};
// NOLINTNEXTLINE
inline Rules gRules = kDefaultRules;

}  // namespace Visual
