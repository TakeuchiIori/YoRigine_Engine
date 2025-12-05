#include "ImGuiControlsHelper.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#ifdef USE_IMGUI

bool ImGuiControlsHelper::DragFloatWithReset(const char* label, float* value, float speed, float min, float max, float defaultValue, const char* format) {
#ifdef USE_IMGUI
    bool changed = ImGui::DragFloat(label, value, speed, min, max, format, ImGuiSliderFlags_AlwaysClamp);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;
#endif // _DEBUG

}

bool ImGuiControlsHelper::DragFloat3WithReset(const char* label, Vector3* value, float speed, float min, float max, const Vector3& defaultValue, const char* format) {
    
#ifdef USE_IMGUI

    bool changed = ImGui::DragFloat3(label, &value->x, speed, min, max, format, ImGuiSliderFlags_AlwaysClamp);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;

#endif // _DEBUG

}

bool ImGuiControlsHelper::DragFloat2WithReset(const char* label, Vector2* value, float speed, float min, float max, const Vector2& defaultValue, const char* format) {
#ifdef USE_IMGUI
    bool changed = ImGui::DragFloat2(label, &value->x, speed, min, max, format, ImGuiSliderFlags_AlwaysClamp);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;

#endif // _DEBUG
}

bool ImGuiControlsHelper::DragIntWithReset(const char* label, int* value, float speed, int min, int max, int defaultValue) {
#ifdef USE_IMGUI
    bool changed = ImGui::DragInt(label, value, speed, min, max);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;
#endif
}

bool ImGuiControlsHelper::InputFloatWithReset(const char* label, float* value, float step, float stepFast, float defaultValue, const char* format) {
#ifdef USE_IMGUI
    bool changed = ImGui::InputFloat(label, value, step, stepFast, format);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;

#endif // _DEBUG
}

bool ImGuiControlsHelper::InputFloat3WithReset(const char* label, Vector3* value, const Vector3& defaultValue, const char* format) {
#ifdef USE_IMGUI
    bool changed = ImGui::InputFloat3(label, &value->x, format);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;

#endif // _DEBUG

}

