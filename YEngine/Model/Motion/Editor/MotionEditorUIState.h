#pragma once

// ============================================================
// モーションエディタの「見た目だけ」の状態
//
// ・ポップアップの開閉、レイアウト寸法、表示切り替えなど
// ・モーションデータや再生状態は持たない（それは MotionEditorSession の担当）
// ============================================================
struct MotionEditorUIState {
  // ダイアログ
  bool showFileDialog = false;   // .anim の保存 / 読み込み
  bool showSourceDialog = false; // glTF からの読み込み

  // レイアウト
  float timelineHeight = 240.0f;
  float boneListWidth = 250.0f;

  // 3D ビュー上のボーン表示
  bool drawBones = false;

  // トリム範囲の入力
  bool useTrim = false;
  float trimStart = 0.0f;
  float trimEnd = 0.0f;
};
