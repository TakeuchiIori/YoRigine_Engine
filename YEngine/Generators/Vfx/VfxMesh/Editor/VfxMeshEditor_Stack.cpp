#ifdef USE_IMGUI
// ===========================================================
// VfxMeshEditor_Stack.cpp
//
// 編集パネルの「スタック（項目一覧）＋インスペクタ（選んだ 1 件の詳細）」。
// CPU / GPU パーティクルエディタと同じ骨格:
//   ⚙ プレビュー設定
//   ── 軌跡 ──        Trail
//   ── エレメント ──  形状 × マテリアル / 特殊エフェクト（何個でも）
//   ── 全体の動き ──  モジュール（何個でも）
// ===========================================================
#include "VfxMeshEditor.h"

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>
#include "Core/Editor/Widgets/YEditorWidget.h"

namespace YoRigine {

    // ModuleUI.cpp で定義
    const char* VfxModuleTypeLabel(VfxModuleType type);
    const char* VfxModuleTargetLabel(VfxModuleTarget target);

    namespace {
        constexpr const char* kElementPayload = "VFX_ELEMENT";
        constexpr const char* kModulePayload  = "VFX_MODULE";

        // vector の要素を from → to へ移動する（並べ替え用）
        template <class T>
        void MoveElement(std::vector<T>& v, int from, int to) {
            if (from < 0 || to < 0 || from >= static_cast<int>(v.size()) || to >= static_cast<int>(v.size()) || from == to) return;
            T item = std::move(v[from]);
            v.erase(v.begin() + from);
            v.insert(v.begin() + to, std::move(item));
        }
    }

    //=================================================================
    // 再生操作（ツールバーに常に出す。設定はスタックの「プレビュー設定」）
    //=================================================================

