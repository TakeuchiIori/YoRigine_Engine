#include "UpdateColorOverLifetime.h"

#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget_Gradient.h"
#include "Core/Editor/Widgets/YEditorWidget_Tooltip.h"
#endif

UpdateColorOverLifetime::UpdateColorOverLifetime() {
  // デフォルト: 炎グラデーション
  gradient_.space = ColorGradient::Space::Oklab;
  gradient_.stops = {
      {0.0f, Vector4{1.0f, 1.0f, 0.8f, 1.0f}}, // 白黄
      {0.3f, Vector4{1.0f, 0.5f, 0.0f, 1.0f}}, // オレンジ
      {0.7f, Vector4{0.8f, 0.1f, 0.0f, 0.5f}}, // 赤（半透明）
      {1.0f, Vector4{0.2f, 0.0f, 0.0f, 0.0f}}, // 暗赤（透明）
  };
  gradient_.MarkDirty();
}

void UpdateColorOverLifetime::OnUpdate(ParticleAttribute *attrs, uint32_t index,
                                       float) {
  const Vector4 c = gradient_.Sample(attrs[index].GetNormalizedAge());
  if (!multiplyInitialColor_) {
    attrs[index].color = c;
    return;
  }
  // 生成時の色（Spawn モジュール適用後に確定した initialColor）に掛け合わせる
  const Vector4 &base = attrs[index].initialColor;
  attrs[index].color = {base.x * c.x, base.y * c.y, base.z * c.z, base.w * c.w};
}

void UpdateColorOverLifetime::DrawEditor() {
#ifdef USE_IMGUI
  YEditorWidget::Gradient("寿命でのカラー & アルファ", gradient_);
  ImGui::Checkbox("生成時の色を掛ける", &multiplyInitialColor_);
  YEditorWidget::ItemTooltip(
      "OFF: グラデーションの色で上書きする（生成時の色は消える）\n"
      "ON : 生成時の色 × "
      "グラデーション（カラー範囲やランダムカラーのばらつきを活かせる）\n"
      "     ON "
      "にするときはグラデーションを白基調（明るさとアルファの変化）にすると扱い"
      "やすい");
#endif
}

void UpdateColorOverLifetime::SaveToJson(nlohmann::json &json) const {
  gradient_.SaveToJson(json);
  // 既定（OFF）は書かない＝既存ファイルの差分を増やさない
  if (multiplyInitialColor_)
    json["multiplyInitialColor"] = true;
}

void UpdateColorOverLifetime::LoadFromJson(const nlohmann::json &json) {
  // 旧フォーマット {"stops":[{"t","color"}]} もそのまま読める（space 無し →
  // RGB補間）
  gradient_.LoadFromJson(json);
  multiplyInitialColor_ = json.value("multiplyInitialColor", false);
}
