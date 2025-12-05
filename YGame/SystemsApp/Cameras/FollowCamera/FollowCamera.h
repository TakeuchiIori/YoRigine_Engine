#pragma once
#include <Vector3.h>
#include <WorldTransform/WorldTransform.h>
#include "Loaders/Json/JsonManager.h"

/// <summary>
/// 追従カメラクラス
/// </summary>
class FollowCamera
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void Update();
	void UpdateInput();
	void FollowProsess();

	Vector3 translate_ = { 0,0,0 };
	Vector3 scale_ = { 1,1,1 };
	Vector3 rotate_ = { 0,0,0 };

	Matrix4x4 matView_ = {};

	void SetTarget(const WorldTransform& target) { target_ = &target; }
	void SetPosition(const Vector3& position) { translate_ = position; }
	void SetIsCloseUp(bool isCloseUp) { isCloseUp_ = isCloseUp; }

private:
	void InitJson();
private:
	///************************* メンバ変数 *************************///
	std::unique_ptr <YoRigine::JsonManager> jsonManager_;
	Vector3 rotation_;
	float kDeadZoneL_ = 100.0f;
	float kRotateSpeed_ = 0.1f;
	// 追従対象
	const WorldTransform* target_;
	bool isCloseUp_ = false;
	Vector3 offset_ = { 0.0f, 6.0f, -40.0f };
	float closeUpScale_ = 0.3f;
	float interpSpeed_ = 5.0f;         // 補間速度（Jsonで調整）

	float currentScale_ = 1.0f;        // 現在のスケール（補間に使用）
};

