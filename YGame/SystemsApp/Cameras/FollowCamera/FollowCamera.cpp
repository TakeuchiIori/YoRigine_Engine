#include "FollowCamera.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include <Systems/Input/Input.h>
#include <DirectXMath.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // _DEBUG

/// <summary>
/// 初期化処理
/// </summary>
void FollowCamera::Initialize()
{
	InitJson();
}

/// <summary>
/// 更新処理
/// </summary>
void FollowCamera::Update()
{
	UpdateInput();
	FollowProsess();
}

/// <summary>
/// 入力処理（コントローラーによる回転操作）
/// </summary>
void FollowCamera::UpdateInput()
{
	// クローズアップ中は操作無効
	if (isCloseUp_) return;

	if (YoRigine::Input::GetInstance()->IsControllerConnected())
	{
		XINPUT_STATE joyState;
		if (YoRigine::Input::GetInstance()->GetJoystickState(0, joyState)) {
			Vector3 move{};
			move.x = 0.0f;
			move.y += static_cast<float>(joyState.Gamepad.sThumbRX);
			move.z = 0.0f;

			// 入力強度に応じた回転速度反映
			if (Length(move) > 0.0f) {
				move = Normalize(move) * kRotateSpeed_;
			} else {
				move = { 0.0f, 0.0f, 0.0f };
			}

			rotate_ += move;
		}
	}
}

/// <summary>
/// 追従処理
/// </summary>
void FollowCamera::FollowProsess()
{
	if (target_ == nullptr) {
		return;
	}

	//------------------------------------------------------------
	// 近接倍率補間処理
	//------------------------------------------------------------
	float targetScale = isCloseUp_ ? closeUpScale_ : 1.0f;

	// 固定ステップ補間（1フレーム約16ms基準）
	currentScale_ += (targetScale - currentScale_) * std::clamp(interpSpeed_ * 0.016f, 0.0f, 1.0f);

	//------------------------------------------------------------
	// 追従座標計算
	//------------------------------------------------------------
	Vector3 offset = offset_ * currentScale_;

	Matrix4x4 rotate = MakeRotateMatrixXYZ(rotate_);
	offset = TransformNormal(offset, rotate);

	translate_ = target_->translate_ + offset;

	//------------------------------------------------------------
	// ビュー行列更新
	//------------------------------------------------------------
	matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
}

/// <summary>
/// JSON初期化処理（パラメータ登録）
/// </summary>
void FollowCamera::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("FollowCamera", "Resources/Json/Cameras");
	jsonManager_->SetCategory("Cameras");
	jsonManager_->SetSubCategory("FollowCamera");

	jsonManager_->Register("オフセットの位置", &offset_);
	jsonManager_->Register("回転", &rotate_);
	jsonManager_->Register("回転速度", &kRotateSpeed_);
	jsonManager_->Register("近づく倍率", &closeUpScale_);
	jsonManager_->Register("補間速度", &interpSpeed_);
}
