#pragma once

// Math
#include "Vector3.h"
#include "MathFunc.h"
#include "Matrix4x4.h"

///************************* カメラクラス *************************///
///
/// カメラの位置・回転・投影設定を管理する  
/// シェイク効果や行列更新も行う
///
class Camera
{
public:
	///************************* 構造体 *************************///

	struct CameraShake
	{
		float shakeTimer_ = 0.0f;
		float shakeDuration_ = 0.0f;
		Vector2 shakeMinRange_;
		Vector2 shakeMaxRange_;
		Vector3 originalPosition_;
		bool isShaking_ = false;
	};

public:
	///************************* 基本関数 *************************///

	Camera();
	void Update();
	void UpdateMatrix();

public:
	///************************* カメラ制御 *************************///

	void DefaultCamera();

public:
	///************************* シェイク処理 *************************///

	void Shake(float time, const Vector2 min, const Vector2 max);

private:
	void UpdateShake();

public:
	///************************* アクセッサ *************************///

	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetFovY(float fovY) { fovY_ = fovY; }
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(float farClip) { farClip_ = farClip; }

	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	Vector3 GetRotate() const { return transform_.rotate; }
	Vector3 GetTranslate() const { return transform_.translate; }
	Vector3 GetScale() const { return transform_.scale; }

public:
	///************************* メンバ変数 *************************///

	EulerTransform transform_;
	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;

	float fovY_;
	float aspectRatio_;
	float nearClip_;
	float farClip_;

	CameraShake cameraShake_;
	Vector3 shakeOffset_;
};
