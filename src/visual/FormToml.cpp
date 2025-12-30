#include "visual/FormToml.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <toml++/toml.h>

#include "util/TomlUtil.h"
#include "visual/Form.h"
#include "visual/Palette.h"
#include "visual/Rules.h"
#include "visual/Shapes.h"

namespace Visual {
namespace {

bool parseVec2(const toml::node& node, Vec2& out) {
  const toml::array* arr = node.as_array();
  if (arr == nullptr || arr->size() < 2) {
    return false;
  }
  auto x = (*arr)[0].value<double>();
  auto y = (*arr)[1].value<double>();
  if (!x || !y) {
    return false;
  }
  out = {static_cast<float>(*x), static_cast<float>(*y)};
  return true;
}

bool parseColor(std::string_view s, Color& out) {
  if (s.empty()) {
    return false;
  }
  out = Color::fromHex(s);
  return true;
}

bool parseColorRole(std::string_view s, ColorRole& out) {
  if (s == "base") {
    out = ColorRole::Base;
    return true;
  }
  if (s == "light") {
    out = ColorRole::Light;
    return true;
  }
  if (s == "dark") {
    out = ColorRole::Dark;
    return true;
  }
  if (s == "accent") {
    out = ColorRole::Accent;
    return true;
  }
  if (s == "outline") {
    out = ColorRole::Outline;
    return true;
  }
  if (s == "fixed") {
    out = ColorRole::Fixed;
    return true;
  }
  return false;
}

bool parseShapeType(std::string_view s, ShapeType& out) {
  if (s == "circle") {
    out = ShapeType::Circle;
    return true;
  }
  if (s == "ellipse") {
    out = ShapeType::Ellipse;
    return true;
  }
  if (s == "capsule") {
    out = ShapeType::Capsule;
    return true;
  }
  if (s == "triangle") {
    out = ShapeType::Triangle;
    return true;
  }
  if (s == "polygon") {
    out = ShapeType::Polygon;
    return true;
  }
  if (s == "rect") {
    out = ShapeType::Rect;
    return true;
  }
  if (s == "pixel_grid") {
    out = ShapeType::PixelGrid;
    return true;
  }
  return false;
}

std::vector<std::string> parseStringArray(const toml::array& arr) {
  std::vector<std::string> out;
  out.reserve(arr.size());
  for (const auto& node : arr) {
    if (auto v = node.value<std::string>()) {
      out.push_back(*v);
    }
  }
  return out;
}

void loadFormMeta(const toml::table& tbl, const char* path, CharacterForm& form) {
  if (const auto* formTbl = tbl["form"].as_table()) {
    TomlUtil::warnUnknownKeys(*formTbl, path, "form", {"id"});
    if (const auto* v = formTbl->get("id"); v != nullptr) {
      form.id = v->value_or(form.id);
    }
  }
}

void loadPalette(const toml::table& tbl, const char* path, CharacterForm& form) {
  Color base = Color::fromHex("808080");
  Color accent = Color::fromHex("F0B040");
  if (const auto* pal = tbl["palette"].as_table()) {
    TomlUtil::warnUnknownKeys(*pal, path, "palette", {"base", "accent"});
    if (const auto* v = pal->get("base"); v != nullptr && v->is_string()) {
      (void)parseColor(v->value_or(""), base);
    }
    if (const auto* v = pal->get("accent"); v != nullptr && v->is_string()) {
      (void)parseColor(v->value_or(""), accent);
    }
  }
  form.palette = buildPalette(base, accent, gRules);
}

void loadSkeleton(const toml::table& tbl, const char* path, CharacterForm& form) {
  if (const auto* skel = tbl["skeleton"].as_table()) {
    TomlUtil::warnUnknownKeys(*skel, path, "skeleton", {"anchors"});
    if (const auto* anchors = skel->get("anchors"); anchors != nullptr && anchors->is_table()) {
      for (const auto& [key, node] : *anchors->as_table()) {
        Vec2 v{};
        if (parseVec2(node, v)) {
          form.skeleton.anchors[std::string{key.str()}] = v;
        }
      }
    }
  }
}

void loadShapeMeta(const toml::table& s, FormShape& shape) {
  if (const auto* v = s.get("name"); v != nullptr) {
    shape.name = v->value_or(shape.name);
  }
  if (const auto* v = s.get("type"); v != nullptr && v->is_string()) {
    (void)parseShapeType(v->value_or(""), shape.type);
  }
  if (const auto* v = s.get("anchor"); v != nullptr) {
    shape.anchor = v->value_or(shape.anchor);
  }
  if (const auto* v = s.get("offset"); v != nullptr) {
    (void)parseVec2(*v, shape.offset);
  }
  if (const auto* v = s.get("z"); v != nullptr) {
    shape.z = v->value_or(shape.z);
  }
  if (const auto* v = s.get("color_role"); v != nullptr && v->is_string()) {
    (void)parseColorRole(v->value_or(""), shape.colorRole);
  }
  if (const auto* v = s.get("color_key"); v != nullptr && v->is_string()) {
    shape.colorKey = v->value_or(shape.colorKey);
  }
  if (const auto* v = s.get("fixed_color"); v != nullptr && v->is_string()) {
    (void)parseColor(v->value_or(""), shape.fixedColor);
  }
}

void loadShapeGeometry(const toml::table& s, FormShape& shape) {
  if (const auto* v = s.get("radius"); v != nullptr) {
    shape.radius = v->value_or(shape.radius);
  }
  if (const auto* v = s.get("rx"); v != nullptr) {
    shape.rx = v->value_or(shape.rx);
  }
  if (const auto* v = s.get("ry"); v != nullptr) {
    shape.ry = v->value_or(shape.ry);
  }
  if (const auto* v = s.get("p1"); v != nullptr) {
    (void)parseVec2(*v, shape.p1);
  }
  if (const auto* v = s.get("p2"); v != nullptr) {
    (void)parseVec2(*v, shape.p2);
  }
  if (const auto* v = s.get("width"); v != nullptr) {
    shape.width = v->value_or(shape.width);
  }
  if (const auto* v = s.get("w"); v != nullptr) {
    shape.w = v->value_or(shape.w);
  }
  if (const auto* v = s.get("h"); v != nullptr) {
    shape.h = v->value_or(shape.h);
  }
}

void loadShapePoints(const toml::table& s, FormShape& shape) {
  if (const auto* points = s.get("points"); points != nullptr && points->is_array()) {
    for (const auto& p : *points->as_array()) {
      Vec2 pt{};
      if (parseVec2(p, pt)) {
        shape.points.push_back(pt);
      }
    }
  }
}

void loadShapePixelSizing(const toml::table& s, FormShape& shape) {
  if (const auto* v = s.get("pixel_size"); v != nullptr) {
    shape.pixelSize = std::max(1, v->value_or(shape.pixelSize));
  }
  if (const auto* v = s.get("pixel_pivot"); v != nullptr) {
    (void)parseVec2(*v, shape.pixelPivot);
  }
  if (const auto* v = s.get("pixel_transparent"); v != nullptr && v->is_string()) {
    std::string val = v->value_or("");
    if (!val.empty()) {
      shape.pixelTransparent = val[0];
    }
  }
}

void loadShapePixelRows(const toml::table& s, FormShape& shape) {
  if (const auto* v = s.get("pixels"); v != nullptr && v->is_array()) {
    for (const auto& row : *v->as_array()) {
      if (auto line = row.value<std::string>()) {
        shape.pixels.push_back(*line);
      }
    }
  }
}

void loadShapePixelRoles(const toml::table& s, FormShape& shape) {
  if (const auto* v = s.get("pixel_roles"); v != nullptr && v->is_table()) {
    for (const auto& [key, roleNode] : *v->as_table()) {
      std::string k{key.str()};
      if (k.empty()) {
        continue;
      }
      ColorRole role = ColorRole::Base;
      if (auto roleStr = roleNode.value<std::string>()) {
        if (parseColorRole(*roleStr, role)) {
          shape.pixelRoles[k[0]] = role;
        }
      }
    }
  }
}

void loadShapePixelColorKeys(const toml::table& s, FormShape& shape) {
  if (const auto* v = s.get("pixel_color_keys"); v != nullptr && v->is_table()) {
    for (const auto& [key, colorNode] : *v->as_table()) {
      std::string k{key.str()};
      if (k.empty()) {
        continue;
      }
      if (auto colorKey = colorNode.value<std::string>()) {
        shape.pixelColorKeys[k[0]] = *colorKey;
      }
    }
  }
}

void loadShapePixelFixed(const toml::table& s, FormShape& shape) {
  if (const auto* v = s.get("pixel_fixed"); v != nullptr && v->is_table()) {
    for (const auto& [key, colNode] : *v->as_table()) {
      std::string k{key.str()};
      if (k.empty()) {
        continue;
      }
      if (auto colStr = colNode.value<std::string>()) {
        Color c{};
        if (parseColor(*colStr, c)) {
          shape.pixelFixed[k[0]] = c;
        }
      }
    }
  }
}

void loadShapePixels(const toml::table& s, FormShape& shape) {
  loadShapePixelSizing(s, shape);
  loadShapePixelRows(s, shape);
  loadShapePixelRoles(s, shape);
  loadShapePixelColorKeys(s, shape);
  loadShapePixelFixed(s, shape);
}

void loadShapeVisibility(const toml::table& s, FormShape& shape) {
  if (const auto* v = s.get("only_in_poses"); v != nullptr && v->is_array()) {
    shape.onlyInPoses = parseStringArray(*v->as_array());
  }
  if (const auto* v = s.get("hidden_in_poses"); v != nullptr && v->is_array()) {
    shape.hiddenInPoses = parseStringArray(*v->as_array());
  }
  if (const auto* v = s.get("only_in_variants"); v != nullptr && v->is_array()) {
    shape.onlyInVariants = parseStringArray(*v->as_array());
  }
  if (const auto* v = s.get("hidden_in_variants"); v != nullptr && v->is_array()) {
    shape.hiddenInVariants = parseStringArray(*v->as_array());
  }
}

void loadShapes(const toml::table& tbl, const char* path, CharacterForm& form) {
  if (const auto* shapes = tbl["shapes"].as_array()) {
    for (size_t i = 0; i < shapes->size(); ++i) {
      const auto& node = (*shapes)[i];
      const auto* s = node.as_table();
      if (s == nullptr) {
        continue;
      }

      const std::string shapeScope = "shapes[" + std::to_string(i) + "]";
      TomlUtil::warnUnknownKeys(*s, path, shapeScope,
                                {"name",
                                 "type",
                                 "anchor",
                                 "offset",
                                 "z",
                                 "color_role",
                                 "color_key",
                                 "fixed_color",
                                 "radius",
                                 "rx",
                                 "ry",
                                 "p1",
                                 "p2",
                                 "width",
                                 "w",
                                 "h",
                                 "points",
                                 "pixel_size",
                                 "pixel_pivot",
                                 "pixel_transparent",
                                 "pixels",
                                 "pixel_roles",
                                 "pixel_color_keys",
                                 "pixel_fixed",
                                 "only_in_poses",
                                 "hidden_in_poses",
                                 "only_in_variants",
                                 "hidden_in_variants"});

      FormShape shape{};
      loadShapeMeta(*s, shape);
      loadShapeGeometry(*s, shape);
      loadShapePoints(*s, shape);
      loadShapePixels(*s, shape);
      loadShapeVisibility(*s, shape);
      form.shapes.push_back(std::move(shape));
    }
  }
}

void loadPoseTable(const toml::table& p,
                   const std::string& poseName,
                   const char* path,
                   CharacterForm& form) {
  Pose pose;
  pose.name = poseName;
  const std::string poseScope = "poses." + pose.name;
  TomlUtil::warnUnknownKeys(p, path, poseScope, {"lean", "squash_x", "squash_y", "anchors"});
  if (const auto* v = p.get("lean"); v != nullptr) {
    pose.lean = v->value_or(pose.lean);
  }
  if (const auto* v = p.get("squash_x"); v != nullptr) {
    pose.squashX = v->value_or(pose.squashX);
  }
  if (const auto* v = p.get("squash_y"); v != nullptr) {
    pose.squashY = v->value_or(pose.squashY);
  }
  if (const auto* anchors = p.get("anchors"); anchors != nullptr && anchors->is_table()) {
    for (const auto& [akey, anode] : *anchors->as_table()) {
      Vec2 v{};
      if (parseVec2(anode, v)) {
        pose.anchors[std::string{akey.str()}] = v;
      }
    }
  }
  form.poses[pose.name] = std::move(pose);
}

void loadPoses(const toml::table& tbl, const char* path, CharacterForm& form) {
  if (const auto* poses = tbl["poses"].as_table()) {
    for (const auto& [key, node] : *poses) {
      const auto* p = node.as_table();
      if (p == nullptr) {
        continue;
      }
      loadPoseTable(*p, std::string{key.str()}, path, form);
    }
  }
}

void loadAnimationTable(const toml::table& a,
                        const std::string& animName,
                        const char* path,
                        CharacterForm& form) {
  Animation anim;
  anim.name = animName;
  const std::string animScope = "animations." + anim.name;
  TomlUtil::warnUnknownKeys(a, path, animScope,
                            {"duration", "loop", "pose_names", "key_times", "easing",
                             "velocity_blend", "velocity_min", "velocity_max"});
  if (const auto* v = a.get("duration"); v != nullptr) {
    anim.duration = v->value_or(anim.duration);
  }
  if (const auto* v = a.get("loop"); v != nullptr) {
    anim.loop = v->value_or(anim.loop);
  }
  if (const auto* v = a.get("pose_names"); v != nullptr && v->is_array()) {
    anim.poseNames = parseStringArray(*v->as_array());
  }
  if (const auto* v = a.get("key_times"); v != nullptr && v->is_array()) {
    for (const auto& t : *v->as_array()) {
      if (auto tv = t.value<double>()) {
        anim.keyTimes.push_back(static_cast<float>(*tv));
      }
    }
  }
  if (const auto* v = a.get("easing"); v != nullptr) {
    anim.easing = v->value_or(anim.easing);
  }
  if (const auto* v = a.get("velocity_blend"); v != nullptr) {
    anim.velocityBlend = v->value_or(anim.velocityBlend);
  }
  if (const auto* v = a.get("velocity_min"); v != nullptr) {
    anim.velocityMin = v->value_or(anim.velocityMin);
  }
  if (const auto* v = a.get("velocity_max"); v != nullptr) {
    anim.velocityMax = v->value_or(anim.velocityMax);
  }

  form.animations[anim.name] = std::move(anim);
}

void loadAnimations(const toml::table& tbl, const char* path, CharacterForm& form) {
  if (const auto* anims = tbl["animations"].as_table()) {
    for (const auto& [key, node] : *anims) {
      const auto* a = node.as_table();
      if (a == nullptr) {
        continue;
      }
      loadAnimationTable(*a, std::string{key.str()}, path, form);
    }
  }
}

void loadDynamics(const toml::table& tbl, const char* path, CharacterForm& form) {
  if (const auto* dyn = tbl["dynamics"].as_table()) {
    TomlUtil::warnUnknownKeys(*dyn, path, "dynamics", {"anchors"});
    if (const auto* anchors = dyn->get("anchors"); anchors != nullptr && anchors->is_table()) {
      for (const auto& [key, node] : *anchors->as_table()) {
        const auto* t = node.as_table();
        if (t == nullptr) {
          continue;
        }
        const std::string anchorScope = "dynamics.anchors." + std::string{key.str()};
        TomlUtil::warnUnknownKeys(*t, path, anchorScope, {"stiffness", "damping", "max_offset"});
        AnchorDynamics dynAnchor;
        if (const auto* v = t->get("stiffness"); v != nullptr) {
          dynAnchor.stiffness = v->value_or(dynAnchor.stiffness);
        }
        if (const auto* v = t->get("damping"); v != nullptr) {
          dynAnchor.damping = v->value_or(dynAnchor.damping);
        }
        if (const auto* v = t->get("max_offset"); v != nullptr) {
          dynAnchor.maxOffset = v->value_or(dynAnchor.maxOffset);
        }
        form.dynamics.anchors[std::string{key.str()}] = dynAnchor;
      }
    }
  }
}

bool loadIkChain(const toml::table& c, const std::string& scope, const char* path, IkChain& chain) {
  TomlUtil::warnUnknownKeys(
      c, path, scope,
      {"root", "mid", "end", "target", "len_a", "len_b", "bend_dir", "mirror_with_facing"});
  if (const auto* v = c.get("root"); v != nullptr) {
    chain.root = v->value_or(chain.root);
  }
  if (const auto* v = c.get("mid"); v != nullptr) {
    chain.mid = v->value_or(chain.mid);
  }
  if (const auto* v = c.get("end"); v != nullptr) {
    chain.end = v->value_or(chain.end);
  }
  if (const auto* v = c.get("target"); v != nullptr) {
    chain.target = v->value_or(chain.target);
  }
  if (const auto* v = c.get("len_a"); v != nullptr) {
    chain.lenA = v->value_or(chain.lenA);
  }
  if (const auto* v = c.get("len_b"); v != nullptr) {
    chain.lenB = v->value_or(chain.lenB);
  }
  if (const auto* v = c.get("bend_dir"); v != nullptr) {
    chain.bendDir = v->value_or(chain.bendDir);
  }
  if (const auto* v = c.get("mirror_with_facing"); v != nullptr) {
    chain.mirrorWithFacing = v->value_or(chain.mirrorWithFacing);
  }

  return !chain.root.empty() && !chain.mid.empty() && !chain.end.empty();
}

void loadIk(const toml::table& tbl, const char* path, CharacterForm& form) {
  if (const auto* ik = tbl["ik"].as_table()) {
    TomlUtil::warnUnknownKeys(*ik, path, "ik", {"chains"});
    if (const auto* chains = ik->get("chains"); chains != nullptr && chains->is_array()) {
      std::size_t idx = 0;
      for (const auto& node : *chains->as_array()) {
        const auto* c = node.as_table();
        const std::string chainScope = "ik.chains[" + std::to_string(idx) + "]";
        ++idx;
        if (c == nullptr) {
          continue;
        }
        IkChain chain;
        if (loadIkChain(*c, chainScope, path, chain)) {
          form.ikChains.push_back(std::move(chain));
        }
      }
    }
  }
}

void validateFormAnchors(const CharacterForm& form, const char* path) {
  auto hasAnchor = [&](const std::string& name) {
    return !name.empty() && form.skeleton.anchors.find(name) != form.skeleton.anchors.end();
  };
  for (size_t i = 0; i < form.shapes.size(); ++i) {
    const FormShape& shape = form.shapes[i];
    if (!hasAnchor(shape.anchor)) {
      TomlUtil::warnf(path, "shapes[{}] '{}' anchor '{}' not found in skeleton", i, shape.name,
                      shape.anchor);
    }
  }
  for (const auto& [poseName, pose] : form.poses) {
    for (const auto& [anchorName, _] : pose.anchors) {
      if (!hasAnchor(anchorName)) {
        TomlUtil::warnf(path, "poses.{} anchor '{}' not found in skeleton", poseName, anchorName);
      }
    }
  }
  for (const auto& [anchorName, _] : form.dynamics.anchors) {
    if (!hasAnchor(anchorName)) {
      TomlUtil::warnf(path, "dynamics.anchors.{} not found in skeleton", anchorName);
    }
  }
  for (size_t i = 0; i < form.ikChains.size(); ++i) {
    const IkChain& chain = form.ikChains[i];
    const char* scope = "ik.chains";
    if (!hasAnchor(chain.root)) {
      TomlUtil::warnf(path, "{}[{}] root '{}' not found in skeleton", scope, i, chain.root);
    }
    if (!hasAnchor(chain.mid)) {
      TomlUtil::warnf(path, "{}[{}] mid '{}' not found in skeleton", scope, i, chain.mid);
    }
    if (!hasAnchor(chain.end)) {
      TomlUtil::warnf(path, "{}[{}] end '{}' not found in skeleton", scope, i, chain.end);
    }
    if (!chain.target.empty() && !hasAnchor(chain.target)) {
      TomlUtil::warnf(path, "{}[{}] target '{}' not found in skeleton", scope, i, chain.target);
    }
  }
}

}  // namespace

bool loadFormFromToml(const char* path, CharacterForm& out) {
  toml::table tbl;
  try {
    tbl = toml::parse_file(path);
  } catch (...) {
    return false;
  }

  TomlUtil::warnUnknownKeys(tbl, path, "root",
                            {"version", "form", "palette", "skeleton", "shapes", "poses",
                             "animations", "dynamics", "ik"});

  CharacterForm form;
  loadFormMeta(tbl, path, form);
  loadPalette(tbl, path, form);
  loadSkeleton(tbl, path, form);
  loadShapes(tbl, path, form);
  loadPoses(tbl, path, form);
  loadAnimations(tbl, path, form);
  loadDynamics(tbl, path, form);
  loadIk(tbl, path, form);
  validateFormAnchors(form, path);

  out = std::move(form);
  return true;
}

}  // namespace Visual