bool ImGuiControlsHelper::InputFloat2WithReset(const char* label, Vector2* value, const Vector2& defaultValue, const char* format) {
#ifdef USE_IMGUI
    bool changed = ImGui::InputFloat2(label, &value->x, format);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::InputIntWithReset(const char* label, int* value, int step, int stepFast, int defaultValue) {
#ifdef USE_IMGUI
    bool changed = ImGui::InputInt(label, value, step, stepFast);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::SmartSliderFloat(const char* label, float* value, float rangeMin, float rangeMax, float defaultValue, const char* format) {
#ifdef USE_IMGUI
    bool changed = false;

    // 利用可能な幅を計算
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float sliderWidth = availableWidth * 0.55f;
    float inputWidth = availableWidth * 0.25f;

    // スライダー部分
    ImGui::PushItemWidth(sliderWidth);
    if (ImGui::SliderFloat(("##slider_" + std::string(label)).c_str(), value, rangeMin, rangeMax, format)) {
        changed = true;
    }
    ImGui::PopItemWidth();

    // 同じ行に入力ボックス
    ImGui::SameLine();
    ImGui::PushItemWidth(inputWidth);
    if (ImGui::InputFloat(("##input_" + std::string(label)).c_str(), value, 0.0f, 0.0f, format)) {
        changed = true;
    }
    ImGui::PopItemWidth();

    // ラベル表示
    ImGui::SameLine();
    ImGui::Text("%s", label);

    // リセットボタン
    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::SmartSliderFloat3(const char* label, Vector3* value, float rangeMin, float rangeMax, const Vector3& defaultValue, const char* format) {
#ifdef USE_IMGUI
    bool changed = false;

    ImGui::Text("%s", label);

    // X成分
    ImGui::PushID("x");
    if (SmartSliderFloat("X", &value->x, rangeMin, rangeMax, defaultValue.x, format)) {
        changed = true;
    }
    ImGui::PopID();

    // Y成分
    ImGui::PushID("y");
    if (SmartSliderFloat("Y", &value->y, rangeMin, rangeMax, defaultValue.y, format)) {
        changed = true;
    }
    ImGui::PopID();

    // Z成分
    ImGui::PushID("z");
    if (SmartSliderFloat("Z", &value->z, rangeMin, rangeMax, defaultValue.z, format)) {
        changed = true;
    }
    ImGui::PopID();

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::SmartSliderInt(const char* label, int* value, int rangeMin, int rangeMax, int defaultValue) {
#ifdef USE_IMGUI
    bool changed = false;

    // 利用可能な幅を計算
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float sliderWidth = availableWidth * 0.55f;
    float inputWidth = availableWidth * 0.25f;

    // スライダー部分
    ImGui::PushItemWidth(sliderWidth);
    if (ImGui::SliderInt(("##slider_" + std::string(label)).c_str(), value, rangeMin, rangeMax)) {
        changed = true;
    }
    ImGui::PopItemWidth();

    // 同じ行に入力ボックス
    ImGui::SameLine();
    ImGui::PushItemWidth(inputWidth);
    if (ImGui::InputInt(("##input_" + std::string(label)).c_str(), value)) {
        changed = true;
    }
    ImGui::PopItemWidth();

    // ラベル表示
    ImGui::SameLine();
    ImGui::Text("%s", label);

    // リセットボタン
    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::FloatWithPresets(const char* label, float* value, const float* presets, const char** presetNames, int presetCount, float defaultValue) {
#ifdef USE_IMGUI
    bool changed = false;

    // メイン入力
    if (InputFloatWithReset(label, value, 0.1f, 1.0f, defaultValue)) {
        changed = true;
    }

    // プリセットボタンが複数ある場合は複数行に分割
    if (presetCount > 0) {
        ImGui::Text("プリセット:");

        const int maxButtonsPerRow = 4;
        for (int i = 0; i < presetCount; ++i) {
            ImGui::PushID(i);

            if (ImGui::Button(presetNames[i])) {
                *value = presets[i];
                changed = true;
            }

            // 改行判定
            if ((i + 1) % maxButtonsPerRow != 0 && i < presetCount - 1) {
                ImGui::SameLine();
            }

            ImGui::PopID();
        }
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::Vector3WithPresets(const char* label, Vector3* value, const Vector3* presets, const char** presetNames, int presetCount, const Vector3& defaultValue) {
#ifdef USE_IMGUI
    bool changed = false;

    // メイン入力
    if (InputFloat3WithReset(label, value, defaultValue)) {
        changed = true;
    }

    // プリセットボタン
    if (presetCount > 0) {
        ImGui::Text("プリセット:");

        const int maxButtonsPerRow = 4;
        for (int i = 0; i < presetCount; ++i) {
            ImGui::PushID(i);

            if (ImGui::Button(presetNames[i])) {
                *value = presets[i];
                changed = true;
            }

            // 改行判定
            if ((i + 1) % maxButtonsPerRow != 0 && i < presetCount - 1) {
                ImGui::SameLine();
            }

            ImGui::PopID();
        }
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::LogSliderFloat(const char* label, float* value, float logMin, float logMax, float defaultValue, const char* format) {
#ifdef USE_IMGUI
    // 対数スケールでのスライダー
    float logValue = ConvertToLog(*value, logMin, logMax);
    bool changed = ImGui::SliderFloat(label, &logValue, 0.0f, 1.0f, "");

    if (changed) {
        *value = ConvertFromLog(logValue, logMin, logMax);
    }

    // 実際の値を表示
    ImGui::SameLine();
    ImGui::Text(format, *value);

    // リセットボタン
    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::RangeSliderFloat(const char* label, float* minValue, float* maxValue, float rangeMin, float rangeMax, float defaultMin, float defaultMax) {
#ifdef USE_IMGUI
    bool changed = false;

    ImGui::Text("%s", label);

    // 最小値
    if (DragFloatWithReset("最小", minValue, 0.01f, rangeMin, *maxValue, defaultMin)) {
        changed = true;
    }

    // 最大値
    if (DragFloatWithReset("最大", maxValue, 0.01f, *minValue, rangeMax, defaultMax)) {
        changed = true;
    }

    // 範囲表示
    ImGui::Text("範囲: %.3f ～ %.3f", *minValue, *maxValue);

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::RangeInputFloat(const char* label, float* minValue, float* maxValue, float defaultMin, float defaultMax, const char* format) {
#ifdef USE_IMGUI
    bool changed = false;

    ImGui::Text("%s", label);

    // 最小値入力
    if (ImGui::InputFloat("最小値", minValue, 0.0f, 0.0f, format)) {
        // 最小値が最大値を超えないように制限
        if (*minValue > *maxValue) {
            *minValue = *maxValue;
        }
        changed = true;
    }

    // 最大値入力
    if (ImGui::InputFloat("最大値", maxValue, 0.0f, 0.0f, format)) {
        // 最大値が最小値を下回らないように制限
        if (*maxValue < *minValue) {
            *maxValue = *minValue;
        }
        changed = true;
    }

    // リセットボタン
    if (ImGui::Button(("リセット##" + std::string(label)).c_str())) {
        *minValue = defaultMin;
        *maxValue = defaultMax;
        changed = true;
    }

    // 範囲表示
    ImGui::Text("範囲: ");
    ImGui::SameLine();
    ImGui::Text(format, *minValue);
    ImGui::SameLine();
    ImGui::Text(" ～ ");
    ImGui::SameLine();
    ImGui::Text(format, *maxValue);

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::AngleDegrees(const char* label, float* value, float defaultValue) {
#ifdef USE_IMGUI

    // ラジアンから度数に変換
    float degrees = *value * 180.0f / 3.14159265359f;
    float defaultDegrees = defaultValue * 180.0f / 3.14159265359f;

    bool changed = DragFloatWithReset(label, &degrees, 1.0f, -360.0f, 360.0f, defaultDegrees, "%.1f°");

    if (changed) {
        // 度数からラジアンに変換
        *value = degrees * 3.14159265359f / 180.0f;
    }

    return changed;

#endif // _DEBUG
}

bool ImGuiControlsHelper::AngleRadians(const char* label, float* value, float defaultValue) {
#ifdef USE_IMGUI
    return DragFloatWithReset(label, value, 0.01f, -6.28318530718f, 6.28318530718f, defaultValue, "%.3f rad");
#endif // _DEBUG
}

bool ImGuiControlsHelper::PercentageSlider(const char* label, float* value, float defaultValue) {
#ifdef USE_IMGUI
    // 0.0-1.0を0%-100%に変換
    float percentage = *value * 100.0f;
    float defaultPercentage = defaultValue * 100.0f;

    bool changed = DragFloatWithReset(label, &percentage, 1.0f, 0.0f, 100.0f, defaultPercentage, "%.1f%%");

    if (changed) {
        *value = percentage / 100.0f;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::TimeInput(const char* label, float* value, float defaultValue) {
#ifdef USE_IMGUI
    bool changed = DragFloatWithReset(label, value, 0.1f, 0.0f, 3600.0f, defaultValue, "%.1f秒");

    // 時間の目安表示
    if (*value >= 60.0f) {
        ImGui::SameLine();
        if (*value >= 3600.0f) {
            ImGui::Text("(%.1f時間)", *value / 3600.0f);
        } else {
            ImGui::Text("(%.1f分)", *value / 60.0f);
        }
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::DirectionVector(const char* label, Vector3* direction, const Vector3& defaultDirection) {
#ifdef USE_IMGUI
    bool changed = false;

    ImGui::Text("%s", label);

    // 正規化された方向ベクトル入力
    if (DragFloat3WithReset("方向", direction, 0.01f, -1.0f, 1.0f, defaultDirection)) {
        // 正規化
        float length = std::sqrt(direction->x * direction->x + direction->y * direction->y + direction->z * direction->z);
        if (length > 0.001f) {
            direction->x /= length;
            direction->y /= length;
            direction->z /= length;
        } else {
            *direction = defaultDirection;
        }
        changed = true;
    }

    // プリセット方向ボタン
    ImGui::Text("プリセット:");

    if (ImGui::Button("上##dir")) {
        *direction = Vector3{ 0, 1, 0 };
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("下##dir")) {
        *direction = Vector3{ 0, -1, 0 };
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("前##dir")) {
        *direction = Vector3{ 0, 0, 1 };
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("後##dir")) {
        *direction = Vector3{ 0, 0, -1 };
        changed = true;
    }

    if (ImGui::Button("左##dir")) {
        *direction = Vector3{ -1, 0, 0 };
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("右##dir")) {
        *direction = Vector3{ 1, 0, 0 };
        changed = true;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::PositionVector(const char* label, Vector3* position, const Vector3& defaultPosition) {
    return DragFloat3WithReset(label, position, 0.1f, -1000.0f, 1000.0f, defaultPosition, "%.2f");
}

bool ImGuiControlsHelper::ScaleVector(const char* label, Vector3* scale, const Vector3& defaultScale) {
#ifdef USE_IMGUI
    bool changed = DragFloat3WithReset(label, scale, 0.01f, 0.001f, 100.0f, defaultScale, "%.3f");

    // 統一スケールボタン
    ImGui::SameLine();
    if (ImGui::Button(("統一##" + std::string(label)).c_str())) {
        float avgScale = (scale->x + scale->y + scale->z) / 3.0f;
        scale->x = scale->y = scale->z = avgScale;
        changed = true;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::ColorEdit4WithReset(const char* label, Vector4* color, const Vector4& defaultValue) {
#ifdef USE_IMGUI
    bool changed = ImGui::ColorEdit4(label, &color->x);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *color = defaultValue;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::ColorPicker4WithReset(const char* label, Vector4* color, const Vector4& defaultValue) {
#ifdef USE_IMGUI
    bool changed = ImGui::ColorPicker4(label, &color->x);

    if (ImGui::Button(("リセット##" + std::string(label)).c_str())) {
        *color = defaultValue;
        changed = true;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::ColorPresets(const char* label, Vector4* color, const Vector4& defaultValue) {
#ifdef USE_IMGUI
    bool changed = false;

    // メインカラーピッカー
    if (ImGui::ColorEdit4(label, &color->x)) {
        changed = true;
    }

    // カラープリセット
    ImGui::Text("プリセット:");

    Vector4 presetColors[] = {
        {1.0f, 1.0f, 1.0f, 1.0f}, // 白
        {1.0f, 0.0f, 0.0f, 1.0f}, // 赤
        {0.0f, 1.0f, 0.0f, 1.0f}, // 緑
        {0.0f, 0.0f, 1.0f, 1.0f}, // 青
        {1.0f, 1.0f, 0.0f, 1.0f}, // 黄
        {1.0f, 0.0f, 1.0f, 1.0f}, // マゼンタ
        {0.0f, 1.0f, 1.0f, 1.0f}, // シアン
        {0.0f, 0.0f, 0.0f, 1.0f}, // 黒
    };

    const char* presetNames[] = {
        "白", "赤", "緑", "青", "黄", "紫", "水", "黒"
    };

    for (int i = 0; i < 8; ++i) {
        ImGui::PushID(i);

        // カラーボタン
        if (ImGui::ColorButton(presetNames[i], ImVec4(presetColors[i].x, presetColors[i].y, presetColors[i].z, presetColors[i].w), 0, ImVec2(25, 25))) {
            *color = presetColors[i];
            changed = true;
        }

        // ツールチップ
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", presetNames[i]);
        }

        // 4個ずつ改行
        if ((i + 1) % 4 != 0) {
            ImGui::SameLine();
        }

        ImGui::PopID();
    }

    // リセットボタン
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *color = defaultValue;
    }

    return changed;
#endif // _DEBUG
}

bool ImGuiControlsHelper::CheckboxWithReset(const char* label, bool* value, bool defaultValue) {
#ifdef USE_IMGUI
    bool changed = ImGui::Checkbox(label, value);

    ImGui::SameLine();
    if (ShowResetButton(GetUniqueID(label).c_str(), &changed)) {
        *value = defaultValue;
    }

    return changed;
#endif // _DEBUG
}

void ImGuiControlsHelper::ShowTooltip(const char* text) {
#ifdef USE_IMGUI
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
#endif // _DEBUG
}

bool ImGuiControlsHelper::ShowResetButton(const char* id, bool* changed) {
#ifdef USE_IMGUI
    std::string buttonLabel = "リセット" + std::string(id);
    if (ImGui::Button(buttonLabel.c_str())) {
        *changed = true;
        return true;
    }
    return false;
#endif // _DEBUG
}

std::string ImGuiControlsHelper::GetUniqueID(const char* baseLabel) {
    return "##" + std::string(baseLabel);
}

float ImGuiControlsHelper::ConvertToLog(float value, float logMin, float logMax) {
    if (value <= logMin) return 0.0f;
    if (value >= logMax) return 1.0f;
    return std::log(value / logMin) / std::log(logMax / logMin);
}

float ImGuiControlsHelper::ConvertFromLog(float logValue, float logMin, float logMax) {
    return logMin * std::pow(logMax / logMin, std::max(0.0f, std::min(1.0f, logValue)));
}
#endif // _DEBUG