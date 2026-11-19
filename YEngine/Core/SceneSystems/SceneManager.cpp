#include "SceneManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Composite/CompositeEffectManager.h"
#include "GPUParticle/YGpuEmitManager.h"
#include "LightManager/LightManager.h"
#include "OffScreen/PostEffectManager.h"
#include "Particle/YParticleManager.h"
#include "SceneEditor/SceneEditor.h"
#include "Sprite/SpriteCommon.h"
#include "Systems/Camera/CameraDirector.h"
#include "Systems/Cinematic/CinematicManager.h"
#include "Systems/GameTime/GameTime.h"
#include <Object3D/BaseObjectManager.h>
#include <Vfx/VfxMesh/Runtime/VfxMeshSpawner.h>
#include <assert.h>

std::unique_ptr<SceneManager> SceneManager::instance = nullptr;
std::once_flag SceneManager::initInstanceFlag;

/// <summary>
/// シングルトンインスタンス取得
/// </summary>
SceneManager *SceneManager::GetInstance() {
  std::call_once(initInstanceFlag,
                 []() { instance = std::make_unique<SceneManager>(); });
  return instance.get();
}

/// <summary>
/// 終了処理
/// </summary>
void SceneManager::Finalize() {
  // 現在のシーンが存在する場合、終了して解放
  if (scene_) {
    scene_->Finalize();
    scene_ = nullptr;
  }

  // 借用ポインタを手放す (ダングリング防止)
  postEffectManager_ = nullptr;
  baseObjectManager_ = nullptr;
}

/// <summary>
/// 初期化処理
/// </summary>
void SceneManager::Initialize() {
  // トランジションが設定されている場合のみ初期化
  if (transitionFactory_) {
    transition_ = transitionFactory_->CreateTransition();
    transition_->Initialize();
  }
}

/// <summary>
/// 更新処理
/// </summary>
void SceneManager::Update() {
  //------------------------------------------------------------
  // 現在のシーン更新
  //------------------------------------------------------------
  if (scene_) {
    YoRigine::GameTime::Update();
    scene_->Update();
    // シーンが動かした結果をもとに、エンジン共通システムを更新する
    UpdateEngineSystems(scene_->GetSceneCamera());
  }

  //------------------------------------------------------------
  // トランジション更新と状態管理
  //------------------------------------------------------------
  if (transition_) {
    transition_->Update();

    switch (transitionState_) {
    case TransitionState::FadeOut:
      // フェードアウト完了時にシーン切り替え
      if (transition_->IsFinished()) {

        PerformSceneTransition();

        // フェードイン開始
        transition_->StartReveal();
        transitionState_ = TransitionState::FadeIn;
      }
      break;

    case TransitionState::FadeIn:
      // フェードイン完了後、通常状態に戻す
      if (transition_->IsFinished()) {
        transitionState_ = TransitionState::None;
      }
      break;

    case TransitionState::None:
      // 次のシーン予約があればフェードアウト開始
      if (nextScene_) {
        transition_->StartCover();
        transitionState_ = TransitionState::FadeOut;
      }
      break;
    }
  }
}

/// <summary>
/// エンジン共通システムの更新
/// 全シーンで同じ処理なので、シーン側に書かせず必ずここを通す。
/// </summary>
void SceneManager::UpdateEngineSystems(YoRigine::Camera *camera) {
  const float vfxDt =
      YoRigine::GameTime::GetDeltaTime(YoRigine::TimeChannel::Vfx);

  YoRigine::SceneEditor::GetInstance()->Update();
  YoRigine::CollisionManager::GetInstance()->Update();
  YParticleManager::GetInstance().Update(vfxDt);

  VfxMeshSpawner::GetInstance()->SetCamera(camera);
  VfxMeshSpawner::GetInstance()->Update(vfxDt);
  // GPU パーティクルも VfxMesh と同じく毎フレーム現在シーンのカメラを渡す。
  // 各シーンに SetCamera を書かせると、書き忘れたシーン（Release
  // 起動のタイトル）で null
  // 参照、または前シーンの破棄済みカメラ参照になって落ちるため。
  YoRigine::YGpuEmitManager::GetInstance()->SetCamera(camera);
  YoRigine::YGpuEmitManager::GetInstance()->Update();
  CompositeEffectManager::GetInstance()->Update(vfxDt);

  // 影行列はカメラ視錐台からカスケードを切るため、カメラ確定後に更新する
  if (camera) {
    YoRigine::LightManager::GetInstance()->UpdateShadowMatrix(camera);
  }
}

