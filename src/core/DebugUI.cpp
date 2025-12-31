#include "core/DebugUI.h"

#ifndef SANDBOX_WITH_IMGUI
#define SANDBOX_WITH_IMGUI 0
#endif

#if SANDBOX_WITH_IMGUI
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_internal.h>
#endif

#include <cfloat>
#include <cstdio>

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <utility>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include "character/CharacterConfig.h"
#include "util/Paths.h"

bool DebugUI::available() {
  return SANDBOX_WITH_IMGUI != 0;
}

#if SANDBOX_WITH_IMGUI
static ImTextureID textureIdFor(SDL_Texture* tex) {
  // ImTextureID is backend-defined; reinterpret_cast handles pointer or integer cases.
  return reinterpret_cast<ImTextureID>(tex);
}

static std::string labelForPath(const std::vector<std::string>* paths,
                                const std::vector<std::string>* labels,
                                const std::string& path) {
  if ((paths == nullptr) || (labels == nullptr)) {
    return Paths::fileStem(path);
  }
  const std::size_t n = std::min(paths->size(), labels->size());
  for (std::size_t i = 0; i < n; ++i) {
    if ((*paths)[i] != path)
      continue;
    if (!(*labels)[i].empty())
      return (*labels)[i];
    break;
  }
  return Paths::fileStem(path);
}

struct CharacterIconSpec {
  std::string iconPath;   // Dedicated icon image (preferred)
  std::string sheetPath;  // Fallback: first frame of sprite sheet
  int frameW = 0;
  int frameH = 0;
  bool loaded = false;
};

static const CharacterIconSpec& iconSpecForPath(const std::string& characterTomlPath) {
  static std::unordered_map<std::string, CharacterIconSpec> cache;
  auto it = cache.find(characterTomlPath);
  if (it != cache.end())
    return it->second;

  CharacterIconSpec spec{};
  CharacterConfig cfg;
  if (cfg.loadFromToml(characterTomlPath.c_str())) {
    if (!cfg.render.icon.empty())
      spec.iconPath = Paths::resolveAssetPath(cfg.render.icon);
    if (!cfg.render.sheet.empty())
      spec.sheetPath = Paths::resolveAssetPath(cfg.render.sheet);
    spec.frameW = cfg.render.frameW;
    spec.frameH = cfg.render.frameH;
  }
  spec.loaded = true;
  it = cache.emplace(characterTomlPath, std::move(spec)).first;
  return it->second;
}

static bool resolveSpriteThumb(SpriteCache* sprites,
                               const CharacterIconSpec& spec,
                               SDL_Texture*& outTex,
                               ImVec2& outUv0,
                               ImVec2& outUv1) {
  outTex = nullptr;
  outUv0 = ImVec2(0.0F, 0.0F);
  outUv1 = ImVec2(1.0F, 1.0F);

  if (sprites == nullptr) {
    return false;
  }

  // Prefer dedicated icon image (full texture, no UV cropping needed)
  if (!spec.iconPath.empty()) {
    SDL_Texture* tex = sprites->get(spec.iconPath);
    if (tex != nullptr) {
      outTex = tex;
      outUv0 = ImVec2(0.0F, 0.0F);
      outUv1 = ImVec2(1.0F, 1.0F);
      return true;
    }
  }

  // Fallback: first frame of sprite sheet
  if (spec.sheetPath.empty() || spec.frameW <= 0 || spec.frameH <= 0) {
    return false;
  }

  SDL_Texture* tex = sprites->get(spec.sheetPath);
  if (tex == nullptr) {
    return false;
  }

  float tw = 0.0F;
  float th = 0.0F;
  if (!SDL_GetTextureSize(tex, &tw, &th) || tw <= 0.0F || th <= 0.0F) {
    return false;
  }

  const float fw = std::clamp(static_cast<float>(spec.frameW), 1.0F, tw);
  const float fh = std::clamp(static_cast<float>(spec.frameH), 1.0F, th);
  outTex = tex;
  outUv0 = ImVec2(0.0F, 0.0F);
  outUv1 = ImVec2(fw / tw, fh / th);
  return true;
}

