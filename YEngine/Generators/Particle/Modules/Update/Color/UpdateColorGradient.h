#pragma once
#include "UpdateColorOverLifetime.h"

/// <summary>
/// 旧「グラデーション」モジュール（等間隔・最大 5 色）。
/// 中身は UpdateColorOverLifetime と同じになったため派生クラスとして残し、
/// 旧 JSON（"colorCount" / "colors"）の読み込みだけを担う。
/// エディタの追加メニューには出さない（「寿命でカラー変化」を使う）。
/// </summary>
class UpdateColorGradient : public UpdateColorOverLifetime {
public:
  UpdateColorGradient() = default;
  std::string GetName() const override { return "グラデーション（旧）"; }
  std::string GetTypeName() const override { return "UpdateColorGradient"; }
  void LoadFromJson(const nlohmann::json &json) override;
};

REGISTER_UPDATE_MODULE(UpdateColorGradient)
