#include "DebugCamera.h"
#include "MathFunc.h"
#include <Systems/Input/Input.h>
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#include "Core/Editor/Editor.h"
#endif

// ============================================================
// 初期化
// ============================================================
void DebugCamera::Initialize() {
	VirtualCamera::Initialize();

	// ------------------------------------------------------------
	// 初期位置の設定
	// ------------------------------------------------------------
	transform_.translate = { 0.0f, 6.0f, -40.0f };

	YoRigine::Input* input = YoRigine::Input::GetInstance();
	prevMousePos_ = input->GetMousePosition();
}

// ============================================================
// 更新処理
// ============================================================
void DebugCamera::Update() {
	YoRigine::Input* input = YoRigine::Input::GetInstance();

	// ------------------------------------------------------------
	// 有効無効の切り替え（Ctrl + Shift）
	// 押した瞬間だけ切り替える（以前は押している間ずっと切り替わり、止まる側が運任せだった）
	// ------------------------------------------------------------
	const bool ctrl = input->PushKey(DIK_LCONTROL);
	const bool shift = input->PushKey(DIK_LSHIFT);
	if ((ctrl && input->TriggerKey(DIK_LSHIFT)) || (shift && input->TriggerKey(DIK_LCONTROL))) {
		isMoving_ = !isMoving_;
	}

	if (!isMoving_) {
		isFlying_ = false;
		isPanning_ = false;
		return;
	}
	UpdateInput();
}

// ============================================================
// 操作してよいかの判定
// ============================================================
bool DebugCamera::CanStartMouseControl() {
#ifdef USE_IMGUI
	Editor* editor = Editor::GetInstance();
	// エディタ表示中: ゲーム画面の上で、かつギズモを掴んでいないときだけ
	if (editor->GetShowEditor()) return editor->CanPickInGameView();
	// エディタ非表示中: ImGui のウィンドウ（デバッグ表示など）の上でなければ
	return !ImGui::GetIO().WantCaptureMouse;
#else
	return true;
#endif
}

bool DebugCamera::IsTypingText() {
#ifdef USE_IMGUI
	return ImGui::GetIO().WantTextInput;
#else
	return false;
#endif
}

// ============================================================
// 入力情報の更新処理
// ============================================================
void DebugCamera::UpdateInput() {
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	const Vector2 mouse = input->GetMousePosition();
	const Vector2 delta = { mouse.x - prevMousePos_.x, mouse.y - prevMousePos_.y };
	prevMousePos_ = mouse;
	const bool canStart = CanStartMouseControl();

	// ------------------------------------------------------------
	// 操作の開始と終了
	// 押し始めがゲーム画面の上のときだけ開始し、押している間は画面外に出ても続ける
	// ------------------------------------------------------------
	if (input->IsTriggerMouse(1) && canStart) isFlying_ = true;
	if (!input->IsPressMouse(1)) isFlying_ = false;
	if (input->IsTriggerMouse(2) && canStart) isPanning_ = true;
	if (!input->IsPressMouse(2)) isPanning_ = false;

	// ------------------------------------------------------------
	// 見回す（右ボタン中のマウス移動）
	// ------------------------------------------------------------
	if (isFlying_) {
		transform_.rotate.x += delta.y * rotateSpeed_ * 0.1f;
		transform_.rotate.y += delta.x * rotateSpeed_ * 0.1f;
	}

	const Matrix4x4 rotMat = MakeRotateMatrixXYZ(transform_.rotate);
	const Vector3 forward = TransformNormal({ 0.0f, 0.0f, 1.0f }, rotMat);
	const Vector3 right = TransformNormal({ 1.0f, 0.0f, 0.0f }, rotMat);
	const Vector3 up = TransformNormal({ 0.0f, 1.0f, 0.0f }, rotMat);

	// ------------------------------------------------------------
	// 右ボタン中: WASD / QE で飛び回る、ホイールで移動速度を変える
	// ------------------------------------------------------------
	const int32_t wheel = input->GetWheel();
	if (isFlying_) {
		if (!IsTypingText()) {
			const float speed = moveSpeed_ * (input->PushKey(DIK_LSHIFT) ? 3.0f : 1.0f);
			Vector3 move{ 0.0f, 0.0f, 0.0f };
			if (input->PushKey(DIK_W)) move.z += 1.0f;
			if (input->PushKey(DIK_S)) move.z -= 1.0f;
			if (input->PushKey(DIK_A)) move.x -= 1.0f;
			if (input->PushKey(DIK_D)) move.x += 1.0f;
			Vector3 worldMove = TransformNormal(move, rotMat) * speed;
			// Q / E はワールドの上下（傾いていてもまっすぐ昇降する）
			if (input->PushKey(DIK_Q)) worldMove.y -= speed;
			if (input->PushKey(DIK_E)) worldMove.y += speed;
			transform_.translate += worldMove;
		}
		if (wheel != 0) {
			moveSpeed_ = std::clamp(moveSpeed_ * (wheel > 0 ? 1.2f : 1.0f / 1.2f), 0.01f, 50.0f);
		}
	} else if (wheel != 0 && canStart) {
		// ------------------------------------------------------------
		// 右ボタンを押していないとき: ホイールで視線方向へ前後（1 ノッチ = 120）
		// ------------------------------------------------------------
		transform_.translate += forward * (static_cast<float>(wheel) / 120.0f) * moveSpeed_ * 4.0f;
	}

	// ------------------------------------------------------------
	// 中ボタン中: 平行移動（つかんだ場所が付いてくる向き）
	// ------------------------------------------------------------
	if (isPanning_) {
		const float panSpeed = moveSpeed_ * 0.05f;
		transform_.translate += right * (-delta.x * panSpeed) + up * (delta.y * panSpeed);
	}

	// ------------------------------------------------------------
	// コントローラーによる操作
	// ------------------------------------------------------------
	if (input->IsControllerConnected()) {
		XINPUT_STATE joyState;
		if (input->GetJoystickState(0, joyState)) {

			transform_.rotate.x += static_cast<float>(joyState.Gamepad.sThumbRY) * rotateSpeedController_;
			transform_.rotate.y += static_cast<float>(joyState.Gamepad.sThumbRX) * rotateSpeedController_;

			Vector3 stickMove{ 0, 0, 0 };
			if (std::abs(joyState.Gamepad.sThumbLX) > 8000) stickMove.x = static_cast<float>(joyState.Gamepad.sThumbLX);
			if (std::abs(joyState.Gamepad.sThumbLY) > 8000) stickMove.z = static_cast<float>(joyState.Gamepad.sThumbLY);

			if (Length(stickMove) > 0) {
				stickMove = Normalize(stickMove) * moveSpeedController_;
				transform_.translate += TransformNormal(stickMove, rotMat);
			}

			if (joyState.Gamepad.bLeftTrigger > 0) {
				transform_.translate.y -= moveSpeedController_ * 0.1f * (joyState.Gamepad.bLeftTrigger / 255.0f);
			}
			if (joyState.Gamepad.bRightTrigger > 0) {
				transform_.translate.y += moveSpeedController_ * 0.1f * (joyState.Gamepad.bRightTrigger / 255.0f);
			}
		}
	}
}