/// <summary>
/// シーン寿命の状態の一掃
/// シングルトンはシーンより長生きするので、シーンが渡したカメラ・コールバック・
/// 再生中の演出を抱えたままだと、次のシーンで破棄済みメモリを触って落ちる。
/// 「タイトル → ゲーム → クリア →
/// タイトル…」を何周しても同じ状態から始まるよう、
/// 旧シーン破棄の直後・新シーン初期化の直前にここでまとめて切り離す。
/// </summary>
void SceneManager::ClearSceneScopedState() {
  // シーンをまたいでパーティクル・VFX が残存しないよう一括停止する
  YParticleManager::GetInstance().StopAndClearActiveEmitters();
  YoRigine::YGpuEmitManager::GetInstance()->StopAllEmitterGroups();
  VfxMeshSpawner::GetInstance()->StopAll();

  // 旧シーンのカメラ（シーン破棄と同時に解放済み）を指したままにしない。
  // 新しいカメラは次フレームの UpdateEngineSystems / 新シーンの Initialize
  // で渡される。
  YoRigine::YGpuEmitManager::GetInstance()->SetCamera(nullptr);
  VfxMeshSpawner::GetInstance()->SetCamera(nullptr);
  YoRigine::CollisionManager::GetInstance()->SetCullingCamera(nullptr);

  // 演出シーケンスは this をキャプチャしたコールバックを持つ。
  // MyGame が毎フレーム CinematicManager を更新するため、ここで止めないと
  // 次フレームで破棄済みシーンのメンバへアクセスする。
  YoRigine::CinematicManager::GetInstance()->Stop();

  // 遅延ヒット判定のコールバックも呼び出し元オブジェクトの寿命に紐づく
  CompositeEffectManager::GetInstance()->ClearPendingHitQueries();

  // カメラが追従するターゲット(WorldTransform)は旧シーンのオブジェクトを指している
  CameraDirector::GetInstance()->ClearTargets();
}

void SceneManager::PerformSceneTransition() {

  //------------------------------------------------------------
  // 現在のシーンを安全に終了
  //------------------------------------------------------------
  if (scene_) {
    scene_->Finalize();
    scene_ = nullptr;
  }

  // 旧シーンが残した参照・演出を一掃する
  ClearSceneScopedState();

  //------------------------------------------------------------
  // 新しいシーンに切り替え
  //------------------------------------------------------------

  // ポストエフェクト初期化
  assert(postEffectManager_ &&
         "SceneManager : SetPostEffectManager() を先に呼ぶこと");
  postEffectManager_->Reset();
  scene_ = std::move(nextScene_);
  nextScene_ = nullptr;
  if (scene_) {
    // 前シーンの BaseObject 登録を一掃してから次シーンを初期化する。
    // これで各シーン側に「登録解除」コードを書く必要がなくなる。
    assert(baseObjectManager_ &&
           "SceneManager : SetBaseObjectManager() を先に呼ぶこと");
    baseObjectManager_->ClearAll();
    scene_->SetSceneManager(this);
    scene_->Initialize();
  }
}

/// <summary>
/// シーン描画（オフスクリーン対象）
/// </summary>
void SceneManager::Draw() {
  if (scene_) {
    scene_->Draw();
  }
}

/// <summary>
/// シーン描画（オフスクリーン外 / トランジション描画含む）
/// </summary>
void SceneManager::DrawNonOffscreen() {
  //------------------------------------------------------------
  // 通常シーン描画
  //------------------------------------------------------------
  if (scene_) {
    scene_->DrawNonOffscreen();
  }

  //------------------------------------------------------------
  // トランジション描画
  //------------------------------------------------------------
  if (transition_ && transitionState_ != TransitionState::None) {
    transition_->Draw();
  }
}

void SceneManager::DrawShadow() {
  if (scene_) {
    scene_->DrawShadow();
  }
}

/// <summary>
/// シーン変更要求
/// </summary>
void SceneManager::ChangeScene(const std::string &sceneName) {
  assert(sceneFactory_);
  // 既に遷移中、または次シーン予約済みなら無視
  if (transitionState_ != TransitionState::None || nextScene_) {
    return;
  }

  nextScene_ = sceneFactory_->CreateScene(sceneName);
  //------------------------------------------------------------
  // 初回シーン生成処理
  //------------------------------------------------------------
  if (!scene_) {
    scene_ = std::move(nextScene_);
    nextScene_ = nullptr;
    // 初回シーンも空の状態から始められるように一掃しておく。
    assert(baseObjectManager_ &&
           "SceneManager : SetBaseObjectManager() を先に呼ぶこと");
    baseObjectManager_->ClearAll();
    scene_->SetSceneManager(this);
    scene_->Initialize();

    // 初回のみフェードイン開始
    if (transition_) {
      transition_->StartReveal();
      transitionState_ = TransitionState::FadeIn;
    }
  }
}

/// <summary>
///	瞬時にシーン遷移（フェードなし）
/// </summary>
/// <param name="sceneName"></param>
void SceneManager::ChangeSceneImmediate(const std::string &sceneName) {
  assert(sceneFactory_);

  // ポストエフェクトのリセット
  assert(postEffectManager_ &&
         "SceneManager : SetPostEffectManager() を先に呼ぶこと");
  postEffectManager_->Reset();
  // 現在のシーンを終了
  if (scene_) {
    scene_->Finalize();
    scene_ = nullptr;
  }

  // 旧シーンが残した参照・演出を一掃する
  ClearSceneScopedState();

  // 前シーンの BaseObject 登録を一掃する（フェード遷移の PerformSceneTransition
  // と同様）。 これを忘れると、解放済みオブジェクトへのダングリング参照が
  // entries_ に残り、 次シーンの DrawShadowAll / UpdateAll 等で落ちる（Develop
  // への即時遷移で発生）。
  assert(baseObjectManager_ &&
         "SceneManager : SetBaseObjectManager() を先に呼ぶこと");
  baseObjectManager_->ClearAll();

  // 新しいシーンを生成して初期化
  scene_ = sceneFactory_->CreateScene(sceneName);
  scene_->SetSceneManager(this);
  scene_->Initialize();

  // トランジションの状態をリセット
  transitionState_ = TransitionState::None;
}
