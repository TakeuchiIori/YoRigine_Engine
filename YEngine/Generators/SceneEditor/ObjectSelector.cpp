#include "ObjectSelector.h"
#include "PickBuffer.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "ImGuizmo.h"
#include "Editor/Editor.h"
#endif

namespace YoRigine {

    void ObjectSelector::SelectObject(int id, bool multiSelect)
    {
        if (!IsValidId(id)) return;
        if (multiSelect) {
            if (IsSelected(id)) RemoveFromSelection(id);
            else                AddToSelection(id);
        }
        else {
            ClearSelection();
            AddToSelection(id);
        }
    }

    void ObjectSelector::AddToSelection(int id)
    {
        if (!IsValidId(id)) return;
        selectedObjectIds_.insert(id);
        primaryId_ = id;
    }

    void ObjectSelector::RemoveFromSelection(int id)
    {
        selectedObjectIds_.erase(id);
        if (primaryId_ == id)
            primaryId_ = selectedObjectIds_.empty() ? -1 : *selectedObjectIds_.begin();
    }

    void ObjectSelector::ClearSelection()
    {
        selectedObjectIds_.clear();
        primaryId_ = -1;
    }

    bool ObjectSelector::IsSelected(int id) const
    {
        return selectedObjectIds_.find(id) != selectedObjectIds_.end();
    }

    // =============================================================================
    // Update
    // =============================================================================
#ifdef USE_IMGUI
    void ObjectSelector::Update(
        const ImVec2& viewportPos,
        const ImVec2& viewportSize)
    {
        if (!camera_ || !objectManager_) return;

        // ── フェーズ1: クリックされた瞬間 → RequestPick で座標を登録 ──────────
        //
        // 受け付けるのは「ゲームビューの絵がホバーされている」ときだけ。
        // 以前はビューポートの矩形とマウス座標の内外判定だけで決めていたため、
        // ゲームビューの上に浮いた ImGui ウィンドウをクリックすると、その奥の
        // オブジェクトまで選択されていた。ホバー判定は ImGui の前後関係を知っているので、
        // 手前のウィンドウ・ポップアップ・ギズモのハンドルの上では自動的に弾かれる。
        //
        // クリックも ImGui の入力から取る。ホバーと同じ入力源で判定しないと、
        // 片方だけ 1 フレームずれた時にすり抜けが起きる。
        const bool clicked =
            Editor::GetInstance()->CanPickInGameView() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        if (clicked) {
            if (pickBuffer_) {
                const ImVec2 mouse = ImGui::GetMousePos();
                // ゲームビュー左上を原点とした相対座標に変換して渡す。
                // PickBuffer のビューポートは SetViewport() でゲームビューサイズに
                // 合わせているため、ここもゲームビュー相対座標で渡す必要がある。
                float relX = mouse.x - viewportPos.x;
                float relY = mouse.y - viewportPos.y;

                // PickBuffer にビューポートサイズも伝えてビューポートを更新する
                pickBuffer_->SetViewport(
                    static_cast<uint32_t>(viewportSize.x),
                    static_cast<uint32_t>(viewportSize.y));

                pickBuffer_->RequestPick(relX, relY);
                pendingMultiSelect_ = ImGui::GetIO().KeyShift;
                hasPendingRead_ = true;
            }
        }

        // ── フェーズ2: 前フレームのクリックの結果を読み取る ───────────────────
        // マウスがビューポートの外へ出ていても読む。読まずに返すと、
        // 古い結果が次のクリックの結果として適用されてしまう。
        if (hasPendingRead_ && pickBuffer_) {
            int pickedId = pickBuffer_->ReadPickResult();
            if (pickedId != -1) {
                // 読み取り時点でギズモが Hover/Using ならピック結果を破棄。
                // フェーズ1のクリック登録 → 結果到着までの 1 フレーム間に
                // ユーザがギズモにマウスを乗せた/掴んだケースをここで救う。
                if (!ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
                    SelectObject(pickedId, pendingMultiSelect_);
                }
                hasPendingRead_ = false;
            }
            else if (!clicked) {
                // クリックがなく ReadPickResult も -1 → 空選択
                // （readbackReady_ が false = まだ結果が来ていない場合は何もしない）
                // readbackReady_ は ReadPickResult 内で false にリセットされるので
                // ここでは何フレームか -1 が続いても待つだけでよい
            }
        }
    }
#endif

    // =============================================================================
    // PerformGPUPick（外部から直接呼ぶ場合のラッパー、通常は Update() 経由）
    // =============================================================================
# ifdef USE_IMUI
    void ObjectSelector::PerformGPUPick(float screenX, float screenY)
    {
        if (!pickBuffer_) return;
        pickBuffer_->RequestPick(screenX, screenY);
    }

#endif

} // namespace YoRigine