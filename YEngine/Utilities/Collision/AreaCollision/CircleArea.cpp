#include "CircleArea.h"
#include "MathFunc.h"
#include <cmath>

CircleArea::CircleArea(const Vector3& center, float radius)
	: center_(center)
	, radius_(radius)
	, debugSegments_(64)
{
}

void CircleArea::Initialize(const Vector3& center, float radius)
{
	center_ = center;
	radius_ = radius;
	debugSegments_ = 64;
	isActive_ = true;
	wasInside_ = false;
}

bool CircleArea::IsInside(const Vector3& position) const
{
	// XZ平面での距離を計算（Y軸は制限しない）
	Vector3 toPosition = position - center_;
	toPosition.y = 0.0f;

	float distanceSq = LengthSquared(toPosition);
	return distanceSq <= (radius_ * radius_);
}

Vector3 CircleArea::ClampPosition(const Vector3& position) const
{
	// XZ平面での距離を計算（Y軸は制限しない）
	Vector3 toPosition = position - center_;
	toPosition.y = 0.0f;

	float distance = Length(toPosition);

	// 境界内ならそのまま返す
	if (distance <= radius_) {
		return position;
	}

	// 境界を超えている場合、境界上に補正
	Vector3 direction = Normalize(toPosition);
	Vector3 clampedXZ = center_ + direction * radius_;

	// Y座標は元のまま
	return Vector3(clampedXZ.x, position.y, clampedXZ.z);
}

float CircleArea::GetDistanceFromBoundary(const Vector3& position) const
{
	// XZ平面での距離を計算
	Vector3 toPosition = position - center_;
	toPosition.y = 0.0f;

	float distance = Length(toPosition);
	return radius_ - distance;  // 正の値 = 内側、負の値 = 外側
}

void CircleArea::Draw(Line* line)
{
	if (!line || !isDebugDrawEnabled_) {
		return;
	}

	float angleStep = (2.0f * 3.14159265f) / debugSegments_;

	for (int i = 0; i < debugSegments_; ++i) {
		float angle1 = i * angleStep;
		float angle2 = (i + 1) * angleStep;

		Vector3 point1 = {
			center_.x + radius_ * cosf(angle1),
			center_.y,
			center_.z + radius_ * sinf(angle1)
		};

		Vector3 point2 = {
			center_.x + radius_ * cosf(angle2),
			center_.y,
			center_.z + radius_ * sinf(angle2)
		};

		// ラインを描画用バッファに追加
		// 実際の描画は呼び出し側でDrawLine()を実行
	}
}