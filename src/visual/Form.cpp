#include "visual/Form.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <numbers>

#include "visual/Rules.h"

namespace Visual {

// Interpolate between two poses
Pose lerpPose(const Pose& a, const Pose& b, float t, const Skeleton& base) {
  Pose result;
  static constexpr float kHalf = 0.5F;
  result.name = t < kHalf ? a.name : b.name;
  result.lean = a.lean + (b.lean - a.lean) * t;
  result.squashX = a.squashX + (b.squashX - a.squashX) * t;
  result.squashY = a.squashY + (b.squashY - a.squashY) * t;

  // Interpolate all anchors from both poses
  for (const auto& [name, pos] : base.anchors) {
    Vec2 posA = a.getAnchor(name, base);
    Vec2 posB = b.getAnchor(name, base);
    result.anchors[name] = lerp(posA, posB, t);
  }

  return result;
}

// Default idle pose
static const Pose kDefaultPose{"idle", 0, {}, 1.0F, 1.0F};

const Pose& CharacterForm::getPose(const std::string& name) const {
  auto it = poses.find(name);
  if (it != poses.end()) {
    return it->second;
  }

  // Try "idle" as fallback
  it = poses.find("idle");
  if (it != poses.end()) {
    return it->second;
  }

  return kDefaultPose;
}

Pose CharacterForm::evaluateAnimation(const std::string& animName, float time) const {
  auto it = animations.find(animName);
  if (it == animations.end()) {
    return getPose("idle");
  }

  const Animation& anim = it->second;
  if (anim.poseNames.empty()) {
    return getPose("idle");
  }

  // Normalize time
  float normalizedTime = time / anim.duration;
  if (anim.loop) {
    normalizedTime = normalizedTime - std::floor(normalizedTime);
  } else {
    normalizedTime = std::clamp(normalizedTime, 0.0F, 1.0F);
  }

  // Find keyframe segment
  if (anim.keyTimes.size() != anim.poseNames.size()) {
    // Evenly spaced keyframes
    float segment = 1.0F / static_cast<float>(anim.poseNames.size());
    int idx = static_cast<int>(normalizedTime / segment);
    idx = std::clamp(idx, 0, static_cast<int>(anim.poseNames.size()) - 1);

    if (idx >= static_cast<int>(anim.poseNames.size()) - 1) {
      return getPose(anim.poseNames.back());
    }

    float localT = (normalizedTime - static_cast<float>(idx) * segment) / segment;
    return lerpPose(getPose(anim.poseNames[idx]), getPose(anim.poseNames[idx + 1]), localT,
                    skeleton);
  }

  // Custom keyframe times
  for (size_t i = 0; i + 1 < anim.keyTimes.size(); ++i) {
    if (normalizedTime >= anim.keyTimes[i] && normalizedTime < anim.keyTimes[i + 1]) {
      float localT =
          (normalizedTime - anim.keyTimes[i]) / (anim.keyTimes[i + 1] - anim.keyTimes[i]);
      return lerpPose(getPose(anim.poseNames[i]), getPose(anim.poseNames[i + 1]), localT, skeleton);
    }
  }

  return getPose(anim.poseNames.back());
}

Pose applyDynamics(const CharacterForm& form, const Pose& pose, FormRuntime& runtime, float dt) {
  Pose out = pose;
  if (runtime.formId != form.id) {
    runtime.formId = form.id;
    runtime.anchors.clear();
  }

  for (const auto& [name, dyn] : form.dynamics.anchors) {
    Vec2 target = pose.getAnchor(name, form.skeleton);
    AnchorSpringState& state = runtime.anchors[name];
    if (!state.initialized) {
      state.initialized = true;
      state.pos = target;
      state.vel = {0, 0};
    }

    Vec2 delta = target - state.pos;
    Vec2 accel = delta * dyn.stiffness - state.vel * dyn.damping;
    state.vel = state.vel + accel * dt;
    state.pos = state.pos + state.vel * dt;

    if (dyn.maxOffset > 0.0F) {
      Vec2 offset = state.pos - target;
      float len = length(offset);
      if (len > dyn.maxOffset) {
        Vec2 dir = (len > kVectorEpsilon) ? (offset * (1.0F / len)) : Vec2{0, 0};
        state.pos = target + dir * dyn.maxOffset;
        static constexpr float kVelocityDecay = 0.5F;
        state.vel = state.vel * kVelocityDecay;
      }
    }

    out.anchors[name] = state.pos;
  }

  return out;
}

Pose applyIk(const CharacterForm& form, const Pose& pose, float facing) {
  if (form.ikChains.empty()) {
    return pose;
  }

  Pose out = pose;
  const float facingSign = (facing < 0.0F) ? -1.0F : 1.0F;

  for (const IkChain& chain : form.ikChains) {
    Vec2 root = pose.getAnchor(chain.root, form.skeleton);
    Vec2 target = chain.target.empty() ? pose.getAnchor(chain.end, form.skeleton)
                                       : pose.getAnchor(chain.target, form.skeleton);

    float lenA = chain.lenA;
    float lenB = chain.lenB;
    if (lenA <= 0.0F) {
      lenA = length(form.skeleton.get(chain.mid) - form.skeleton.get(chain.root));
    }
    if (lenB <= 0.0F) {
      lenB = length(form.skeleton.get(chain.end) - form.skeleton.get(chain.mid));
    }

    if (lenA <= kVectorEpsilon || lenB <= kVectorEpsilon) {
      continue;
    }

    Vec2 toTarget = target - root;
    float dist = length(toTarget);
    if (dist < kVectorEpsilon) {
      continue;
    }

    const float minReach = std::fabs(lenA - lenB);
    const float maxReach = lenA + lenB;
    dist = std::clamp(dist, minReach, maxReach);

    Vec2 dir = normalize(toTarget);
    float baseAngle = std::atan2(dir.y, dir.x);
    static constexpr float kTwo = 2.0F;
    float cosA = (lenA * lenA + dist * dist - lenB * lenB) / (kTwo * lenA * dist);
    cosA = std::clamp(cosA, -1.0F, 1.0F);
    float angleA = std::acos(cosA);

    float bend = chain.bendDir;
    if (chain.mirrorWithFacing) {
      bend *= facingSign;
    }

    float midAngle = baseAngle + angleA * bend;
    Vec2 mid{root.x + std::cos(midAngle) * lenA, root.y + std::sin(midAngle) * lenA};
    Vec2 end{root.x + dir.x * dist, root.y + dir.y * dist};

    out.anchors[chain.mid] = mid;
    out.anchors[chain.end] = end;
  }

  return out;
}

static bool poseInList(const std::vector<std::string>& list, const std::string& poseName) {
  return std::find(list.begin(), list.end(), poseName) != list.end();
}

static bool variantsMatch(const std::vector<std::string>& shapeKeys,
                          const std::vector<std::string>& variants) {
  return std::ranges::any_of(shapeKeys, [&](const std::string& key) {
    return std::find(variants.begin(), variants.end(), key) != variants.end();
  });
}

static bool shouldRenderShape(const FormShape& shape, const Pose& pose, const FormStyle* style) {
  if (!shape.onlyInPoses.empty() && !poseInList(shape.onlyInPoses, pose.name)) {
    return false;
  }
  if (!shape.hiddenInPoses.empty() && poseInList(shape.hiddenInPoses, pose.name)) {
    return false;
  }

  const std::vector<std::string> empty;
  const std::vector<std::string>& variants = (style != nullptr) ? style->variants : empty;

  if (!shape.onlyInVariants.empty() && !variantsMatch(shape.onlyInVariants, variants)) {
    return false;
  }
  if (!shape.hiddenInVariants.empty() && variantsMatch(shape.hiddenInVariants, variants)) {
    return false;
  }

  return true;
}

static Vec2 applyPoseLean(Vec2 anchor, float leanDegrees, float facing) {
  if (leanDegrees == 0.0F) {
    return anchor;
  }

  static constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0F;
  const float signedDeg = leanDegrees * ((facing < 0.0F) ? -1.0F : 1.0F);
  const float rad = signedDeg * kDegToRad;
  const float c = std::cos(rad);
  const float s = std::sin(rad);
  const float x = anchor.x;
  const float y = anchor.y;
  return {x * c - y * s, x * s + y * c};
}

static Palette resolveShapePalette(const FormShape& shape,
                                   const Palette& palette,
                                   const FormStyle* style) {
  Palette shapePalette = palette;
  if (style != nullptr && !shape.colorKey.empty() && shape.colorRole != ColorRole::Fixed &&
      shape.colorRole != ColorRole::Outline) {
    auto it = style->namedColors.find(shape.colorKey);
    if (it != style->namedColors.end()) {
      shapePalette = buildPalette(it->second, it->second, gRules);
    }
  }
  return shapePalette;
}

static void renderCircle(ShapeRenderer& shapes,
                         const FormShape& shape,
                         const Pose& pose,
                         float screenX,
                         float screenY,
                         float scale,
                         Color color) {
  float r = shape.radius * scale;
  static constexpr float kAverageSquash = 0.5F;
  r *= (pose.squashX + pose.squashY) * kAverageSquash;
  shapes.fillCircle(screenX, screenY, r, color);
}

static void renderEllipse(ShapeRenderer& shapes,
                          const FormShape& shape,
                          const Pose& pose,
                          float screenX,
                          float screenY,
                          float scale,
                          Color color) {
  float rx = shape.rx * scale * pose.squashX;
  float ry = shape.ry * scale * pose.squashY;
  shapes.fillEllipse(screenX, screenY, rx, ry, color);
}

static void renderCapsule(ShapeRenderer& shapes,
                          const FormShape& shape,
                          const Pose& pose,
                          float screenX,
                          float screenY,
                          float scale,
                          float facing,
                          Color color) {
  Vec2 p1 = shape.p1 * Vec2{facing, 1.0F} * Vec2{pose.squashX, pose.squashY};
  Vec2 p2 = shape.p2 * Vec2{facing, 1.0F} * Vec2{pose.squashX, pose.squashY};
  Vec2 worldP1{screenX + p1.x * scale, screenY + p1.y * scale};
  Vec2 worldP2{screenX + p2.x * scale, screenY + p2.y * scale};
  shapes.fillCapsule(worldP1, worldP2, shape.width * scale, color);
}

static void renderTriangle(ShapeRenderer& shapes,
                           const FormShape& shape,
                           const Pose& pose,
                           float screenX,
                           float screenY,
                           float scale,
                           float facing,
                           Color color) {
  if (shape.points.size() < 3) {
    return;
  }

  Vec2 squash{pose.squashX, pose.squashY};
  Vec2 a = {screenX + shape.points[0].x * facing * squash.x * scale,
            screenY + shape.points[0].y * squash.y * scale};
  Vec2 b = {screenX + shape.points[1].x * facing * squash.x * scale,
            screenY + shape.points[1].y * squash.y * scale};
  Vec2 c = {screenX + shape.points[2].x * facing * squash.x * scale,
            screenY + shape.points[2].y * squash.y * scale};
  shapes.fillTriangle(a, b, c, color);
}

static void renderPolygon(ShapeRenderer& shapes,
                          const FormShape& shape,
                          const Pose& pose,
                          float screenX,
                          float screenY,
                          float scale,
                          float facing,
                          Color color) {
  if (shape.points.size() < 3) {
    return;
  }

  std::vector<Vec2> transformed;
  transformed.reserve(shape.points.size());
  Vec2 squash{pose.squashX, pose.squashY};
  for (const Vec2& p : shape.points) {
    transformed.push_back(
        {screenX + p.x * facing * squash.x * scale, screenY + p.y * squash.y * scale});
  }
  shapes.fillPolygon(transformed, color);
}

static void renderRect(ShapeRenderer& shapes,
                       const FormShape& shape,
                       const Pose& pose,
                       float screenX,
                       float screenY,
                       float scale,
                       Color color) {
  float rw = shape.w * scale * pose.squashX;
  float rh = shape.h * scale * pose.squashY;
  static constexpr float kHalf = 0.5F;
  shapes.fillRect(screenX - rw * kHalf, screenY - rh * kHalf, rw, rh, color);
}

static bool resolvePixelColor(char pixel,
                              const FormShape& shape,
                              const Palette& palette,
                              const FormStyle* style,
                              bool forceSolid,
                              Color solidColor,
                              Color& outColor) {
  if (forceSolid) {
    outColor = solidColor;
    return true;
  }

  if (style != nullptr) {
    auto itKey = shape.pixelColorKeys.find(pixel);
    if (itKey != shape.pixelColorKeys.end()) {
      auto itNamed = style->namedColors.find(itKey->second);
      if (itNamed != style->namedColors.end()) {
        outColor = itNamed->second;
        return true;
      }
    }
  }

  auto itFixed = shape.pixelFixed.find(pixel);
  if (itFixed != shape.pixelFixed.end()) {
    outColor = itFixed->second;
    return true;
  }

  bool hasRole = false;
  ColorRole role = ColorRole::Base;
  auto itRole = shape.pixelRoles.find(pixel);
  if (itRole != shape.pixelRoles.end()) {
    role = itRole->second;
    hasRole = true;
  } else {
    switch (pixel) {
      case 'b':
        role = ColorRole::Base;
        hasRole = true;
        break;
      case 'l':
        role = ColorRole::Light;
        hasRole = true;
        break;
      case 'd':
        role = ColorRole::Dark;
        hasRole = true;
        break;
      case 'a':
        role = ColorRole::Accent;
        hasRole = true;
        break;
      case 'o':
        role = ColorRole::Outline;
        hasRole = true;
        break;
      case 'f':
        role = ColorRole::Fixed;
        hasRole = true;
        break;
      default:
        break;
    }
  }

  if (!hasRole) {
    return false;
  }

  outColor = (role == ColorRole::Fixed) ? shape.fixedColor
                                        : resolveColorRole(role, palette, shape.fixedColor);
  return true;
}

static void renderPixelGrid(ShapeRenderer& shapes,
                            const FormShape& shape,
                            const Palette& palette,
                            const FormStyle* style,
                            float screenX,
                            float screenY,
                            float scale,
                            float facing,
                            const Pose& pose) {
  if (shape.pixels.empty()) {
    return;
  }

  int rows = static_cast<int>(shape.pixels.size());
  int cols = 0;
  for (const std::string& row : shape.pixels) {
    cols = std::max(cols, static_cast<int>(row.size()));
  }
  if (rows <= 0 || cols <= 0) {
    return;
  }

  const float pxSize = static_cast<float>(std::max(1, shape.pixelSize)) * scale;
  const Vec2 squash{pose.squashX, pose.squashY};
  const bool forceSolid =
      (shape.colorRole == ColorRole::Outline || shape.colorRole == ColorRole::Fixed);
  Color solidColor = shape.fixedColor;
  if (shape.colorRole == ColorRole::Outline) {
    solidColor = resolveColorRole(ColorRole::Outline, palette, shape.fixedColor);
  }

  for (int y = 0; y < rows; ++y) {
    const std::string& row = shape.pixels[y];
    const int rowLen = static_cast<int>(row.size());
    for (int x = 0; x < rowLen; ++x) {
      char c = row[x];
      if (c == shape.pixelTransparent) {
        continue;
      }

      Color pixelColor{};
      if (!resolvePixelColor(c, shape, palette, style, forceSolid, solidColor, pixelColor)) {
        continue;
      }

      int ix = x;
      if (facing < 0.0F) {
        ix = (cols - 1) - x;
      }

      const float localX = (static_cast<float>(ix) - shape.pixelPivot.x) * pxSize * squash.x;
      const float localY = (static_cast<float>(y) - shape.pixelPivot.y) * pxSize * squash.y;
      shapes.fillRect(screenX + localX, screenY + localY, pxSize * squash.x, pxSize * squash.y,
                      pixelColor);
    }
  }
}

static void renderShapeGeometry(ShapeRenderer& shapes,
                                const FormShape& shape,
                                const Pose& pose,
                                const Palette& palette,
                                const FormStyle* style,
                                float screenX,
                                float screenY,
                                float scale,
                                float facing,
                                Color color) {
  switch (shape.type) {
    case ShapeType::Circle:
      renderCircle(shapes, shape, pose, screenX, screenY, scale, color);
      break;
    case ShapeType::Ellipse:
      renderEllipse(shapes, shape, pose, screenX, screenY, scale, color);
      break;
    case ShapeType::Capsule:
      renderCapsule(shapes, shape, pose, screenX, screenY, scale, facing, color);
      break;
    case ShapeType::Triangle:
      renderTriangle(shapes, shape, pose, screenX, screenY, scale, facing, color);
      break;
    case ShapeType::Polygon:
      renderPolygon(shapes, shape, pose, screenX, screenY, scale, facing, color);
      break;
    case ShapeType::Rect:
      renderRect(shapes, shape, pose, screenX, screenY, scale, color);
      break;
    case ShapeType::PixelGrid:
      renderPixelGrid(shapes, shape, palette, style, screenX, screenY, scale, facing, pose);
      break;
  }
}

void FormRenderer::render(const CharacterForm& form,
                          const Pose& pose,
                          Vec2 position,
                          float facing,
                          float camX,
                          float camY,
                          const FormStyle* style) {
  // Sort shapes by z-order
  std::vector<const FormShape*> sorted;
  sorted.reserve(form.shapes.size());
  for (const auto& shape : form.shapes) {
    sorted.push_back(&shape);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const FormShape* a, const FormShape* b) { return a->z < b->z; });

