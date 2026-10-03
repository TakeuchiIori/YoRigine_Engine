#pragma once

#include "Loaders/Json/Use/AutoJson.h"

#include <string>

namespace YoRigine {

class CollisionManager;

// 当たり判定システム全体の設定を編集・永続化するエディター。
// 個々のコライダー形状や配置はシーンエディター側に残し、
// BroadPhase／接触解決など全コライダー共通の設定だけを扱う。

// 操作対象の CollisionManager はコンストラクタで参照を受け取る。
// → 「このエディタは誰を編集するのか」が型から読める。
class CollisionEditor {
public:
  explicit CollisionEditor(CollisionManager &target);

  void Initialize();
  void DrawImGui();

private:
  void ApplySettings();
  void SaveSettings();
  void LoadSettings();
  void ResetDefaults();

  static constexpr const char *kSettingsPath =
      "Resources/Json/Collision/CollisionSettings.json";

  // 設定の適用先。所有者 (CollisionManager) が必ず本体より長生きする
  CollisionManager &target_;

  AutoJson autoJson_;

  float broadPhaseCellSize_ = 2.5f;
  bool enableFrustumCulling_ = false;
  int resolveIterations_ = 3;
  int contactExitGraceFrames_ = 2;

  bool initialized_ = false;
  bool dirty_ = false;
  std::string status_;
};

} // namespace YoRigine
