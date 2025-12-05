#pragma once
#include "Vector3.h"

struct TrailParams {
	// トレイルエミッターにするかどうか
	bool isTrail = false;
	// どのくらい移動したら粒を出すか
	float minDistance = 0.1f;
	// トレイル専用の寿命（0 の場合は粒の元々の寿命を使用）
	float lifeTime = 0.0f;
	// 1回で何粒出すか
	float emissionCount = 1;
	// 親のスケールを反映するか
	bool inheritScale = true;
};