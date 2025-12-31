#include "visual/CharacterForms.h"

// Legacy form definitions use aggregate init without listing every optional field.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "util/Paths.h"
#include "visual/FormToml.h"
#include "visual/Palette.h"
#include "visual/Rules.h"
#include "visual/Shapes.h"

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
namespace Visual {

CharacterForm createPuppetForm() {
  CharacterForm form;
  const std::string path = Paths::resolveAssetPath("assets/forms/puppet.toml");
  if (!loadFormFromToml(path.c_str(), form)) {
    form = createFallbackForm();
    form.id = "puppet";
  }
  return form;
}

CharacterForm createFallbackForm() {
  CharacterForm form;
  form.id = "fallback";

  // Soft purple with pink accent - friendly 8-bit ghost/slime vibe
  form.palette = buildPalette(Color::fromHex("9070C0"), Color::fromHex("F080A0"), gRules);

  form.skeleton.anchors = {
      {"root", {0, 0}},    {"body", {0, -10}},   {"eye_l", {-3, -14}},
      {"eye_r", {3, -14}}, {"foot_l", {-3, -1}}, {"foot_r", {3, -1}},
  };

  // Cute blob body - rounded bottom
  form.shapes.push_back(FormShape{
      .name = "body",
      .type = ShapeType::Ellipse,
      .anchor = "body",
      .offset = {0, 0},
      .rx = 8.0F,
      .ry = 10.0F,
      .colorRole = ColorRole::Base,
      .z = 0,
  });

  // Body highlight
  form.shapes.push_back(FormShape{
      .name = "highlight",
      .type = ShapeType::Circle,
      .anchor = "body",
      .offset = {-2, -4},
      .radius = 3.0F,
      .colorRole = ColorRole::Light,
      .z = 1,
  });

  // Left eye
  form.shapes.push_back(FormShape{
      .name = "eye_l_white",
      .type = ShapeType::Ellipse,
      .anchor = "eye_l",
      .offset = {0, 0},
      .rx = 2.5F,
      .ry = 3.0F,
      .colorRole = ColorRole::Fixed,
      .fixedColor = {255, 255, 255},
      .z = 10,
  });

  form.shapes.push_back(FormShape{
      .name = "eye_l_pupil",
      .type = ShapeType::Circle,
      .anchor = "eye_l",
      .offset = {0.5F, 0},
      .radius = 1.5F,
      .colorRole = ColorRole::Fixed,
      .fixedColor = {30, 20, 40},
      .z = 11,
  });

  // Right eye
  form.shapes.push_back(FormShape{
      .name = "eye_r_white",
      .type = ShapeType::Ellipse,
      .anchor = "eye_r",
      .offset = {0, 0},
      .rx = 2.5F,
      .ry = 3.0F,
      .colorRole = ColorRole::Fixed,
      .fixedColor = {255, 255, 255},
      .z = 10,
  });

  form.shapes.push_back(FormShape{
      .name = "eye_r_pupil",
      .type = ShapeType::Circle,
      .anchor = "eye_r",
      .offset = {0.5F, 0},
      .radius = 1.5F,
      .colorRole = ColorRole::Fixed,
      .fixedColor = {30, 20, 40},
      .z = 11,
  });

  // Little feet
  {
    FormShape shape{};
    shape.name = "foot_l";
    shape.type = ShapeType::Circle;
    shape.anchor = "foot_l";
    shape.offset = {0, 0};
    shape.radius = 2.5F;
    shape.colorRole = ColorRole::Accent;
    shape.z = 5;
    form.shapes.push_back(std::move(shape));
  }

  {
    FormShape shape{};
    shape.name = "foot_r";
    shape.type = ShapeType::Circle;
    shape.anchor = "foot_r";
    shape.offset = {0, 0};
    shape.radius = 2.5F;
    shape.colorRole = ColorRole::Accent;
    shape.z = 5;
    form.shapes.push_back(std::move(shape));
  }

  // Poses
  Pose idle;
  idle.name = "idle";
  idle.squashX = 1.0F;
  idle.squashY = 1.0F;
  form.poses["idle"] = idle;

  Pose run1;
  run1.name = "run1";
  run1.squashX = 1.05F;
  run1.squashY = 0.95F;
  run1.anchors = {
      {"foot_l", {-4, -1}},
      {"foot_r", {4, -2}},
  };
  form.poses["run1"] = run1;

  Pose run2;
  run2.name = "run2";
  run2.squashX = 1.05F;
  run2.squashY = 0.95F;
  run2.anchors = {
      {"foot_l", {-4, -2}},
      {"foot_r", {4, -1}},
  };
  form.poses["run2"] = run2;

  Pose jump;
  jump.name = "jump";
  jump.squashX = 0.9F;
  jump.squashY = 1.1F;
  jump.anchors = {
      {"foot_l", {-2, -2}},
      {"foot_r", {2, -2}},
  };
  form.poses["jump"] = jump;

  Pose fall;
  fall.name = "fall";
  fall.squashX = 1.1F;
  fall.squashY = 0.9F;
  fall.anchors = {
      {"foot_l", {-3, -1}},
      {"foot_r", {3, -1}},
  };
  form.poses["fall"] = fall;

  Animation runAnim;
  runAnim.name = "run";
  runAnim.duration = 0.25F;
  runAnim.loop = true;
  runAnim.poseNames = {"run1", "idle", "run2", "idle"};
  form.animations["run"] = runAnim;

  return form;
}

}  // namespace Visual
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