// NOLINTNEXTLINE
static DebugUIMenuActions drawSandboxControls(SpriteCache* sprites,
                                              const DebugUIMenuModel& model,
                                              const DebugUIStageInspectorModel& stageModel,
                                              bool showResumeButton,
                                              bool closeOnAction,
                                              bool showHotkeys) {
  DebugUIMenuActions out{};

  auto sameLineIfFitsNext = [&](const char* nextLabel) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float nextWidth = ImGui::CalcTextSize(nextLabel).x + style.FramePadding.x * 2.0F;
    const float nextX2 = ImGui::GetItemRectMax().x + style.ItemSpacing.x + nextWidth;
    const float windowX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    if (nextX2 < windowX2)
      ImGui::SameLine();
  };

  if (showResumeButton) {
    if (ImGui::Button("Resume"))
      out.close = true;
    ImGui::SameLine();
  }

  if (ImGui::Button("Quit"))
    out.quit = true;

  const std::string stageLabel =
      model.stagePath.empty() ? "(none)"
                              : labelForPath(model.stagePaths, model.stageLabels, model.stagePath);
  const ImGuiComboFlags comboFlags = ImGuiComboFlags_WidthFitPreview;
  if (model.stagePaths != nullptr) {
    const float stageComboW = ImGui::GetFrameHeight() + ImGui::CalcTextSize(stageLabel.c_str()).x +
                              ImGui::GetStyle().FramePadding.x * 2.0F;
    const float stagePopupMaxW = std::max(420.0F, stageComboW);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.0F, 0.0F), ImVec2(stagePopupMaxW, FLT_MAX));
    if (ImGui::BeginCombo("Stage", stageLabel.c_str(), comboFlags)) {
      static ImGuiTextFilter stageFilter;
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Filter");
      ImGui::SameLine();
      stageFilter.Draw("##stage_filter", ImGui::GetContentRegionAvail().x);
      ImGui::Separator();
      const std::size_t n = model.stagePaths->size();
      for (std::size_t i = 0; i < n; ++i) {
        const std::string& p = (*model.stagePaths)[i];
        const bool selected = (p == model.stagePath);
        std::string label = Paths::fileStem(p);
        if ((model.stageLabels != nullptr) && i < model.stageLabels->size() &&
            !(*model.stageLabels)[i].empty()) {
          label = (*model.stageLabels)[i];
        }
        if (stageFilter.IsActive()) {
          std::string haystack = label;
          haystack += ' ';
          haystack += p;
          if (!stageFilter.PassFilter(haystack.c_str()))
            continue;
        }
        std::string idLabel = label;
        idLabel += "##";
        idLabel += p;
        if (ImGui::Selectable(idLabel.c_str(), selected)) {
          out.selectStage = true;
          out.stagePath = p;
          if (closeOnAction)
            out.close = true;
        }
        if (selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered() && !stageLabel.empty())
      ImGui::SetTooltip("%s", stageLabel.c_str());
  }

  const std::string characterLabel =
      model.characterPath.empty()
          ? "(none)"
          : labelForPath(model.characterPaths, model.characterLabels, model.characterPath);
  if (model.characterPaths != nullptr) {
    auto drawSpriteThumb = [&](const CharacterIconSpec& spec) {
      const ImGuiStyle& style = ImGui::GetStyle();
      const float iconSize = std::max(1.0F, ImGui::GetFrameHeight() - style.FramePadding.y * 2.0F);

      SDL_Texture* tex = nullptr;
      ImVec2 uv0{};
      ImVec2 uv1{};
      if (!resolveSpriteThumb(sprites, spec, tex, uv0, uv1)) {
        ImGui::Dummy(ImVec2(iconSize, iconSize));
        return;
      }
      ImGui::Image(textureIdFor(tex), ImVec2(iconSize, iconSize), uv0, uv1);
    };

    const ImGuiStyle& style = ImGui::GetStyle();
    const float iconSize = std::max(1.0F, ImGui::GetFrameHeight() - style.FramePadding.y * 2.0F);
    float previewW = ImGui::CalcTextSize(characterLabel.c_str()).x;
    if (!model.characterPath.empty()) {
      const CharacterIconSpec& spec = iconSpecForPath(model.characterPath);
      if (!spec.sheetPath.empty() && spec.frameW > 0 && spec.frameH > 0) {
        previewW += iconSize + style.ItemInnerSpacing.x;
      }
    }
    const float arrowSize = ImGui::GetFrameHeight();
    const float characterComboW = arrowSize + previewW + style.FramePadding.x * 2.0F;

    ImGui::SetNextItemWidth(characterComboW);
    const float characterPopupMaxW = std::max(420.0F, characterComboW);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.0F, 0.0F), ImVec2(characterPopupMaxW, FLT_MAX));
    const ImGuiComboFlags characterFlags =
        static_cast<ImGuiComboFlags>(ImGuiComboFlags_CustomPreview);
    if (ImGui::BeginCombo("Character", "", characterFlags)) {
      static ImGuiTextFilter characterFilter;
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Filter");
      ImGui::SameLine();
      characterFilter.Draw("##character_filter", ImGui::GetContentRegionAvail().x);
      ImGui::Separator();

      const std::size_t n = model.characterPaths->size();
      for (std::size_t i = 0; i < n; ++i) {
        const std::string& p = (*model.characterPaths)[i];
        const bool selected = (p == model.characterPath);
        std::string label = Paths::fileStem(p);
        if ((model.characterLabels != nullptr) && i < model.characterLabels->size() &&
            !(*model.characterLabels)[i].empty()) {
          label = (*model.characterLabels)[i];
        }
        if (characterFilter.IsActive()) {
          std::string haystack = label;
          haystack += ' ';
          haystack += p;
          if (!characterFilter.PassFilter(haystack.c_str()))
            continue;
        }

        const CharacterIconSpec& spec = iconSpecForPath(p);

        ImGui::PushID(p.c_str());
        const float rowH =
            std::max(ImGui::GetTextLineHeightWithSpacing(), iconSize + style.FramePadding.y * 2.0F);
        const bool picked = ImGui::Selectable(
            "##character_pick", selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0F, rowH));

        // Overlay the icon + text on top of the selectable (don't use cursor repositioning in the
        // popup window; it can trigger ImGui's "extend boundaries" assertion).
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(itemMin, itemMax, true);

        const float h = itemMax.y - itemMin.y;
        const float iconX = itemMin.x + style.FramePadding.x;
        const float iconY = itemMin.y + std::max(0.0F, (h - iconSize) * 0.5F);
        const ImVec2 iconP0(iconX, iconY);
        const ImVec2 iconP1(iconX + iconSize, iconY + iconSize);

        SDL_Texture* tex = nullptr;
        ImVec2 uv0{};
        ImVec2 uv1{};
        if (resolveSpriteThumb(sprites, spec, tex, uv0, uv1)) {
          dl->AddImage(textureIdFor(tex), iconP0, iconP1, uv0, uv1);
        } else {
          const ImU32 bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
          const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
          dl->AddRectFilled(iconP0, iconP1, bg, 3.0F);
          dl->AddRect(iconP0, iconP1, border, 3.0F);
        }

        const float textX = iconP1.x + style.ItemInnerSpacing.x;
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        const float textY = itemMin.y + std::max(0.0F, (h - textSize.y) * 0.5F);
        dl->AddText(ImVec2(textX, textY), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());

        dl->PopClipRect();
        ImGui::PopID();

        if (picked) {
          out.selectCharacter = true;
          out.characterPath = p;
          if (closeOnAction)
            out.close = true;
        }
        if (selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    if (ImGui::BeginComboPreview()) {
      if (!model.characterPath.empty()) {
        const CharacterIconSpec& spec = iconSpecForPath(model.characterPath);
        drawSpriteThumb(spec);
        ImGui::SameLine(0.0F, style.ItemInnerSpacing.x);
      }
      ImGui::TextUnformatted(characterLabel.c_str());
      ImGui::EndComboPreview();
    }

    if (ImGui::IsItemHovered() && !characterLabel.empty())
      ImGui::SetTooltip("%s", characterLabel.c_str());
  }

  std::string spawnLabel;
  if (model.spawnPoint.empty()) {
    spawnLabel = "custom (" + std::to_string(static_cast<int>(model.spawnX)) + "," +
                 std::to_string(static_cast<int>(model.spawnY)) + ")";
  } else {
    spawnLabel = model.spawnPoint;
  }

  if ((stageModel.spawns != nullptr) && !model.spawnPoint.empty()) {
    for (const auto& sp : *stageModel.spawns) {
      if (sp.name != model.spawnPoint)
        continue;
      spawnLabel = sp.name + " (" + std::to_string(static_cast<int>(sp.x)) + "," +
                   std::to_string(static_cast<int>(sp.y)) + "," +
                   std::string((sp.facingX < 0) ? "<" : ">") + ")";
      break;
    }
  }

  if (model.spawnPoints != nullptr) {
    const float spawnComboW = ImGui::GetFrameHeight() + ImGui::CalcTextSize(spawnLabel.c_str()).x +
                              ImGui::GetStyle().FramePadding.x * 2.0F;
    const float spawnPopupMaxW = std::max(420.0F, spawnComboW);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.0F, 0.0F), ImVec2(spawnPopupMaxW, FLT_MAX));
    if (ImGui::BeginCombo("Spawn", spawnLabel.c_str(), ImGuiComboFlags_WidthFitPreview)) {
      static ImGuiTextFilter spawnFilter;
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Filter");
      ImGui::SameLine();
      spawnFilter.Draw("##spawn_filter", ImGui::GetContentRegionAvail().x);
      ImGui::Separator();

      if ((stageModel.spawns != nullptr) && !stageModel.spawns->empty()) {
        for (const auto& sp : *stageModel.spawns) {
          const bool selected = (sp.name == model.spawnPoint);

          if (spawnFilter.IsActive()) {
            const std::string haystack = sp.name + " " + std::to_string(static_cast<int>(sp.x)) +
                                         " " + std::to_string(static_cast<int>(sp.y));
            if (!spawnFilter.PassFilter(haystack.c_str()))
              continue;
          }

          const std::string display = sp.name + " (" + std::to_string(static_cast<int>(sp.x)) +
                                      "," + std::to_string(static_cast<int>(sp.y)) + "," +
                                      std::string((sp.facingX < 0) ? "<" : ">") + ")";
          const std::string idLabel = display + "##spawn_" + sp.name;
          if (ImGui::Selectable(idLabel.c_str(), selected)) {
            out.selectSpawnPoint = true;
            out.spawnPoint = sp.name;
            if (closeOnAction)
              out.close = true;
          }
          if (selected)
            ImGui::SetItemDefaultFocus();
        }
      } else {
        for (const auto& sp : *model.spawnPoints) {
          const bool selected = (sp == model.spawnPoint);
          if (spawnFilter.IsActive() && !spawnFilter.PassFilter(sp.c_str()))
            continue;
          if (ImGui::Selectable(sp.c_str(), selected)) {
            out.selectSpawnPoint = true;
            out.spawnPoint = sp;
            if (closeOnAction)
              out.close = true;
          }
          if (selected)
            ImGui::SetItemDefaultFocus();
        }
      }

      ImGui::EndCombo();
    }

    if (ImGui::IsItemHovered() && !spawnLabel.empty())
      ImGui::SetTooltip("%s", spawnLabel.c_str());
  }

  // Action buttons: wrap to the next line when the sidebar is narrow.
  const char* labels[] = {"Respawn",      "Teleport",    "Reload stage", "Reload character",
                          "Apply config", "Apply+reset", "Reset state"};
  for (int i = 0; i < 7; ++i) {
    const char* label = labels[i];
    if (ImGui::Button(label)) {
      switch (i) {
        case 0:
          out.respawn = true;
          break;
        case 1:
          out.teleportToSpawn = true;
          break;
        case 2:
          out.reloadStage = true;
          break;
        case 3:
          out.reloadCharacter = true;
          break;
        case 4:
          out.reloadCharacterInPlace = true;
          break;
        case 5:
          out.reloadCharacterInPlace = true;
          out.reloadCharacterResetRuntime = true;
          break;
        case 6:
          out.resetState = true;
          break;
        default:
          break;
      }

      if (closeOnAction)
        out.close = true;
    }

    if (i + 1 < 7)
      sameLineIfFitsNext(labels[i + 1]);
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Simulation");

  bool paused = model.simPaused;
  if (ImGui::Checkbox("Pause", &paused)) {
    out.setSimPaused = true;
    out.simPaused = paused;
  }

  float timeScale = model.timeScale;
  ImGui::TextUnformatted("Time scale");
  ImGui::SetNextItemWidth(-1.0F);
  if (ImGui::SliderFloat("##time_scale", &timeScale, 0.1F, 4.0F, "%.2F")) {
    out.setTimeScale = true;
    out.timeScale = timeScale;
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Render");
  static constexpr const char* kResModes[] = {"Auto (balanced)", "960x540", "640x360", "480x270",
                                              "320x180"};
  static constexpr int kResModeCount = static_cast<int>(sizeof(kResModes) / sizeof(kResModes[0]));
  int resMode = std::clamp(model.internalResMode, 0, kResModeCount - 1);
  if (ImGui::BeginCombo("Internal res", kResModes[resMode], ImGuiComboFlags_WidthFitPreview)) {
    for (int i = 0; i < kResModeCount; ++i) {
      const bool selected = (i == resMode);
      if (ImGui::Selectable(kResModes[i], selected)) {
        out.setInternalResMode = true;
        out.internalResMode = i;
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::TextDisabled("Smaller internal res = more zoom; Auto tries to avoid docked zoom-in");

  bool integerScaleOnly = model.integerScaleOnly;
  if (ImGui::Checkbox("Integer scale only", &integerScaleOnly)) {
    out.setIntegerScaleOnly = true;
    out.integerScaleOnly = integerScaleOnly;
  }
  ImGui::TextDisabled("When scaling up, snap to integer scale + nearest");

  if (ImGui::CollapsingHeader("Visual rules")) {
    ImGui::TextDisabled("procedural forms only (not persisted)");

    if (ImGui::Button("Reset visual rules"))
      out.resetVisualRules = true;

    DebugUIVisualRulesModel rules = model.visualRules;
    bool changed = false;

    changed |= ImGui::Checkbox("Warm highlights", &rules.highlightWarm);
    changed |= ImGui::Checkbox("Cool shadows", &rules.shadowCool);

    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0F);
    changed |= ImGui::SliderFloat("Outline scale", &rules.outlineScale, 1.0F, 1.25F, "%.3F");

    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0F);
    changed |=
        ImGui::SliderFloat("Max squash/stretch", &rules.squashStretchMax, 1.0F, 2.0F, "%.2F");
    changed |= ImGui::Checkbox("Preserve mass", &rules.preserveMass);

    ImGui::SetNextItemWidth(-1.0F);
    changed |= ImGui::SliderFloat("Smear threshold", &rules.smearThreshold, 0.0F, 1200.0F, "%.0F");

    changed |= ImGui::SliderInt("Afterimages", &rules.afterimageCount, 0, 4);

    ImGui::SetNextItemWidth(-1.0F);
    changed |=
        ImGui::SliderFloat("Afterimage decay", &rules.afterimageDecay, 0.05F, 1.0F, "%.2F s");

    if (changed) {
      out.setVisualRules = true;
      out.visualRules = rules;
    }
  }

  static int stepN = 10;
  stepN = std::clamp(stepN, 1, 10'000);
  ImGui::SetNextItemWidth(90.0F);
  ImGui::InputInt("Step N##step_n_count", &stepN);
  stepN = std::clamp(stepN, 1, 10'000);

  if (ImGui::Button("Step 1"))
    out.stepFrames = 1;
  ImGui::SameLine();
  if (ImGui::Button("Step N##step_n_btn"))
    out.stepFrames = stepN;

  ImGui::Separator();
  ImGui::TextUnformatted("Iteration");
  bool autoReload = model.autoReload;
  if (ImGui::Checkbox("Auto reload TOML", &autoReload)) {
    out.setAutoReload = true;
    out.autoReload = autoReload;
  }
  ImGui::TextDisabled("watches stage + character files on disk");

  ImGui::Separator();
  ImGui::TextUnformatted("Input");
  int deadzone = std::clamp(model.gamepadDeadzone, 0, 32767);
  ImGui::SetNextItemWidth(-1.0F);
  if (ImGui::SliderInt("Gamepad deadzone", &deadzone, 0, 32767)) {
    out.setGamepadDeadzone = true;
    out.gamepadDeadzone = deadzone;
  }
  ImGui::TextDisabled("left stick only (axis range 0..32767), d-pad unaffected");

  if (showHotkeys) {
    ImGui::Separator();
    ImGui::TextUnformatted("Hotkeys:");
    ImGui::PushTextWrapPos(0.0F);
    const char* hotkeys[] = {
        "Ctrl-C: quit",
        "Ctrl-1: overlay  Ctrl-2: collision",
        "Ctrl-H: hide/show UI",
    };
    for (const char* line : hotkeys) {
      ImGui::Bullet();
      ImGui::SameLine();
      ImGui::TextWrapped("%s", line);
    }
    ImGui::PopTextWrapPos();
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Reset");
  if (ImGui::Button("Reset layout + prefs"))
    out.resetLayoutAndPrefs = true;
  ImGui::TextDisabled("clears imgui.ini + session.toml (restart recommended)");

  return out;
}

static void drawQuickCharacterBar(SpriteCache* sprites,
                                  const DebugUIMenuModel& model,
                                  const ImVec2& imagePos,
                                  float drawW,
                                  float drawH,
                                  DebugUIMenuActions& out) {
  if ((model.characterPaths == nullptr) || model.characterPaths->empty()) {
    return;
  }

  const int slotCount =
      std::min(5, static_cast<int>(std::max<std::size_t>(1, model.characterPaths->size())));
  if (slotCount <= 0 || drawW <= 0.0F || drawH <= 0.0F) {
    return;
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float sizeScale = 0.34F * 1.75F;
  const float numberScale = std::clamp(sizeScale * 2.0F, 0.5F, 0.8F);
  const float barPad = 6.0F * sizeScale;
  const float barInset = 4.0F * sizeScale;
  const float slotPad = 4.0F * sizeScale;
  const float maxIconSize = 36.0F * sizeScale;
  const float minIconSize = 8.0F * sizeScale;
  const float rounding = 6.0F * sizeScale;
  const float shadowOffset = std::max(0.5F, 1.0F * sizeScale);
  const float barX = imagePos.x + barPad;
  const float barW = drawW - barPad * 2.0F;
  if (barW <= 0.0F) {
    return;
  }

  const float availableW =
      barW - style.ItemSpacing.x * static_cast<float>(slotCount - 1) - barInset * 2.0F;
  if (availableW <= 0.0F) {
    return;
  }
  float iconSize = (availableW / static_cast<float>(slotCount)) - slotPad * 2.0F;
  iconSize = std::clamp(iconSize, minIconSize, maxIconSize);
  if (iconSize <= 0.0F) {
    return;
  }
  const float slotW = iconSize + slotPad * 2.0F;
  const float slotH = slotW;
  const float totalW = slotW * slotCount + style.ItemSpacing.x * static_cast<float>(slotCount - 1);
  const float barH = slotH + barInset * 2.0F;
  const float barY = imagePos.y + drawH - barH - barPad;
  dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(0, 0, 0, 160),
                    rounding);

  ImFont* font = ImGui::GetFont();
  const float numberFontSize = ImGui::GetFontSize() * numberScale;
  float x = barX + std::max(0.0F, (barW - totalW) * 0.5F);
  const float slotY = barY + barInset;
  for (int i = 0; i < slotCount; ++i) {
    const std::string& path = (*model.characterPaths)[i];
    const std::string numberLabel = std::to_string(i + 1);

    const ImVec2 slotPos(x, slotY);
    ImGui::SetCursorScreenPos(slotPos);
    ImGui::PushID(i);
    ImGui::InvisibleButton("##quick_char", ImVec2(slotW, slotH));
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    const bool selected = (path == model.characterPath);
    const ImU32 fill = selected ? IM_COL32(80, 140, 220, 190)
                                : (hovered ? IM_COL32(90, 90, 90, 180) : IM_COL32(60, 60, 60, 150));
    const ImU32 border = selected ? IM_COL32(170, 210, 255, 230) : IM_COL32(20, 20, 20, 220);

    dl->AddRectFilled(slotPos, ImVec2(slotPos.x + slotW, slotPos.y + slotH), fill, rounding);
    dl->AddRect(slotPos, ImVec2(slotPos.x + slotW, slotPos.y + slotH), border, rounding);

    const float numX = slotPos.x + slotPad;
    const float numY = slotPos.y + slotPad * 0.25F;
    dl->AddText(font, numberFontSize, ImVec2(numX + shadowOffset, numY + shadowOffset),
                IM_COL32(0, 0, 0, 200), numberLabel.c_str());
    dl->AddText(font, numberFontSize, ImVec2(numX, numY), IM_COL32(255, 255, 255, 220),
                numberLabel.c_str());

    const float iconX = slotPos.x + slotPad;
    const float iconY = slotPos.y + slotPad;
    const ImVec2 iconP0(iconX, iconY);
    const ImVec2 iconP1(iconX + iconSize, iconY + iconSize);

    SDL_Texture* tex = nullptr;
    ImVec2 uv0{};
    ImVec2 uv1{};
    const CharacterIconSpec& spec = iconSpecForPath(path);
    if (resolveSpriteThumb(sprites, spec, tex, uv0, uv1)) {
      dl->AddImage(textureIdFor(tex), iconP0, iconP1, uv0, uv1);
    } else {
      const ImU32 slotBg = IM_COL32(30, 30, 30, 200);
      dl->AddRectFilled(iconP0, iconP1, slotBg, rounding);
      dl->AddRect(iconP0, iconP1, border, rounding);
    }

    if (clicked) {
      out.swapCharacterInPlace = true;
      out.swapCharacterPath = path;
    }

    x += slotW + style.ItemSpacing.x;
  }

  ImGuiIO& io = ImGui::GetIO();
  if (!io.WantTextInput) {
    static constexpr ImGuiKey keys[] = {ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4, ImGuiKey_5};
    for (int i = 0; i < slotCount; ++i) {
      if (ImGui::IsKeyPressed(keys[i], false)) {
        out.swapCharacterInPlace = true;
        out.swapCharacterPath = (*model.characterPaths)[i];
      }
    }
  }
}

// NOLINTNEXTLINE
static void drawStageInspectorContents(const DebugUIStageInspectorModel& model) {
  auto parseHexColor = [](const std::string& value, ImVec4& out) -> bool {
    if (value.size() != 6)
      return false;
    auto hex = [](char c) -> int {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
      return -1;
    };
    int r1 = hex(value[0]);
    int r2 = hex(value[1]);
    int g1 = hex(value[2]);
    int g2 = hex(value[3]);
    int b1 = hex(value[4]);
    int b2 = hex(value[5]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
      return false;

    const int r = (r1 << 4) | r2;
    const int g = (g1 << 4) | g2;
    const int b = (b1 << 4) | b2;
    out = ImVec4(static_cast<float>(r) / 255.0F, static_cast<float>(g) / 255.0F,
                 static_cast<float>(b) / 255.0F, 1.0F);
    return true;
  };

  if (!model.stageDisplay.empty()) {
    ImGui::PushTextWrapPos(0.0F);
    if (!model.stageId.empty())
      ImGui::TextWrapped("name: %s  (id: %s)", model.stageDisplay.c_str(), model.stageId.c_str());
    else
      ImGui::TextWrapped("name: %s", model.stageDisplay.c_str());
    ImGui::PopTextWrapPos();
  } else if (!model.stageId.empty()) {
    ImGui::Text("id: %s", model.stageId.c_str());
  }

  if (model.stageVersion > 0)
    ImGui::Text("version: %d", model.stageVersion);

  if (!model.stagePath.empty()) {
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextWrapped("stage: %s", Paths::fileStem(model.stagePath).c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", model.stagePath.c_str());
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
  } else {
    ImGui::TextUnformatted("stage: (none)");
  }

  ImGui::Separator();
  ImGui::Text("solids: %llu", static_cast<unsigned long long>(model.solidCount));
  if (model.slopeCount > 0) {
    ImGui::Text("slopes: %llu", static_cast<unsigned long long>(model.slopeCount));
  }

  ImGui::Separator();
  ImGui::TextUnformatted("render:");
  if (ImGui::BeginTable("##render_table", 3, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("key");
    ImGui::TableSetupColumn("hex");
    ImGui::TableSetupColumn("swatch");

    auto row = [&](const char* key, const std::string& value) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(key);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(value.c_str());
      ImGui::TableSetColumnIndex(2);
      ImVec4 col{1.0F, 1.0F, 1.0F, 1.0F};
      const bool ok = parseHexColor(value, col);
      const std::string id = std::string("##swatch_") + key;
      const ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoTooltip;
      if (ok) {
        (void)ImGui::ColorButton(id.c_str(), col, flags, ImVec2(18.0F, 18.0F));
      } else {
        ImGui::TextUnformatted("-");
      }
    };

    row("bg_top", model.renderBgTopHex);
    row("bg_bottom", model.renderBgBottomHex);
    row("platform_base", model.renderPlatformBaseHex);
    row("platform_light", model.renderPlatformLightHex);
    row("platform_dark", model.renderPlatformDarkHex);
    row("platform_highlight", model.renderPlatformHighlightHex);

    ImGui::EndTable();
  }

  if (ImGui::Button("Copy [render] snippet")) {
    std::string snippet;
    snippet.reserve(256);
    snippet += "[render]\n";
    snippet += "bg_top = \"" + model.renderBgTopHex + "\"\n";
    snippet += "bg_bottom = \"" + model.renderBgBottomHex + "\"\n";
    snippet += "platform_base = \"" + model.renderPlatformBaseHex + "\"\n";
    snippet += "platform_light = \"" + model.renderPlatformLightHex + "\"\n";
    snippet += "platform_dark = \"" + model.renderPlatformDarkHex + "\"\n";
    snippet += "platform_highlight = \"" + model.renderPlatformHighlightHex + "\"\n";
    ImGui::SetClipboardText(snippet.c_str());
  }
  ImGui::SameLine();
  ImGui::TextDisabled("copies current values to clipboard");

  ImGui::Separator();
  ImGui::TextUnformatted("collision:");
  ImGui::Text("ground_snap: %.2F", model.collisionGroundSnap);
  ImGui::Text("step_up: %.2F", model.collisionStepUp);
  ImGui::Text("skin: %.3F", model.collisionSkin);
  if (ImGui::Button("Copy [collision] snippet")) {
    char buf[192] = {};
    std::snprintf(buf, sizeof(buf),
                  "[collision]\n"
                  "ground_snap = %.2F\n"
                  "step_up = %.2F\n"
                  "skin = %.3F\n",
                  model.collisionGroundSnap, model.collisionStepUp, model.collisionSkin);
    ImGui::SetClipboardText(buf);
  }

  if (model.hasWorldBounds)
    ImGui::Text("world bounds: (%.0F,%.0F) %.0fx%.0F", model.worldX, model.worldY, model.worldW,
                model.worldH);
  if (model.hasCameraBounds)
    ImGui::Text("camera bounds: (%.0F,%.0F) %.0fx%.0F", model.cameraX, model.cameraY, model.cameraW,
                model.cameraH);
  if (model.hasCameraFollow) {
    ImGui::Text("deadzone: %.0fx%.0F", model.cameraDeadzoneW, model.cameraDeadzoneH);
    ImGui::Text("lookahead: (%.0F, %.0F)", model.cameraLookaheadX, model.cameraLookaheadY);
  }

  if (ImGui::Button("Copy [camera] snippet")) {
    char buf[256] = {};
    int n = std::snprintf(buf, sizeof(buf),
                          "[camera]\n"
                          "deadzone_w = %.0F\n"
                          "deadzone_h = %.0F\n"
                          "lookahead_x = %.0F\n"
                          "lookahead_y = %.0F\n",
                          model.cameraDeadzoneW, model.cameraDeadzoneH, model.cameraLookaheadX,
                          model.cameraLookaheadY);
    if (n < 0)
      n = 0;

    if (model.hasCameraBounds && n < static_cast<int>(sizeof(buf))) {
      (void)std::snprintf(buf + n, sizeof(buf) - static_cast<std::size_t>(n),
                          "\n"
                          "[camera.bounds]\n"
                          "x = %.2F\n"
                          "y = %.2F\n"
                          "w = %.2F\n"
                          "h = %.2F\n",
                          model.cameraX, model.cameraY, model.cameraW, model.cameraH);
    }
    ImGui::SetClipboardText(buf);
  }

  bool canCopyWorld = model.hasWorldBounds && model.worldW > 0.0F && model.worldH > 0.0F;
  if (!canCopyWorld)
    ImGui::BeginDisabled();
  if (ImGui::Button("Copy [world] snippet")) {
    char buf[96] = {};
    std::snprintf(buf, sizeof(buf),
                  "[world]\n"
                  "width = %.0F\n"
                  "height = %.0F\n",
                  model.worldW, model.worldH);
    ImGui::SetClipboardText(buf);
  }
  if (!canCopyWorld)
    ImGui::EndDisabled();

  if (model.spawns && !model.spawns->empty()) {
    ImGui::Separator();
    ImGui::TextUnformatted("spawns:");
    ImGui::BeginChild("spawns", ImVec2(0.0F, 160.0F), true);

    auto appendTomlKey = [](std::string& out, const std::string& key) {
      auto isBareKeyChar = [](unsigned char c) -> bool {
        if (c >= '0' && c <= '9')
          return true;
        if (c >= 'a' && c <= 'z')
          return true;
        if (c >= 'A' && c <= 'Z')
          return true;
        if (c == '_' || c == '-')
          return true;
        return false;
      };

      bool needsQuotes = false;
      if (key.empty())
        needsQuotes = true;
      for (unsigned char c : key) {
        if (!isBareKeyChar(c)) {
          needsQuotes = true;
          break;
        }
      }

      if (!needsQuotes) {
        out += key;
        return;
      }

      out += '"';
      for (const char c : key) {
        if (c == '"' || c == '\\')
          out += '\\';
        out += c;
      }
      out += '"';
    };

    auto copySpawnSnippet = [&](const DebugUIStageInspectorModel::SpawnInfo& sp) {
      std::string snippet;
      snippet.reserve(128 + sp.name.size());
      snippet += "[spawns.";
      appendTomlKey(snippet, sp.name);
      snippet += "]\n";
      const int facing = (sp.facingX < 0) ? -1 : 1;
      char buf[192] = {};
      std::snprintf(buf, sizeof(buf), "x = %.2F\ny = %.2F\nfacing = %d\n", sp.x, sp.y, facing);
      snippet += buf;
      ImGui::SetClipboardText(snippet.c_str());
    };

    if (ImGui::BeginTable("##spawn_table", 5, ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("name");
      ImGui::TableSetupColumn("x");
      ImGui::TableSetupColumn("y");
      ImGui::TableSetupColumn("facing");
      ImGui::TableSetupColumn("copy");

      for (const auto& sp : *model.spawns) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(sp.name.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.0F", sp.x);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.0F", sp.y);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%s (%d)", (sp.facingX < 0) ? "<" : ">", (sp.facingX < 0) ? -1 : 1);
        ImGui::TableSetColumnIndex(4);
        ImGui::PushID(sp.name.c_str());
        if (ImGui::SmallButton("Copy"))
          copySpawnSnippet(sp);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
          ImGui::BeginTooltip();
          ImGui::Text("Copy [spawns.%s] snippet", sp.name.c_str());
          ImGui::EndTooltip();
        }
        ImGui::PopID();
      }

      ImGui::EndTable();
    }

    ImGui::EndChild();
  } else if (model.spawnPoints != nullptr) {
    ImGui::Separator();
    ImGui::TextUnformatted("spawns:");
    ImGui::BeginChild("spawns", ImVec2(0.0F, 140.0F), true);
    for (const auto& sp : *model.spawnPoints)
      ImGui::BulletText("%s", sp.c_str());
    ImGui::EndChild();
  }
}

// NOLINTNEXTLINE
static void drawPlayerInspectorContents(const DebugUIPlayerInspectorModel& model) {
  const DebugUIOverlayModel& o = model.overlay;

  if (!o.hasPlayer) {
    ImGui::TextUnformatted("player: (none)");
    return;
  }

  ImGui::PushTextWrapPos(0.0F);

  if (!o.playerName.empty())
    ImGui::TextWrapped("player: %u  name: %s", o.playerId, o.playerName.c_str());
  else
    ImGui::TextWrapped("player: %u", o.playerId);

  if (!model.animState.empty())
    ImGui::TextWrapped("anim: %s", model.animState.c_str());
  if (!model.movementState.empty())
    ImGui::TextWrapped("state: %s", model.movementState.c_str());

  if (o.hasPos)
    ImGui::TextWrapped("pos: (%.1F, %.1F)", o.posX, o.posY);
  if (o.hasVel)
    ImGui::TextWrapped("vel: (%.1F, %.1F)", o.velX, o.velY);
  if (model.hasMath && o.hasVel && model.subpixel > 1) {
    const int subX = static_cast<int>(std::lround(o.velX * static_cast<float>(model.subpixel)));
    const int subY = static_cast<int>(std::lround(o.velY * static_cast<float>(model.subpixel)));
    ImGui::TextWrapped("vel_sub: (%d, %d) /%d", subX, subY, model.subpixel);
  }
  if (o.hasGrounded)
    ImGui::TextWrapped("grounded: %d", o.grounded ? 1 : 0);
  if (o.hasAabb)
    ImGui::TextWrapped("hitbox: %.1F x %.1F", o.aabbW, o.aabbH);

  if (o.hasTimers) {
    ImGui::TextWrapped("coyote=%d  jumpbuf=%d  action_cd=%d  attack_cd=%d", o.coyoteFrames,
                       o.jumpBufferFrames, o.actionCooldownFrames, o.attackCooldownFrames);
  }

  if (o.hasActionState) {
    ImGui::TextWrapped("dash=%d  air_used=%d", o.dashRemainingFrames, o.airDashesUsed);
    ImGui::TextWrapped("spindash=%d (%d frames)  spin=%d dir=%d", o.spindashCharging ? 1 : 0,
                       o.spindashChargeFrames, o.spinning ? 1 : 0, o.spinFacingX);
    ImGui::TextWrapped("fly=%d  glide=%d", o.flying ? 1 : 0, o.gliding ? 1 : 0);
    const char* wallLabel = "none";
    if (o.walling)
      wallLabel = (o.wallDirX < 0) ? "left" : "right";
    ImGui::TextWrapped("wall=%s  dash_held=%d  wall_kick=%d", wallLabel, o.dashHeld ? 1 : 0,
                       o.dashWallKickFrames);
  }

  if (model.hasPhysics) {
    ImGui::TextWrapped("gravity=%.0F  fall_g_mul=%.2F  eff_g_mul=%.2F", model.gravity,
                       model.fallGravityMultiplier, model.effectiveGravityMultiplier);
  }

  ImGui::PopTextWrapPos();

  if (o.hasPos) {
    ImGui::Separator();
    ImGui::TextUnformatted("Copy:");

    if (ImGui::Button("Copy --spawn args")) {
      char buf[128] = {};
      std::snprintf(buf, sizeof(buf), "--spawn %.2F %.2F", o.posX, o.posY);
      ImGui::SetClipboardText(buf);
    }

    bool canCopyRepro = !model.stageId.empty() && !model.characterId.empty();
    if (!canCopyRepro)
      ImGui::BeginDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Copy CLI repro")) {
      char buf[256] = {};
      std::snprintf(buf, sizeof(buf), "--stage-id %s --character-id %s --spawn %.2F %.2F",
                    model.stageId.c_str(), model.characterId.c_str(), o.posX, o.posY);
      ImGui::SetClipboardText(buf);
    }
    if (!canCopyRepro)
      ImGui::EndDisabled();

    ImGui::TextDisabled("useful for filing bugs / sharing repro positions");

    if (ImGui::Button("Copy [spawns.debug] snippet")) {
      const int facing = model.hasFacing ? model.facingX : 1;
      char buf[192] = {};
      std::snprintf(buf, sizeof(buf),
                    "[spawns.debug]\n"
                    "x = %.2F\n"
                    "y = %.2F\n"
                    "facing = %d\n",
                    o.posX, o.posY, (facing < 0) ? -1 : 1);
      ImGui::SetClipboardText(buf);
    }
  }
}
#endif

bool DebugUI::init(SDL_Window* window, SDL_Renderer* renderer) {
#if SANDBOX_WITH_IMGUI
  if (initialized_)
    return true;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  iniPath_.clear();
  using PrefPathPtr = std::unique_ptr<char, decltype(&SDL_free)>;
  PrefPathPtr prefPath{SDL_GetPrefPath("sdl3-sandbox", "sandbox"), SDL_free};
  if (prefPath) {
    iniPath_ = std::string(prefPath.get()) + "imgui.ini";
    io.IniFilename = iniPath_.c_str();  // persist layout outside the repo
  } else {
    io.IniFilename = nullptr;
  }
  io.LogFilename = nullptr;

  if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
    ImGui::DestroyContext();
    return false;
  }

  if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    return false;
  }

  uiSprites_.init(renderer);
  initialized_ = true;
  return true;
#else
  (void)window;
  (void)renderer;
  return false;
#endif
}

void DebugUI::shutdown() {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return;
  uiSprites_.shutdown();
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  initialized_ = false;
#endif
}

void DebugUI::processEvent(const SDL_Event& e) {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return;
  (void)ImGui_ImplSDL3_ProcessEvent(&e);
#else
  (void)e;
#endif
}

void DebugUI::beginFrame() {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return;
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
#endif
}

void DebugUI::endFrame(SDL_Renderer* renderer) {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return;
  ImGui::Render();
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
#else
  (void)renderer;
#endif
}

bool DebugUI::wantCaptureKeyboard() const {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return false;
  return ImGui::GetIO().WantCaptureKeyboard;
#else
  return false;
#endif
}

bool DebugUI::wantCaptureMouse() const {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return false;
  return ImGui::GetIO().WantCaptureMouse;
#else
  return false;
#endif
}

// NOLINTNEXTLINE
void DebugUI::drawOverlay(const DebugUIOverlayModel& model) {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return;

  ImGui::SetNextWindowPos(ImVec2(16.0F, 200.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.75F);

  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

  if (ImGui::Begin("Overlay", nullptr, flags)) {
    ImGui::Text("frame: %llu  dt: %.5F", static_cast<unsigned long long>(model.frame), model.dt);
    ImGui::Text("debug: overlay=%d collision=%d  (Ctrl-1, Ctrl-2)", model.debugOverlay ? 1 : 0,
                model.debugCollision ? 1 : 0);
    ImGui::TextUnformatted("ui: Ctrl-H hide/show  quit: Ctrl-C");
    ImGui::Text("camera: (%.1F, %.1F)", model.camX, model.camY);

    if (model.hasPlayer) {
      if (!model.playerName.empty())
        ImGui::Text("player: %u  name: %s", model.playerId, model.playerName.c_str());
      else
        ImGui::Text("player: %u", model.playerId);

      if (model.hasPos)
        ImGui::Text("pos: (%.1F, %.1F)", model.posX, model.posY);
      if (model.hasVel)
        ImGui::Text("vel: (%.1F, %.1F)", model.velX, model.velY);
      if (model.hasGrounded)
        ImGui::Text("grounded: %d", model.grounded ? 1 : 0);

      if (model.hasTimers) {
        ImGui::Text("coyote=%d  jumpbuf=%d  action_cd=%d", model.coyoteFrames,
                    model.jumpBufferFrames, model.actionCooldownFrames);
      }

      if (model.hasActionState) {
        ImGui::Text("action: dash=%d air_used=%d  spindash=%d(%d)  spin=%d  glide=%d",
                    model.dashRemainingFrames, model.airDashesUsed, model.spindashCharging ? 1 : 0,
                    model.spindashChargeFrames, model.spinning ? 1 : 0, model.gliding ? 1 : 0);
      }
    }
  }
  ImGui::End();
#else
  (void)model;
#endif
}

void DebugUI::drawStageInspector(const DebugUIStageInspectorModel& model) {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return;

  ImGui::SetNextWindowPos(ImVec2(560.0F, 16.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.90F);

  const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
  if (ImGui::Begin("Stage", nullptr, flags)) {
    drawStageInspectorContents(model);
  }
  ImGui::End();
#else
  (void)model;
#endif
}

void DebugUI::drawPlayerInspector(const DebugUIPlayerInspectorModel& model) {
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return;

  ImGui::SetNextWindowPos(ImVec2(560.0F, 260.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.90F);

  const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
  if (ImGui::Begin("Player", nullptr, flags)) {
    drawPlayerInspectorContents(model);
  }
  ImGui::End();
#else
  (void)model;
#endif
}

// NOLINTNEXTLINE
DebugUIPanelsResult DebugUI::drawPanels(int windowW,
                                        int windowH,
                                        const DebugUIMenuModel& menuModel,
                                        const DebugUIStageInspectorModel& stageModel,
                                        const DebugUIPlayerInspectorModel& playerModel,
                                        SDL_Texture* gameTexture,
                                        int gameTextureW,
                                        int gameTextureH,
                                        const DebugUIControlsModel& controlsModel) {
  DebugUIPanelsResult out{};
#if SANDBOX_WITH_IMGUI
  if (!initialized_)
    return out;

  if (windowW <= 0 || windowH <= 0)
    return out;

  static bool dockBuilt = false;

  ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport)
    return out;

  const ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
  const ImGuiWindowFlags dockspaceWindowFlags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

  ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::Begin("##sandbox_dockspace", nullptr, dockspaceWindowFlags);
  ImGui::PopStyleVar(3);

  const ImGuiID dockspaceId = ImGui::GetID("SandboxDockSpace");
  ImGui::DockSpace(dockspaceId, ImVec2(0.0F, 0.0F), dockFlags);
  ImGui::End();

  if (!dockBuilt) {
    dockBuilt = true;

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, dockFlags | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->Size);

    ImGuiID dockMain = dockspaceId;
    ImGuiID dockLeft =
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.25F, nullptr, &dockMain);
    ImGuiID dockRight =
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25F, nullptr, &dockMain);

    ImGuiID dockLeftBottom =
        ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, 0.40F, nullptr, &dockLeft);
    ImGuiID dockRightBottom =
        ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.40F, nullptr, &dockRight);

    ImGui::DockBuilderDockWindow("Sandbox", dockLeft);
    ImGui::DockBuilderDockWindow("Stage", dockLeftBottom);
    ImGui::DockBuilderDockWindow("Controls", dockRight);
    ImGui::DockBuilderDockWindow("Player", dockRightBottom);
    ImGui::DockBuilderDockWindow("Game", dockMain);

    ImGui::DockBuilderFinish(dockspaceId);
  }

  if (ImGui::Begin("Sandbox")) {
    ImGui::TextDisabled("Ctrl-H: hide UI");
    out.actions = drawSandboxControls(&uiSprites_, menuModel, stageModel, false, false, true);
  }
  ImGui::End();

  if (ImGui::Begin("Stage")) {
    drawStageInspectorContents(stageModel);
  }
  ImGui::End();

  if (ImGui::Begin("Player")) {
    drawPlayerInspectorContents(playerModel);
  }
  ImGui::End();

  if (ImGui::Begin("Controls")) {
    if (!controlsModel.characterLabel.empty())
      ImGui::Text("character: %s", controlsModel.characterLabel.c_str());

    if (!controlsModel.legend.empty()) {
      ImGui::Separator();
      ImGui::TextUnformatted("Legend:");
      ImGui::PushTextWrapPos(0.0F);
      for (const auto& line : controlsModel.legend) {
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", line.c_str());
      }
      ImGui::PopTextWrapPos();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Actions:");
    if (controlsModel.actions.empty()) {
      ImGui::TextUnformatted("(none)");
    } else {
      ImGui::PushTextWrapPos(0.0F);
      for (const auto& line : controlsModel.actions) {
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", line.c_str());
      }
      ImGui::PopTextWrapPos();
    }
  }
  ImGui::End();

  if (out.actions.resetLayoutAndPrefs) {
    ImGui::LoadIniSettingsFromMemory("");
    dockBuilt = false;
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  const ImGuiWindowFlags gameFlags = ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoScrollWithMouse |
                                     ImGuiWindowFlags_NoBackground;
  if (ImGui::Begin("Game", nullptr, gameFlags)) {
    ImVec2 gamePos = ImGui::GetCursorScreenPos();
    ImVec2 gameSize = ImGui::GetContentRegionAvail();
    const int gx = std::clamp(static_cast<int>(gamePos.x), 0, windowW);
    const int gy = std::clamp(static_cast<int>(gamePos.y), 0, windowH);
    const int availW = std::max(0, std::min(static_cast<int>(gameSize.x), windowW - gx));
    const int availH = std::max(0, std::min(static_cast<int>(gameSize.y), windowH - gy));
    out.gameAvailW = availW;
    out.gameAvailH = availH;

    if ((gameTexture != nullptr) && gameTextureW > 0 && gameTextureH > 0 && availW > 0 &&
        availH > 0) {
      const float texW = static_cast<float>(gameTextureW);
      const float texH = static_cast<float>(gameTextureH);
      const float maxScaleX = static_cast<float>(availW) / texW;
      const float maxScaleY = static_cast<float>(availH) / texH;
      const float rawScale = std::min(maxScaleX, maxScaleY);
      const int scaleIntX = availW / gameTextureW;
      const int scaleIntY = availH / gameTextureH;
      const int scaleInt = std::min(scaleIntX, scaleIntY);
      float scale = rawScale;
      bool forcedInteger = false;
      if (menuModel.integerScaleOnly && scaleInt >= 1) {
        scale = static_cast<float>(scaleInt);
        forcedInteger = true;
      }

      // Proportional scaling to fill the window region. Use linear sampling when scaling is not an
      // integer multiple to avoid harsh aliasing (especially with procedural forms).
      const float rounded = std::round(scale);
      const bool isIntegerScale =
          forcedInteger || ((scale >= 1.0F) && (std::fabs(scale - rounded) < 1e-3F));
      (void)SDL_SetTextureScaleMode(gameTexture,
                                    isIntegerScale ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);

      const float drawW = texW * scale;
      const float drawH = texH * scale;

      const ImVec2 cursor = ImGui::GetCursorPos();
      const float offX = std::max(0.0F, (static_cast<float>(availW) - drawW) * 0.5F);
      const float offY = std::max(0.0F, (static_cast<float>(availH) - drawH) * 0.5F);
      ImGui::SetCursorPos(ImVec2(cursor.x + offX, cursor.y + offY));

      const ImVec2 imagePos = ImGui::GetCursorScreenPos();
      out.gameViewport = SDL_Rect{static_cast<int>(imagePos.x), static_cast<int>(imagePos.y),
                                  static_cast<int>(drawW), static_cast<int>(drawH)};

      ImGui::Image(textureIdFor(gameTexture), ImVec2(drawW, drawH));

      char hud[128] = {};
      std::snprintf(hud, sizeof(hud), "%dx%d  scale %.2fx  %s", gameTextureW, gameTextureH, scale,
                    isIntegerScale ? "nearest" : "linear");
      ImDrawList* dl = ImGui::GetWindowDrawList();
      const ImVec2 textSize = ImGui::CalcTextSize(hud);
      const ImVec2 pad = ImVec2(6.0F, 4.0F);
      const ImVec2 p0 = ImVec2(gamePos.x + 8.0F, gamePos.y + 8.0F);
      const ImVec2 bg0 = p0;
      const ImVec2 bg1 = ImVec2(p0.x + textSize.x + pad.x * 2.0F, p0.y + textSize.y + pad.y * 2.0F);
      dl->AddRectFilled(bg0, bg1, IM_COL32(0, 0, 0, 140), 4.0F);
      dl->AddText(ImVec2(p0.x + pad.x, p0.y + pad.y), IM_COL32(255, 255, 255, 210), hud);

      drawQuickCharacterBar(&uiSprites_, menuModel, imagePos, drawW, drawH, out.actions);
    } else {
      ImGui::Dummy(gameSize);
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
#else
  (void)windowW;
  (void)windowH;
  (void)menuModel;
  (void)stageModel;
  (void)playerModel;
  (void)gameTexture;
  (void)gameTextureW;
  (void)gameTextureH;
  (void)controlsModel;
#endif
  return out;
}