  // Render each shape
  const Palette& palette = (style != nullptr) ? style->palette : form.palette;
  for (const FormShape* shape : sorted) {
    renderShape(*shape, pose, form.skeleton, palette, style, position, facing, 1.0F, camX, camY);
  }
}

void FormRenderer::renderWithOutline(const CharacterForm& form,
                                     const Pose& pose,
                                     Vec2 position,
                                     float facing,
                                     float camX,
                                     float camY,
                                     const FormStyle* style) {
  // Sort shapes by z-order
  std::vector<const FormShape*> sorted;
  sorted.reserve(form.shapes.size());
  for (const auto& shape : form.shapes) {
    sorted.push_back(&shape);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const FormShape* a, const FormShape* b) { return a->z < b->z; });

  // First pass: render outlines (scaled up)
  float outlineScale = rules_.outlineScale;
  const Palette& palette = (style != nullptr) ? style->palette : form.palette;
  for (const FormShape* shape : sorted) {
    // Create outline version of shape
    FormShape outlineShape = *shape;
    outlineShape.colorRole = ColorRole::Outline;

    renderShape(outlineShape, pose, form.skeleton, palette, style, position, facing, outlineScale,
                camX, camY);
  }

  // Second pass: render fills
  for (const FormShape* shape : sorted) {
    renderShape(*shape, pose, form.skeleton, palette, style, position, facing, 1.0F, camX, camY);
  }
}