    void VfxMeshEditor::DrawPlaybackControls()
    {
        if (previewPlaying_) {
            if (ImGui::Button(ICON_FA_STOP)) {
                previewPlaying_ = false;
                if (previewTrailEmitter_) previewTrailEmitter_->Stop();
            }
            YEditorWidget::ItemTooltip("停止");
        } else {
            if (ImGui::Button(ICON_FA_PLAY)) {
                previewPlaying_ = true;
                previewTimer_ = 0.f;
                if (previewTrailEmitter_) previewTrailEmitter_->Play();
            }
            YEditorWidget::ItemTooltip("再生");
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_REDO_ALT)) {
            previewTimer_ = 0.f;
            if (previewTrailEmitter_) previewTrailEmitter_->Play();
        }
        YEditorWidget::ItemTooltip("最初からもう一度");
        ImGui::SameLine();
        if (ImGui::Checkbox("ループ", &loopOneShot_)) {
            previewTimer_ = 0.f;
            if (previewTrailEmitter_) previewTrailEmitter_->Play();
        }
        YEditorWidget::ItemTooltip("ON: 1 回ずつ繰り返し Emit / OFF: 1 回だけ再生して止まる");
        ImGui::SameLine();
        if (burstMode_) {
            char ov[32];
            std::snprintf(ov, sizeof(ov), "%.0f%%", std::max(burstProgress_, 0.f) * 100.f);
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
            ImGui::ProgressBar(std::max(burstProgress_, 0.f), ImVec2(ImGui::GetFontSize() * 6.0f, 0), ov);
        } else {
            ImGui::TextDisabled("%.2f 秒", previewTimer_);
        }
    }

    //=================================================================
    // スタック（中）
    //=================================================================

    void VfxMeshEditor::DrawStackPanel()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& asset = sel->asset;

        // 行 1 本: 有効チェック（任意）＋ 名前（選択）＋ 右側の要約
        auto row = [&](VfxStackItem item, int index, bool* enabled, const char* enableLabel,
                       const std::string& name, const std::string& summary) {
            ImGui::PushID(static_cast<int>(item) * 1000 + index);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (enabled) {
                // 押されたときだけ「変更前」を作る（毎フレーム行ごとにアセットを丸ごとコピーしない）
                const bool prev = *enabled;
                if (ImGui::Checkbox("##en", enabled)) {
                    const bool now = *enabled;
                    *enabled = prev;
                    VfxEffectAsset b = asset;
                    *enabled = now;
                    CommitChange(b, enableLabel);
                }
            }
            // 要約を先に描き、名前（Selectable）を行の最後に描く。
            // 呼び出し側の BeginDragDropSource / BeginPopupContextItem は「直前の項目」を対象にするため、
            // 最後が ID の無い Text だとドラッグ時に ImGui の assert で止まる（右クリックも効かない）。
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", summary.c_str());
            ImGui::TableSetColumnIndex(1);
            const bool selected = (stackItem_ == item && stackIndex_ == index);
            if (enabled && !*enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                stackItem_ = item;
                stackIndex_ = index;
            }
            if (enabled && !*enabled) ImGui::PopStyleColor();
            ImGui::PopID();
        };
        auto beginTable = [] {
            if (!ImGui::BeginTable("##rows", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX)) return false;
            ImGui::TableSetupColumn("##en", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##sum", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6.0f);
            return true;
        };

        // ── プレビュー設定 ──
        ImGui::PushID("preview");
        if (beginTable()) {
            row(VfxStackItem::Preview, -1, nullptr, nullptr, ICON_FA_COG " プレビュー設定", "");
            ImGui::EndTable();
        }
        ImGui::PopID();

        // ── 軌跡（Trail） ──
        ImGui::PushID("trail");
        ImGui::SeparatorText("軌跡");
        if (beginTable()) {
            char sum[48];
            std::snprintf(sum, sizeof(sum), "寿命 %.2f 秒", asset.trail.lifetime);
            row(VfxStackItem::Trail, -1, &asset.useTrail, "Trail 有効切替", ICON_FA_WIND " Trail", asset.useTrail ? sum : "無効");
            ImGui::EndTable();
        }
        ImGui::PopID();

        // ── エレメント ──
        ImGui::PushID("elements");
        ImGui::SeparatorText("エレメント");
        auto& subs = asset.elements;
        int moveFrom = -1, moveTo = -1, dupIdx = -1, removeIdx = -1;
        if (!subs.empty() && beginTable()) {
            for (int i = 0; i < static_cast<int>(subs.size()); ++i) {
                auto& sub = subs[i];
                const char* icon = (sub.kind == VfxElementKind::Composed) ? ICON_FA_CUBE " " : ICON_FA_BOLT " ";
                const std::string summary = sub.modules.empty() ? std::string{} : ("動き " + std::to_string(sub.modules.size()));
                row(VfxStackItem::Element, i, &sub.enabled, "エレメント 有効切替", icon + ElementDisplayName(sub), summary);

                // ドラッグで並べ替え（描画順が変わる）
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(kElementPayload, &i, sizeof(int));
                    ImGui::TextUnformatted(ElementDisplayName(sub).c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kElementPayload)) {
                        moveFrom = *static_cast<const int*>(p->Data);
                        moveTo = i;
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginPopupContextItem("##elemMenu")) {
                    stackItem_ = VfxStackItem::Element;
                    stackIndex_ = i;
                    if (ImGui::MenuItem(ICON_FA_CLONE " 複製")) dupIdx = i;
                    if (ImGui::MenuItem(ICON_FA_ARROW_UP " 上へ", nullptr, false, i > 0)) { moveFrom = i; moveTo = i - 1; }
                    if (ImGui::MenuItem(ICON_FA_ARROW_DOWN " 下へ", nullptr, false, i + 1 < static_cast<int>(subs.size()))) { moveFrom = i; moveTo = i + 1; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(ICON_FA_TRASH " 削除")) removeIdx = i;
                    ImGui::EndPopup();
                }
            }
            ImGui::EndTable();
        }
        if (subs.empty()) ImGui::TextDisabled("  (なし)");
        if (ImGui::Button(ICON_FA_PLUS " 追加##elem", ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##addElement");
        if (ImGui::BeginPopup("##addElement")) {
            if (DrawAddElementMenuItems()) {
                stackItem_ = VfxStackItem::Element;
                stackIndex_ = static_cast<int>(subs.size()) - 1;
            }
            ImGui::EndPopup();
        }
        // 構造変更は一覧を描き終えてから（描画中に vector を壊さない）
        if (moveFrom >= 0 && moveTo >= 0) {
            VfxEffectAsset b = asset;
            MoveElement(subs, moveFrom, moveTo);
            CommitChange(b, "エレメント 並べ替え");
            stackItem_ = VfxStackItem::Element;
            stackIndex_ = moveTo;
        }
        if (dupIdx >= 0) {
            VfxEffectAsset b = asset;
            VfxElement copy = subs[dupIdx];
            subs.insert(subs.begin() + dupIdx + 1, std::move(copy));
            CommitChange(b, "エレメント 複製");
            stackItem_ = VfxStackItem::Element;
            stackIndex_ = dupIdx + 1;
        }
        if (removeIdx >= 0) {
            VfxEffectAsset b = asset;
            subs.erase(subs.begin() + removeIdx);
            CommitChange(b, "エレメント 削除"); // Undo で戻せるので確認は出さない
            stackIndex_ = std::min(removeIdx, static_cast<int>(subs.size()) - 1);
            if (stackIndex_ < 0) stackItem_ = VfxStackItem::Trail;
        }
        ImGui::PopID();

        // ── 全体の動き（モジュール） ──
        ImGui::PushID("modules");
        ImGui::SeparatorText("全体の動き");
        YEditorWidget::ItemTooltip("エフェクト全体に効く動き（寿命・移動・脈動など）。ゲームでも同じ動きで再生されます");
        auto& mods = asset.modules;
        moveFrom = moveTo = dupIdx = removeIdx = -1;
        if (!mods.empty() && beginTable()) {
            for (int i = 0; i < static_cast<int>(mods.size()); ++i) {
                const auto& m = mods[i];
                char lifeBuf[32];
                std::snprintf(lifeBuf, sizeof(lifeBuf), "%.2f 秒", m.duration);
                const std::string summary = (m.type == VfxModuleType::Lifetime) ? std::string(lifeBuf) : std::string(VfxModuleTargetLabel(m.target));
                row(VfxStackItem::Module, i, nullptr, nullptr, std::string(ICON_FA_RUNNING " ") + VfxModuleTypeLabel(m.type), summary);

                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(kModulePayload, &i, sizeof(int));
                    ImGui::TextUnformatted(VfxModuleTypeLabel(m.type));
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kModulePayload)) {
                        moveFrom = *static_cast<const int*>(p->Data);
                        moveTo = i;
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginPopupContextItem("##modMenu")) {
                    stackItem_ = VfxStackItem::Module;
                    stackIndex_ = i;
                    if (ImGui::MenuItem(ICON_FA_CLONE " 複製")) dupIdx = i;
                    if (ImGui::MenuItem(ICON_FA_ARROW_UP " 上へ", nullptr, false, i > 0)) { moveFrom = i; moveTo = i - 1; }
                    if (ImGui::MenuItem(ICON_FA_ARROW_DOWN " 下へ", nullptr, false, i + 1 < static_cast<int>(mods.size()))) { moveFrom = i; moveTo = i + 1; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(ICON_FA_TRASH " 削除")) removeIdx = i;
                    ImGui::EndPopup();
                }
            }
            ImGui::EndTable();
        }
        if (mods.empty()) ImGui::TextDisabled("  (なし = 従来挙動)");
        if (ImGui::Button(ICON_FA_PLUS " 追加##mod", ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##addModule");
        {
            VfxEffectAsset b = asset;
            if (DrawAddModuleMenu(mods, "##addModule")) {
                CommitChange(b, "モジュール追加");
                stackItem_ = VfxStackItem::Module;
                stackIndex_ = static_cast<int>(mods.size()) - 1;
            }
        }
        if (moveFrom >= 0 && moveTo >= 0) {
            VfxEffectAsset b = asset;
            MoveElement(mods, moveFrom, moveTo);
            CommitChange(b, "モジュール 並べ替え");
            stackItem_ = VfxStackItem::Module;
            stackIndex_ = moveTo;
        }
        if (dupIdx >= 0) {
            VfxEffectAsset b = asset;
            mods.insert(mods.begin() + dupIdx + 1, mods[dupIdx]);
            CommitChange(b, "モジュール 複製");
            stackItem_ = VfxStackItem::Module;
            stackIndex_ = dupIdx + 1;
        }
        if (removeIdx >= 0) {
            VfxEffectAsset b = asset;
            mods.erase(mods.begin() + removeIdx);
            CommitChange(b, "モジュール 削除");
            stackIndex_ = std::min(removeIdx, static_cast<int>(mods.size()) - 1);
            if (stackIndex_ < 0) stackItem_ = VfxStackItem::Trail;
        }
        ImGui::PopID();
    }

    //=================================================================
    // インスペクタ（右）
    //=================================================================

    void VfxMeshEditor::DrawInspectorPanel()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& asset = sel->asset;

        switch (stackItem_) {
        case VfxStackItem::Preview:
            DrawPreviewSection();
            break;

        case VfxStackItem::Trail:
            ImGui::SeparatorText(ICON_FA_WIND " Trail（軌跡）");
            if (asset.useTrail) {
                DrawTrailSection();
            } else {
                ImGui::TextDisabled("無効です。スタックのチェックで有効にできます。");
            }
            break;

        case VfxStackItem::Element: {
            if (stackIndex_ < 0 || stackIndex_ >= static_cast<int>(asset.elements.size())) {
                stackItem_ = VfxStackItem::Trail;
                break;
            }
            auto& sub = asset.elements[stackIndex_];
            ImGui::SeparatorText((std::string(sub.kind == VfxElementKind::Composed ? ICON_FA_CUBE " " : ICON_FA_BOLT " ")
                + ElementDisplayName(sub)).c_str());
            {
                VfxEffectAsset b = asset;
                if (ImGui::Checkbox("有効", &sub.enabled)) CommitChange(b, "エレメント 有効切替");
            }
            ImGui::PushID(stackIndex_);
            DrawElementInspector(sub);
            ImGui::PopID();
            break;
        }

        case VfxStackItem::Module: {
            if (stackIndex_ < 0 || stackIndex_ >= static_cast<int>(asset.modules.size())) {
                stackItem_ = VfxStackItem::Trail;
                break;
            }
            auto& m = asset.modules[stackIndex_];
            ImGui::SeparatorText((std::string(ICON_FA_RUNNING " 全体の動き: ") + VfxModuleTypeLabel(m.type)).c_str());
            ImGui::PushID(stackIndex_);
            VfxEffectAsset b = asset;
            if (DrawModuleBody(m, /*showTarget=*/true)) CommitChange(b, "モジュール編集");
            ImGui::PopID();
            break;
        }
        }
    }

} // namespace YoRigine
#endif
