#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <json.hpp>
#include "Vector2.h"
#include "Vector4.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// 正規化寿命 [0,1] に対する 1 次元の値カーブ。
/// 任意個のキー（時刻 t, 値 v）を持ち、Linear / Smooth(smoothstep) で補間する。
/// UpdateSizeOverLifetime などが「寿命でサイズ変化」を自由曲線で表現するために使用。
/// キーは常に t 昇順で保持する（編集時に順序が入れ替わらないよう隣接キーでクランプ）。
/// </summary>
struct ParticleCurve {
    enum class Interp { Linear = 0, Smooth = 1 };

    struct Key {
        float t;  // 0.0〜1.0
        float v;  // 値
    };

    std::vector<Key> keys_ = { {0.0f, 1.0f}, {1.0f, 0.0f} };
    Interp interp_ = Interp::Smooth;

    /// 正規化時刻 t の値を返す
    float Evaluate(float t) const {
        if (keys_.empty()) return 1.0f;
        if (t <= keys_.front().t) return keys_.front().v;
        if (t >= keys_.back().t)  return keys_.back().v;
        for (size_t i = 0; i + 1 < keys_.size(); ++i) {
            const Key& a = keys_[i];
            const Key& b = keys_[i + 1];
            if (t >= a.t && t <= b.t) {
                float local = (b.t > a.t) ? (t - a.t) / (b.t - a.t) : 0.0f;
                if (interp_ == Interp::Smooth) local = local * local * (3.0f - 2.0f * local);
                return a.v + (b.v - a.v) * local;
            }
        }
        return keys_.back().v;
    }

    void SortKeys() {
        std::sort(keys_.begin(), keys_.end(),
            [](const Key& a, const Key& b) { return a.t < b.t; });
    }

    void SaveToJson(nlohmann::json& json) const {
        json["interp"] = static_cast<int>(interp_);
        nlohmann::json arr = nlohmann::json::array();
        for (const Key& k : keys_) arr.push_back({ {"t", k.t}, {"v", k.v} });
        json["keys"] = arr;
    }

    void LoadFromJson(const nlohmann::json& json) {
        if (json.contains("interp")) interp_ = static_cast<Interp>(static_cast<int>(json["interp"]));
        if (json.contains("keys")) {
            keys_.clear();
            for (const auto& e : json["keys"]) keys_.push_back({ e["t"], e["v"] });
            if (keys_.size() < 2) keys_ = { {0.0f, 1.0f}, {1.0f, 0.0f} };
            SortKeys();
        }
    }

#ifdef USE_IMGUI
    // 転置用（シリアライズ対象外）のドラッグ中キーインデックス
    int dragKey_ = -1;

    /// カーブ編集ウィジェット。編集されたら true を返す。
    bool DrawEditor(const char* label, float vMin = 0.0f, float vMax = 3.0f,
        ImVec2 size = ImVec2(0, 120)) {
        bool changed = false;
        ImGui::PushID(this);
        if (label && label[0]) ImGui::TextUnformatted(label);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (size.x <= 0.0f) size.x = ImGui::GetContentRegionAvail().x;
        if (size.x < 60.0f) size.x = 60.0f;
        if (size.y <= 0.0f) size.y = 120.0f;

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
        ImGui::InvisibleButton("##curvecanvas", size);
        bool hovered = ImGui::IsItemHovered();
        ImVec2 mouse = ImGui::GetIO().MousePos;

        // 背景 + グリッド
        dl->AddRectFilled(p0, p1, IM_COL32(28, 28, 32, 255));
        for (int i = 1; i < 4; ++i) {
            float gx = p0.x + size.x * i / 4.0f;
            dl->AddLine(ImVec2(gx, p0.y), ImVec2(gx, p1.y), IM_COL32(55, 55, 60, 255));
            float gy = p0.y + size.y * i / 4.0f;
            dl->AddLine(ImVec2(p0.x, gy), ImVec2(p1.x, gy), IM_COL32(55, 55, 60, 255));
        }
        dl->AddRect(p0, p1, IM_COL32(90, 90, 100, 255));

        auto toScr = [&](float t, float v) {
            float ny = (vMax > vMin) ? (v - vMin) / (vMax - vMin) : 0.0f;
            ny = std::clamp(ny, 0.0f, 1.0f);
            return ImVec2(p0.x + std::clamp(t, 0.0f, 1.0f) * size.x, p1.y - ny * size.y);
            };

        // カーブ本体
        const int kSamples = 64;
        ImVec2 prev;
        for (int i = 0; i <= kSamples; ++i) {
            float t = static_cast<float>(i) / kSamples;
            ImVec2 s = toScr(t, Evaluate(t));
            if (i > 0) dl->AddLine(prev, s, IM_COL32(120, 200, 255, 255), 2.0f);
            prev = s;
        }

        // ドラッグ中キーの移動
        const float kR = 6.0f;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            dragKey_ >= 0 && dragKey_ < static_cast<int>(keys_.size())) {
            float t = (mouse.x - p0.x) / size.x;
            float v = vMin + ((p1.y - mouse.y) / size.y) * (vMax - vMin);
            v = std::clamp(v, vMin, vMax);
            if (dragKey_ == 0) t = 0.0f;
            else if (dragKey_ == static_cast<int>(keys_.size()) - 1) t = 1.0f;
            else t = std::clamp(t, keys_[dragKey_ - 1].t + 0.001f, keys_[dragKey_ + 1].t - 0.001f);
            keys_[dragKey_].t = t;
            keys_[dragKey_].v = v;
            changed = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragKey_ = -1;

        // ハンドル描画 + 選択 / 削除
        for (int i = 0; i < static_cast<int>(keys_.size()); ++i) {
            ImVec2 s = toScr(keys_[i].t, keys_[i].v);
            bool isNear = std::fabs(mouse.x - s.x) < kR + 3 && std::fabs(mouse.y - s.y) < kR + 3;
            ImU32 col = (i == dragKey_) ? IM_COL32(255, 220, 120, 255)
                : (isNear ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 220, 255, 255));
            dl->AddCircleFilled(s, kR, col);
            dl->AddCircle(s, kR, IM_COL32(0, 0, 0, 255));
            if (hovered && isNear && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) dragKey_ = i;
            if (hovered && isNear && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
                i != 0 && i != static_cast<int>(keys_.size()) - 1 && keys_.size() > 2) {
                keys_.erase(keys_.begin() + i);
                changed = true;
                break;
            }
        }

        // ダブルクリックで点追加
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            float t = std::clamp((mouse.x - p0.x) / size.x, 0.0f, 1.0f);
            keys_.push_back({ t, Evaluate(t) });
            SortKeys();
            changed = true;
        }

        int mode = static_cast<int>(interp_);
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("補間", &mode, "Linear\0Smooth\0")) {
            interp_ = static_cast<Interp>(mode);
            changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Wクリック:点追加 / 右クリック:点削除");

        ImGui::PopID();
        return changed;
    }
#endif
};

