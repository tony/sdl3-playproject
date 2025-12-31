#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "visual/Palette.h"
#include "visual/Shapes.h"

namespace Visual {
struct Rules;

// Shape types that can compose a character form
enum class ShapeType { Circle, Ellipse, Capsule, Triangle, Polygon, Rect, PixelGrid };

// A single shape in a character's form
struct FormShape {
  std::string name;
  ShapeType type = ShapeType::Circle;
  std::string anchor;  // skeleton point this shape attaches to
  Vec2 offset{0, 0};   // offset from anchor

  // Shape-specific parameters
  float radius = 0;          // for Circle
  float rx = 0, ry = 0;      // for Ellipse
  Vec2 p1{0, 0}, p2{0, 0};   // for Capsule endpoints (relative to anchor+offset)
  float width = 0;           // for Capsule width
  std::vector<Vec2> points;  // for Triangle/Polygon (relative to anchor+offset)
  float w = 0;               // for Rect
  float h = 0;               // for Rect

  // PixelGrid data (16-bit-style pixel sketches)
  int pixelSize = 1;
  Vec2 pixelPivot{0, 0};  // pivot in pixel units (0,0 = top-left of grid)
  char pixelTransparent = '.';
  std::vector<std::string> pixels;  // rows of chars (top to bottom)
  std::unordered_map<char, ColorRole> pixelRoles;
  std::unordered_map<char, Color> pixelFixed;
  std::unordered_map<char, std::string> pixelColorKeys;

  ColorRole colorRole = ColorRole::Base;
  Color fixedColor{255, 255, 255};  // used when colorRole == Fixed
  std::string colorKey;

  int z = 0;  // z-order for rendering (higher = in front)

  // Optional pose visibility filters
  std::vector<std::string> onlyInPoses;
  std::vector<std::string> hiddenInPoses;

  // Optional variant visibility filters
  std::vector<std::string> onlyInVariants;
  std::vector<std::string> hiddenInVariants;
};

// Skeleton: named anchor points that poses manipulate
struct Skeleton {
  std::unordered_map<std::string, Vec2> anchors;

  Vec2 get(const std::string& name) const {
    auto it = anchors.find(name);
    return it != anchors.end() ? it->second : Vec2{0, 0};
  }
};

// A pose: specific positions for all skeleton anchors + distortion
struct Pose {
  std::string name;
  float lean = 0;  // degrees of forward/back tilt
  std::unordered_map<std::string, Vec2> anchors;
  float squashX = 1.0F;
  float squashY = 1.0F;

  Vec2 getAnchor(const std::string& name, const Skeleton& base) const {
    auto it = anchors.find(name);
    if (it != anchors.end()) {
      return it->second;
    }
    return base.get(name);
  }
};

// Interpolate between two poses
Pose lerpPose(const Pose& a, const Pose& b, float t, const Skeleton& base);

// Animation: sequence of poses over time
struct Animation {
  std::string name;
  float duration = 1.0F;
  bool loop = true;
  std::vector<std::string> poseNames;
  std::vector<float> keyTimes;  // normalized 0-1
  std::string easing = "linear";

  // Velocity-based blending (alternative to time-based)
  bool velocityBlend = false;
  float velocityMin = 0;
  float velocityMax = 300;
};

struct AnchorDynamics {
  float stiffness = 80.0F;
  float damping = 12.0F;
  float maxOffset = 6.0F;
};

struct FormDynamics {
  std::unordered_map<std::string, AnchorDynamics> anchors;
};

struct IkChain {
  std::string root;
  std::string mid;
  std::string end;
  std::string target;  // optional target anchor (defaults to end)
  float lenA = 0.0F;
  float lenB = 0.0F;
  float bendDir = 1.0F;  // 1 or -1 (controls elbow/knee bend)
  bool mirrorWithFacing = true;
};

struct AnchorSpringState {
  Vec2 pos{0, 0};
  Vec2 vel{0, 0};
  bool initialized = false;
};

struct FormRuntime {
  std::string formId;
  std::unordered_map<std::string, AnchorSpringState> anchors;
};

// Complete character form definition
struct CharacterForm {
  std::string id;
  Skeleton skeleton;
  std::vector<FormShape> shapes;
  std::unordered_map<std::string, Pose> poses;
  std::unordered_map<std::string, Animation> animations;
  Palette palette;
  FormDynamics dynamics;
  std::vector<IkChain> ikChains;

  // Get pose by name, or return default idle
  const Pose& getPose(const std::string& name) const;

  // Evaluate animation at time t, return interpolated pose
  Pose evaluateAnimation(const std::string& animName, float time) const;
};

struct FormStyle {
  Palette palette;
  std::unordered_map<std::string, Color> namedColors;
  std::vector<std::string> variants;
};

// Apply spring dynamics to anchored points (secondary motion).
Pose applyDynamics(const CharacterForm& form, const Pose& pose, FormRuntime& runtime, float dt);

// Apply simple 2-bone IK chains (locks end anchors to targets).
Pose applyIk(const CharacterForm& form, const Pose& pose, float facing);

// Character form renderer
class FormRenderer {
 public:
  FormRenderer(ShapeRenderer& shapes, const Rules& rules) : shapes_(shapes), rules_(rules) {}

  // Render a character form at position with given pose
  void render(const CharacterForm& form,
              const Pose& pose,
              Vec2 position,
              float facing,
              float camX,
              float camY,
              const FormStyle* style = nullptr);

  // Render with outline (silhouette-first approach)
  void renderWithOutline(const CharacterForm& form,
                         const Pose& pose,
                         Vec2 position,
                         float facing,
                         float camX,
                         float camY,
                         const FormStyle* style = nullptr);

  // Render afterimage (simplified silhouette)
  void renderAfterimage(const CharacterForm& form,
                        const Pose& pose,
                        Vec2 position,
                        float facing,
                        Color tint,
                        float camX,
                        float camY,
                        const FormStyle* style = nullptr);

 private:
  ShapeRenderer& shapes_;
  const Rules& rules_;

  void renderShape(const FormShape& shape,
                   const Pose& pose,
                   const Skeleton& skeleton,
                   const Palette& palette,
                   const FormStyle* style,
                   Vec2 position,
                   float facing,
                   float scale,
                   float camX,
                   float camY);
};

}  // namespace Visual