// ============================================================
// エディタ用GUI描画
// ============================================================
void DebugCamera::DrawDebugGui() {
#ifdef USE_IMGUI
	ImGui::Text("デバッグカメラ設定");
	ImGui::Checkbox("有効 (Ctrl+Shift)", &isMoving_);
	ImGui::TextDisabled("ゲーム画面の上で 右ボタンを押しながら: マウス = 見回す / WASD = 移動 / Q・E = 下降・上昇");
	ImGui::TextDisabled("  Shift = 速く / ホイール = 移動速度   中ボタンドラッグ = 平行移動   ホイール = 前後");
	ImGui::DragFloat("移動速度", &moveSpeed_, 0.01f, 0.01f, 50.0f);
	ImGui::DragFloat("回転速度", &rotateSpeed_, 0.01f);

	ImGui::Separator();
	ImGui::Text("パッド設定");
	ImGui::DragFloat("移動速度 (パッド)", &moveSpeedController_, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("回転速度 (パッド)", &rotateSpeedController_, 0.001f, 0.0f, 0.5f);

	ImGui::Separator();
	ImGui::DragFloat3("位置", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("回転", &transform_.rotate.x, 0.01f);

	float fov = GetFovY();
	if (ImGui::SliderAngle("FOV", &fov, 10.0f, 120.0f)) {
		SetFovY(fov);
	}
#endif
}

// ============================================================
// 保存
// ============================================================
void DebugCamera::Save(nlohmann::json& j) const {
	VirtualCamera::Save(j);
	j["isMoving"] = isMoving_;
	j["moveSpeed"] = moveSpeed_;
	j["rotateSpeed"] = rotateSpeed_;
	j["moveSpeedController"] = moveSpeedController_;
	j["rotateSpeedController"] = rotateSpeedController_;
}

// ============================================================
// 読み込み
// ============================================================
void DebugCamera::Load(const nlohmann::json& j) {
	VirtualCamera::Load(j);
	isMoving_ = j.value("isMoving", true);
	moveSpeed_ = j.value("moveSpeed", 0.5f);
	rotateSpeed_ = j.value("rotateSpeed", 0.05f);
	moveSpeedController_ = j.value("moveSpeedController", 0.1f);
	rotateSpeedController_ = j.value("rotateSpeedController", 0.005f);
}