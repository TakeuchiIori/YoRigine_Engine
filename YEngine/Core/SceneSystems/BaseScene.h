#pragma once

// C++
#include <memory>
#include <string>

// Engine
#include <Systems/Camera/Camera.h>

// 前方宣言
class SceneManager;

/// <summary>
/// メインシーンの基底クラス
/// エンジン共通システム（当たり判定・パーティクル・VFX・影行列など）の更新は
/// SceneManager が Update() の後にまとめて行うので、派生シーンでは書かない。
/// </summary>
class BaseScene {
public:
  ///************************* 基本的な関数 *************************///
  virtual ~BaseScene() = default;

  // 初期化
  virtual void Initialize() = 0;
  // 更新（シーン固有の処理だけを書く）
  virtual void Update() = 0;
  // 描画（ポストエフェクト対象）
  virtual void Draw() = 0;

  // 以下は不要なシーンが空実装を書かなくて済むよう、既定の実装を持つ
  // 終了
  virtual void Finalize() {}
  // PostEffectを掛けたくないものを描画
  virtual void DrawNonOffscreen() {}
  // 影描画（既定は登録オブジェクトと配置オブジェクトの影を一括描画）
  virtual void DrawShadow() { DrawCommonShadow(); }

  // シーンの出力カメラ。共通システムの更新・PiP・ポストエフェクトが参照する
  virtual YoRigine::Camera *GetSceneCamera() = 0;
  // ビュープロジェクション行列を取得
  Matrix4x4 GetViewProjection() {
    return GetSceneCamera()->viewProjectionMatrix_;
  }

  // PiP 用の 3D
  // オンリー描画。スカイボックスとシーンオブジェクトを描く程度に絞る。 UI /
  // ポストエフェクト / シャドウパスは含めない。
  virtual void DrawScene3DOnly() {}

public:
  ///************************* アクセッサ *************************///
  virtual void SetSceneManager(SceneManager *sceneManager) {
    sceneManager_ = sceneManager;
  }
  const std::string &GetName() const { return sceneName_; }

protected:
  ///************************* 継承関で使用 *************************///
  SceneManager *sceneManager_ = nullptr;
  BaseScene(const std::string &name) : sceneName_(name) {}

  ///************************* 共通処理 *************************///
  // シャドウパス開始前に必ず呼ぶ（PSO・RS・topology・gLight 一括設定 ＋
  // 登録オブジェクトの影描画）
  void DrawCommonShadow();

  ///************************* BaseObject 一括駆動 *************************///
  // BaseObjectManager に登録済みのオブジェクトをまとめて駆動するヘルパー。
  // 衝突判定の前 /
  // スカイボックスとの描画順を保つため、呼ぶ位置はシーン側で決める。
  void UpdateObjects(); // 登録オブジェクトの一括 Update
  void DrawObjects();   // 登録オブジェクトの一括 Draw + DrawAnimation

private:
  ///************************* メンバ変数 *************************///
  std::string sceneName_;
};