void FormRenderer::renderAfterimage(const CharacterForm& form,
                                    const Pose& pose,
                                    Vec2 position,
                                    float facing,
                                    Color tint,
                                    float camX,
                                    float camY,
                                    const FormStyle* style) {
  // Sort shapes by z-order
  std::vector<const FormShape*> sorted;
  sorted.reserve(form.shapes.size());
  for (const auto& shape : form.shapes) {
    sorted.push_back(&shape);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const FormShape* a, const FormShape* b) { return a->z < b->z; });

  // Render all shapes with tint color
  const Palette& palette = (style != nullptr) ? style->palette : form.palette;
  for (const FormShape* shape : sorted) {
    FormShape tintedShape = *shape;
    tintedShape.colorRole = ColorRole::Fixed;
    tintedShape.fixedColor = tint;

    renderShape(tintedShape, pose, form.skeleton, palette, style, position, facing, 1.0F, camX,
                camY);
  }
}

void FormRenderer::renderShape(const FormShape& shape,
                               const Pose& pose,
                               const Skeleton& skeleton,
                               const Palette& palette,
                               const FormStyle* style,
                               Vec2 position,
                               float facing,
                               float scale,
                               float camX,
                               float camY) {
  if (!shouldRenderShape(shape, pose, style)) {
    return;
  }

  // Get anchor position from pose
  Vec2 anchor = pose.getAnchor(shape.anchor, skeleton);

  // Apply squash/stretch
  anchor.x *= pose.squashX;
  anchor.y *= pose.squashY;

  // Apply facing (mirror on X)
  if (facing < 0.0F) {
    anchor.x = -anchor.x;
  }

  // Apply pose lean (rotate around root). Positive lean tilts forward in the facing direction.
  anchor = applyPoseLean(anchor, pose.lean, facing);

  // Calculate world position
  Vec2 worldPos = position + anchor + shape.offset * Vec2{facing, 1.0F};

  // Apply camera offset
  float screenX = worldPos.x - camX;
  float screenY = worldPos.y - camY;

  // Resolve palette (optionally overridden by named colors).
  Palette shapePalette = resolveShapePalette(shape, palette, style);

  // Resolve color
  Color color = resolveColorRole(shape.colorRole, shapePalette, shape.fixedColor);

  // Render based on shape type
  renderShapeGeometry(shapes_, shape, pose, shapePalette, style, screenX, screenY, scale, facing,
                      color);
}

}  // namespace Visual
