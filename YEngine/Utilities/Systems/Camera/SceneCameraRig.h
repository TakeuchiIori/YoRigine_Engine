#pragma once

// C++
#include <memory>
#include <string>

// Engine
#include "Systems/Camera/Camera.h"
#include "Systems/Camera/CameraEditor.h"

/// <summary>
/// シーンの出力カメラ一式
/// 出力用 Camera の所有、CameraDirector の更新、通常カメラ / デバッグカメラの
/// 切り替え、Director 結果の出力カメラへの同期をまとめて受け持つ。
/// 各シーンが同じカメラ処理を個別に書かないようにするためのクラス。
/// </summary>
class SceneCameraRig {
public:
  // デバッグ用 VirtualCamera の名前（全シーン共通）
  static constexpr const char *kDebugCameraId = "MainDebug";

  /// <summary>
  /// 初期化パラメータ
  /// </summary>
  struct Desc {
    // CameraEditor の保存先 JSON
    std::string editorFilePath;
    // CameraEditor が読み込むプリセット名
    std::string editorPresetName;
    // 通常時に優先する VirtualCamera の名前
    std::string mainCameraId;
    // デバッグカメラ中にゲーム時間を止めるか
    bool pauseGameTimeInDebug = false;
    // 通常カメラ中は毎フレームスナップしてブレンドを無効にするか
    // (Title / Clear の旋回カメラの見た目を従来どおりに保つための互換設定)
    bool snapMainEveryFrame = false;
  };

public:
  ///************************* 基本関数 *************************///

  // 出力カメラの生成と CameraDirector / CameraEditor の準備
  void Initialize(const Desc &desc);

  // Director を更新して出力カメラへ反映する
  // forceRealtime: デバッグ以外でも停止を無視し、実時間で進めたいとき
  // true（死亡演出など）
  void Update(bool forceRealtime = false);

  // シーン終了時の後始末（自分が止めたゲーム時間を戻す）
  void Finalize();

  // 現在のモードの優先度を設定し、ブレンド無しで即座に切り替える
  void SnapToActive();

  ///************************* デバッグカメラ *************************///

  // デバッグカメラの ON/OFF（F3 キーと ImGui の両方からここを通す）
  void SetDebugActive(bool active);
  bool IsDebugActive() const { return debugActive_; }

#ifdef USE_IMGUI
  // エディタパネル（カメラエディター / カメラモード切り替え）を登録する
  void RegisterEditorPanels(const std::string &sceneName,
                            const std::string &category = "");
#endif

  ///************************* アクセッサ *************************///

  YoRigine::Camera *GetCamera() const { return camera_.get(); }

private:
  ///************************* 内部処理 *************************///

  // 通常 / デバッグに応じて VirtualCamera の優先度を設定する
  void ApplyPriorities();

  // Director が計算した姿勢・レンズを出力カメラへ写す
  void SyncFromDirector();

  // デバッグカメラ中のゲーム時間停止を、状態に合わせて掛ける / 外す
  void UpdateTimePause();

#ifdef USE_IMGUI
  // カメラモード切り替えボタン
  void DrawModeSwitcher();
#endif

private:
  ///************************* メンバ変数 *************************///

  Desc desc_;

  // 出力用カメラ（実体）。描画や当たり判定のカリングはこのカメラを参照する
  std::unique_ptr<YoRigine::Camera> camera_;
  std::unique_ptr<CameraEditor> editor_;

  bool debugActive_ = false;
  // 自分がゲーム時間を止めたか。止めた側だけが再開させる
  bool pausedByDebug_ = false;
};
