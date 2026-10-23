#include "SceneCameraRig.h"

// Engine
#include "Systems./Input./Input.h"
#include "Systems/Camera/CameraDirector.h"
#include "Systems/GameTime/GameTime.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include <Editor/Editor.h>
#endif

namespace {
// 選ばれている VirtualCamera と、選ばれていない側の優先度
constexpr int kActivePriority = 10;
constexpr int kInactivePriority = 0;
} // namespace

/// <summary>
/// 初期化処理
/// </summary>
void SceneCameraRig::Initialize(const Desc &desc) {
  desc_ = desc;
  debugActive_ = false;
  pausedByDebug_ = false;

  camera_ = std::make_unique<YoRigine::Camera>();

  // VirtualCamera 群はエディタの JSON から生成されるので、Director
  // の初期化を先に行う
  CameraDirector::GetInstance()->Initialize();

  editor_ = std::make_unique<CameraEditor>();
  editor_->Initialize();
  editor_->SetFilePath(desc_.editorFilePath);
  editor_->LoadFileOrDefault(editor_->GetFilePath(), desc_.editorPresetName);
}

/// <summary>
/// 更新処理
/// </summary>
void SceneCameraRig::Update(bool forceRealtime) {
#ifdef _DEBUG
  // デバッグカメラ切り替え。停止中でも戻せるよう、停止判定より前で受け付ける
  if (YoRigine::Input::GetInstance()->TriggerKey(DIK_F3)) {
    SetDebugActive(!debugActive_);
  }
#endif

  // デバッグカメラ中と死亡演出などは、ゲーム時間の停止やスローに影響されない
  const bool realtime = debugActive_ || forceRealtime;
  if (YoRigine::GameTime::IsPause() && !realtime) {
    return;
  }

  ApplyPriorities();

  auto *director = CameraDirector::GetInstance();
  if (desc_.snapMainEveryFrame && !debugActive_) {
    director->SnapToActiveCamera();
  }

  const float dt = realtime ? YoRigine::GameTime::GetUnscaledDeltaTime()
                            : YoRigine::GameTime::GetDeltaTime();
  director->Update(dt);

  SyncFromDirector();
}

/// <summary>
/// 終了処理
/// </summary>
void SceneCameraRig::Finalize() {
  // 次のシーンにゲーム時間の停止を持ち越さない
  if (pausedByDebug_) {
    YoRigine::GameTime::Resume();
    pausedByDebug_ = false;
  }
  debugActive_ = false;
}

/// <summary>
/// 優先度を確定させてブレンド無しで切り替える
/// </summary>
void SceneCameraRig::SnapToActive() {
  ApplyPriorities();
  CameraDirector::GetInstance()->SnapToActiveCamera();
}

/// <summary>
/// デバッグカメラの ON/OFF
/// </summary>
void SceneCameraRig::SetDebugActive(bool active) {
  debugActive_ = active;
  // 切り替え直後に遠い姿勢同士を補間して回り込まないよう、ブレンドを切る
  CameraDirector::GetInstance()->SetEnableBlending(false);
  UpdateTimePause();
}

/// <summary>
/// VirtualCamera の優先度設定
/// </summary>
void SceneCameraRig::ApplyPriorities() {
  auto *director = CameraDirector::GetInstance();

  const std::string active = debugActive_ ? kDebugCameraId : desc_.mainCameraId;
  const std::string inactive =
      debugActive_ ? desc_.mainCameraId : kDebugCameraId;

  // 通常カメラ自体がデバッグカメラのシーン(Develop)では同名になるため、
  // 選ばれている側を最後に設定して上書きされないようにする
  if (inactive != active) {
    director->SetPriority(inactive, kInactivePriority);
  }
  director->SetPriority(active, kActivePriority);
}

/// <summary>
/// Director の結果を出力カメラへ同期
/// </summary>
void SceneCameraRig::SyncFromDirector() {
  auto *director = CameraDirector::GetInstance();

  // Director が導き出した「理想の座標・回転・レンズ情報」をコピー
  camera_->SetTranslate(director->GetActiveCameraPos());
  camera_->SetRotate(director->GetActiveCameraRot());
  camera_->SetFovY(director->GetFovY());
  camera_->viewMatrix_ = director->GetViewMatrix();
  // カメラ自体の更新（内部でのシェイク計算など）
  camera_->Update();
  // 最終的な行列の計算
  camera_->UpdateMatrix();
}

/// <summary>
/// デバッグカメラ中のゲーム時間停止
/// </summary>
void SceneCameraRig::UpdateTimePause() {
  if (!desc_.pauseGameTimeInDebug) {
    return;
  }

  if (debugActive_) {
    // 既に他の理由で止まっている場合は、自分の停止として記録しない
    if (!YoRigine::GameTime::IsPause()) {
      YoRigine::GameTime::Pause();
      pausedByDebug_ = true;
    }
    return;
  }

  if (pausedByDebug_) {
    YoRigine::GameTime::Resume();
    pausedByDebug_ = false;
  }
}

#ifdef USE_IMGUI
/// <summary>
/// エディタパネル登録
/// </summary>
void SceneCameraRig::RegisterEditorPanels(const std::string &sceneName,
                                          const std::string &category) {
  Editor::GetInstance()->RegisterGameUI(
      "カメラエディター", [this]() { editor_->Update(); }, sceneName, category);
  Editor::GetInstance()->RegisterGameUI(
      "カメラモード切り替え", [this]() { DrawModeSwitcher(); }, sceneName,
      category);
}

/// <summary>
/// カメラモード切り替えボタン
/// </summary>
void SceneCameraRig::DrawModeSwitcher() {
  if (ImGui::Button("通常カメラ")) {
    SetDebugActive(false);
  }
  ImGui::SameLine();
  if (ImGui::Button("デバッグカメラ (F3)")) {
    SetDebugActive(true);
  }
  ImGui::Text("現在: %s",
              debugActive_ ? kDebugCameraId : desc_.mainCameraId.c_str());
}
#endif
