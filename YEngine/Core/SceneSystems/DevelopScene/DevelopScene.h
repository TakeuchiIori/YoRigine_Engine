#pragma once
// Engine
#include <SceneSystems/BaseScene.h>

#include "Systems/Camera/SceneCameraRig.h"

#include "Motion/Editor/MotionEditor.h"


class DevelopScene : public BaseScene
{
public:
	///************************* 基本関数 *************************///
	DevelopScene() : BaseScene("Develop") {}
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void Finalize() override;

	// PiP 用: シーンカメラ公開 + 3D オンリー描画パス
	YoRigine::Camera* GetSceneCamera() override { return cameraRig_.GetCamera(); }
	void DrawScene3DOnly() override;

private:
	///************************* 内部処理 *************************///

	void DrawObject();
	void DrawLine();

private:
	///************************* メンバ変数 *************************///

	// 出力カメラ一式（Develop は通常カメラ自体がデバッグカメラ）
	SceneCameraRig cameraRig_;

	std::unique_ptr<MotionEditor> motionEditor_ = nullptr;
	std::unique_ptr<YoRigine::Line> line_;
};

