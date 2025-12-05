#include "DebugCamera.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include <Systems/Input/Input.h>
#include <DirectXMath.h>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

/// <summary>
/// 初期化処理
/// </summary>
void DebugCamera::Initialize()
{
	InitJson();

	// カメラ初期位置
	translate_ = { 0.0f, 6.0f, -40.0f };

	// 初期マウス位置取得
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	prevMousePos_ = input->GetMousePosition();
}

/// <summary>
/// 更新処理
/// </summary>
void DebugCamera::Update()
{
	UpdateInput();

	// ビュー行列を更新
	matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));

#ifdef USE_IMGUI
	// ImGui描画（必要に応じて）
#endif
}

/// <summary>
/// 入力更新処理（マウス・キーボード・コントローラー）
/// </summary>
void DebugCamera::UpdateInput()
{
	YoRigine::Input* input = YoRigine::Input::GetInstance();

	//------------------------------------------------------------
	// マウスによる操作
	//------------------------------------------------------------
	Vector2 currentMousePos = input->GetMousePosition();

	// 右クリックでカメラ回転
	if (input->IsPressMouse(1)) {
		if (!isDragging_) {
			isDragging_ = true;
			prevMousePos_ = currentMousePos;
		}

		float deltaX = currentMousePos.x - prevMousePos_.x;
		float deltaY = currentMousePos.y - prevMousePos_.y;

		rotate_.y += deltaX * rotateSpeed_ * 0.01f;
		rotate_.x += deltaY * rotateSpeed_ * 0.01f;

		// ピッチ角制限
		rotate_.x = std::clamp(rotate_.x, -1.5f, 1.5f);
	} else {
		isDragging_ = false;
	}

	// マウス位置更新
	prevMousePos_ = currentMousePos;

	// ホイール前後移動
	int32_t wheel = input->GetWheel();
	if (wheel != 0) {
		Vector3 forward = TransformNormal({ 0, 0, 1.0f }, MakeRotateMatrixXYZ(rotate_));
		translate_ += forward * static_cast<float>(wheel) * moveSpeed_ * 0.1f;
	}

	//------------------------------------------------------------
	// キーボードによる移動
	//------------------------------------------------------------
	Vector3 moveDirection = { 0, 0, 0 };

	if (input->PushKey(DIK_W)) moveDirection.z += 1.0f;
	if (input->PushKey(DIK_S)) moveDirection.z -= 1.0f;
	if (input->PushKey(DIK_A)) moveDirection.x -= 1.0f;
	if (input->PushKey(DIK_D)) moveDirection.x += 1.0f;
	if (input->PushKey(DIK_Q) || input->IsPressMouse(3)) moveDirection.y += 1.0f;
	if (input->PushKey(DIK_E) || input->IsPressMouse(4)) moveDirection.y -= 1.0f;

	// Shiftキーで加速
	float speedMultiplier = (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT)) ? 3.0f : 1.0f;

	if (Length(moveDirection) > 0.0f) {
		moveDirection = Normalize(moveDirection);

		Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotate_);
		Vector3 transformedMove = TransformNormal(moveDirection, rotMat);

		translate_ += transformedMove * moveSpeed_ * speedMultiplier;
	}

	//------------------------------------------------------------
	// コントローラーによる操作
	//------------------------------------------------------------
	if (input->IsControllerConnected()) {
		XINPUT_STATE joyState;
		if (input->GetJoystickState(0, joyState)) {

			// 右スティックで回転
			Vector3 rotateMove{};
			rotateMove.y += static_cast<float>(joyState.Gamepad.sThumbRX) * rotateSpeedController_ * 0.0001f;
			rotateMove.x -= static_cast<float>(joyState.Gamepad.sThumbRY) * rotateSpeedController_ * 0.0001f;
			rotate_ += rotateMove;

			rotate_.x = std::clamp(rotate_.x, -1.5f, 1.5f);

			// 左スティックで移動
			Vector3 stickMove{};
			stickMove.x = static_cast<float>(joyState.Gamepad.sThumbLX);
			stickMove.z = static_cast<float>(joyState.Gamepad.sThumbLY);

			if (Length(stickMove) > 0.0f) {
				stickMove = Normalize(stickMove) * moveSpeedController_ * 0.1f;

				Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotate_);
				Vector3 transformedMove = TransformNormal(stickMove, rotMat);
				translate_ += transformedMove;
			}

			// トリガーで上下移動
			if (joyState.Gamepad.bLeftTrigger > 0) {
				translate_.y -= moveSpeedController_ * 0.1f * (joyState.Gamepad.bLeftTrigger / 255.0f);
			}
			if (joyState.Gamepad.bRightTrigger > 0) {
				translate_.y += moveSpeedController_ * 0.1f * (joyState.Gamepad.bRightTrigger / 255.0f);
			}
		}
	}
}

/// <summary>
/// JSON初期化処理（カメラ設定登録）
/// </summary>
void DebugCamera::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("DebugCamera", "Resources/Json/Cameras");
	jsonManager_->SetCategory("Cameras");
	jsonManager_->SetSubCategory("DebugCamera");

	jsonManager_->Register("Translate", &translate_);
	jsonManager_->Register("Rotate", &rotate_);
	jsonManager_->Register("RotateSpeed", &rotateSpeed_);
	jsonManager_->Register("RotateSpeed Controller", &rotateSpeedController_);
	jsonManager_->Register("MoveSpeed", &moveSpeed_);
	jsonManager_->Register("MoveSpeed Controller", &moveSpeedController_);
}

/// <summary>
/// JSONのImGui表示（任意で使用）
/// </summary>
void DebugCamera::JsonImGui()
{
	// JSON用ImGui（必要に応じて拡張）
}

/// <summary>
/// デバッグ用ImGui表示
/// </summary>
void DebugCamera::ImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("DebugCamera Info");

	ImGui::DragFloat3("Position", &translate_.x, 0.1f);
	ImGui::DragFloat3("Rotation", &rotate_.x, 0.01f);
	ImGui::DragFloat("Rotate Speed", &rotateSpeed_, 0.01f, 0.01f, 2.0f);
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, 0.1f, 10.0f);

	ImGui::End();
#endif
}
