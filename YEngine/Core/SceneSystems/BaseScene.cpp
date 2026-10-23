#include "BaseScene.h"
#include <Object3D/BaseObjectManager.h>
#include <Object3D/Object3dCommon.h>
#include <SceneEditor/SceneEditor.h>

void BaseScene::DrawCommonShadow() {
  Object3dCommon::GetInstance()->ShadowDrawPreference();

  //------------------------------------------------------------
  // 登録オブジェクトの影描画もここで一括処理する。
  // 全シーンが本関数を呼んでいるため、シーン側に影描画コードは不要になる。
  // ボーン無しオブジェクトはインスタンシングでまとめて描画する
  // （色パスの DrawAllInstanced と対称。個別 DrawShadowAll は
  //  敵1体ごとに非インスタンスDrawを積むため、敵が多いと影パスだけ重くなる）。
  //------------------------------------------------------------
  BaseObjectManager::GetInstance()->DrawShadowAllInstanced();
  YoRigine::SceneEditor::GetInstance()->DrawShadow();
}

// ============================================================
// 登録オブジェクトの一括更新
// ============================================================
void BaseScene::UpdateObjects() {
  BaseObjectManager::GetInstance()->UpdateAll();
}

// ============================================================
// 登録オブジェクトの一括描画
// ============================================================
void BaseScene::DrawObjects() {
  BaseObjectManager::GetInstance()->DrawAll();
  BaseObjectManager::GetInstance()->DrawAnimationAll();
}
