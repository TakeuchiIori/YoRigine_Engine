// ===========================================================
// YEditorWidget_Gradient.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Gradient.h"
#include "YEditorWidget_Tooltip.h"
#include <imgui_gradient/imgui_gradient.hpp>
#include <IconsFontAwesome5.h>
#include <algorithm>
#include <list>
#include <unordered_map>

namespace YEditorWidget {

namespace {

// imgui_gradient のウィジェットは「選択中マーカー」などの UI 状態を内部に持つため、
// ImGui ID ごとに 1 つ保持しておく。synced は最後に同期した内容で、
// Undo やファイル読込など外部から書き換えられたことの検出に使う。
struct GradientWidgetCache {
    ImGG::GradientWidget widget;
    ColorGradient        synced;
};

std::unordered_map<ImGuiID, GradientWidgetCache>& GetCache()
{
    static std::unordered_map<ImGuiID, GradientWidgetCache> cache;
    return cache;
}

ImGG::GradientWidget MakeWidget(const ColorGradient& g)
{
    std::list<ImGG::Mark> marks;
    for (const auto& s : g.stops) {
        marks.emplace_back(ImGG::RelativePosition{ std::clamp(s.t, 0.0f, 1.0f) },
                           ImVec4{ s.color.x, s.color.y, s.color.z, s.color.w });
    }
    return ImGG::GradientWidget{ marks };
}

void ReadBack(const ImGG::GradientWidget& widget, ColorGradient& g)
{
    g.stops.clear();
    for (const auto& m : widget.gradient().get_marks()) {
        g.stops.push_back({ m.position.get(), { m.color.x, m.color.y, m.color.z, m.color.w } });
    }
    g.SortStops();
}

// よく使う配色。ゼロから作るより、近いものを選んで直す方が速い
struct GradientPreset {
    const char*                    name;
    std::vector<ColorGradient::Stop> stops;
};

const std::vector<GradientPreset>& GetPresets()
{
    static const std::vector<GradientPreset> presets = {
        { "白 → 透明",         { { 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } }, { 1.0f, { 1.0f, 1.0f, 1.0f, 0.0f } } } },
        { "炎",                { { 0.0f, { 1.0f, 1.0f, 0.8f, 1.0f } }, { 0.3f, { 1.0f, 0.5f, 0.0f, 1.0f } },
                                 { 0.7f, { 0.8f, 0.1f, 0.0f, 0.5f } }, { 1.0f, { 0.2f, 0.0f, 0.0f, 0.0f } } } },
        { "煙",                { { 0.0f, { 0.6f, 0.6f, 0.6f, 0.0f } }, { 0.15f, { 0.5f, 0.5f, 0.5f, 0.6f } },
                                 { 1.0f, { 0.2f, 0.2f, 0.2f, 0.0f } } } },
        { "魔法（白 → 青）",   { { 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } }, { 0.4f, { 0.3f, 0.6f, 1.0f, 1.0f } },
                                 { 1.0f, { 0.1f, 0.1f, 0.6f, 0.0f } } } },
        { "電撃（白 → 紫）",   { { 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } }, { 0.5f, { 0.7f, 0.4f, 1.0f, 1.0f } },
                                 { 1.0f, { 0.3f, 0.0f, 0.6f, 0.0f } } } },
        { "回復（白 → 緑）",   { { 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } }, { 0.5f, { 0.4f, 1.0f, 0.5f, 0.8f } },
                                 { 1.0f, { 0.1f, 0.5f, 0.2f, 0.0f } } } },
        { "フェードイン/アウト", { { 0.0f, { 1.0f, 1.0f, 1.0f, 0.0f } }, { 0.2f, { 1.0f, 1.0f, 1.0f, 1.0f } },
                                 { 0.8f, { 1.0f, 1.0f, 1.0f, 1.0f } }, { 1.0f, { 1.0f, 1.0f, 1.0f, 0.0f } } } },
    };
    return presets;
}

} // namespace

bool Gradient(const char* label, ColorGradient& gradient)
{
    bool changed = false;

    ImGui::PushID(label);
    const ImGuiID id = ImGui::GetID("##gradient");

    // 外部で書き換えられていたら（初回 / Undo / 読込など）ウィジェットを作り直す
    auto& cache = GetCache();
    auto  it    = cache.find(id);
    if (it == cache.end()) {
        it = cache.emplace(id, GradientWidgetCache{ MakeWidget(gradient), gradient }).first;
    } else if (!(it->second.synced == gradient)) {
        it->second.widget = MakeWidget(gradient);
        it->second.synced = gradient;
    }
    GradientWidgetCache& entry = it->second;

    // ── 見出し行: ラベル / 補間方式 / プリセット / 反転 ──────────────
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);

    ImGui::SameLine();
    static const char* kSpaceNames[] = { "RGB補間", "Oklab補間" };
    int spaceIdx = static_cast<int>(gradient.space);
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(kSpaceNames[1]).x + ImGui::GetFrameHeight() * 1.5f);
    if (ImGui::Combo("##space", &spaceIdx, kSpaceNames, IM_ARRAYSIZE(kSpaceNames))) {
        gradient.space = static_cast<ColorGradient::Space>(spaceIdx);
        gradient.MarkDirty();
        changed = true;
    }
    ItemTooltip("Oklab: 途中の色が濁らず、透明へ消えるときも色味が残る（おすすめ）\n"
                "RGB: 値をそのまま直線補間（旧データはこちら）");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PALETTE " プリセット")) ImGui::OpenPopup("##gradientPresets");
    if (ImGui::BeginPopup("##gradientPresets")) {
        for (const auto& preset : GetPresets()) {
            if (ImGui::MenuItem(preset.name)) {
                gradient.stops = preset.stops;
                gradient.MarkDirty();
                changed = true;
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_EXCHANGE_ALT)) {
        for (auto& s : gradient.stops) s.t = 1.0f - s.t;
        gradient.SortStops();
        changed = true;
    }
    ItemTooltip("左右反転");

    // 見出し行で変わったらウィジェット側へ反映してから描く
    if (changed) {
        entry.widget = MakeWidget(gradient);
    }

    // ── 本体 ─────────────────────────────────────────────────────────
    ImGG::Settings settings;
    settings.gradient_width    = 100000.0f; // 実際は利用可能幅いっぱいに収まる
    settings.gradient_height   = 30.0f;
    settings.horizontal_margin = 6.0f;
    // RGB補間は ImGui 標準の多色矩形（=値の直線補間）、Oklab は細かく刻んで実行時の見た目に合わせる
    settings.gradient_subdivisions = (gradient.space == ColorGradient::Space::Oklab) ? 16 : 0;
    settings.flags            = ImGG::Flag::NoLabel | ImGG::Flag::NoResetButton;
    settings.color_edit_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf;

    if (entry.widget.widget("##body", settings)) {
        ColorGradient edited = gradient;
        ReadBack(entry.widget, edited);
        if (!(edited == gradient)) {
            gradient = edited;
            gradient.MarkDirty();
            changed = true;
        }
    }
    ImGui::TextDisabled("クリック: 追加 / ドラッグ: 移動 / 下へドラッグ・Del: 削除");

    entry.synced = gradient;
    ImGui::PopID();
    return changed;
}

} // namespace YEditorWidget
#endif // USE_IMGUI
