#pragma once
#include <string>
#include <vector>
#include "Vector3.h"


///************************* 状態定義 *************************///
enum class BattleEnemyState {
	Idle,       // 待機
	Approach,   // 接近
	Attack,     // 攻撃
	Damaged,    // 被弾
	Dead        // 撃破
};

///************************* 基本データ *************************///
struct BattleEnemyData {
	std::string enemyId;
	std::string modelPath;

	int currentHp_;
	int maxHp_;

	int level = 1;
	int hp = 100;
	int attack = 15;
	int defense = 10;
	float moveSpeed = 5.0f;

	// 接近状態に入る距離
	float approachStateRange = 15.0f;
	// 攻撃状態に入る距離
	float attackStateRange = 10.0f;
	std::string aiType = "aggressive";

	std::vector<std::string> attackPatterns = { "rush","spin","charge","combo","leap" };
};

///************************* ノックバック *************************///
struct KnockbackData {
	bool isKnockingBack_ = false;
	Vector3 knockbackDirection_;
	float knockbackPower_ = 0.0f;
	float knockbackDuration_ = 0.0f;
	float knockbackTimer_ = 0.0f;
};
