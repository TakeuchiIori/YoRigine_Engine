#pragma once
#include "../../BattleEnemy.h"
#include "BattleRushAttackState.h"
#include "BattleLeapAttackState.h"
#include "BattleSpinAttackState.h"
#include "BattleChargeAttackState.h"
#include "BattleComboAttackState.h"
#include <memory>
#include <random>
#include <vector>
#include <string>
#include <algorithm>

/// <summary>
/// 敵の攻撃パターンを選択するヘルパークラス
/// </summary>
class AttackSelector {
public:
	/// <summary>
	/// 敵のデータに基づいてランダムに攻撃状態を生成
	/// </summary>
	static std::unique_ptr<IEnemyState<BattleEnemy>> SelectRandomAttack(
		const BattleEnemy& enemy)
	{
		const auto& patterns = enemy.GetEnemyData().attackPatterns;

		if (patterns.empty()) {
			// デフォルトは突進攻撃
			return std::make_unique<BattleRushAttackState>();
		}

		// ランダムに選択
		static std::random_device rd;
		static std::mt19937 gen(rd());
		std::uniform_int_distribution<size_t> dist(0, patterns.size() - 1);

		size_t index = dist(gen);
		return CreateAttackState(patterns[index]);
	}

	/// <summary>
	/// 距離に応じて最適な攻撃を選択
	/// </summary>
	static std::unique_ptr<IEnemyState<BattleEnemy>> SelectSmartAttack(
		const BattleEnemy& enemy)
	{
		if (!enemy.GetPlayer()) {
			return std::make_unique<BattleRushAttackState>();
		}

		const auto& patterns = enemy.GetEnemyData().attackPatterns;
		Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
		float distance = Length(toPlayer);

		// 遠距離 (7.0f以上)
		if (distance > 7.0f) {
			if (HasPattern(patterns, "leap")) {
				return std::make_unique<BattleLeapAttackState>();
			}
			if (HasPattern(patterns, "charge")) {
				return std::make_unique<BattleChargeAttackState>();
			}
		}
		// 中距離 (3.0f ~ 7.0f)
		else if (distance > 3.0f) {
			if (HasPattern(patterns, "rush")) {
				return std::make_unique<BattleRushAttackState>();
			}
		}
		// 近距離 (3.0f未満)
		else {
			if (HasPattern(patterns, "spin")) {
				return std::make_unique<BattleSpinAttackState>();
			}
			if (HasPattern(patterns, "combo")) {
				return std::make_unique<BattleComboAttackState>();
			}
		}

		// フォールバック：ランダム選択
		return SelectRandomAttack(enemy);
	}

private:
	/// <summary>
	/// 攻撃名から状態を生成
	/// </summary>
	static std::unique_ptr<IEnemyState<BattleEnemy>> CreateAttackState(
		const std::string& patternName)
	{
		if (patternName == "rush") {
			return std::make_unique<BattleRushAttackState>();
		}
		else if (patternName == "leap") {
			return std::make_unique<BattleLeapAttackState>();
		}
		else if (patternName == "spin") {
			return std::make_unique<BattleSpinAttackState>();
		}
		else if (patternName == "charge") {
			return std::make_unique<BattleChargeAttackState>();
		}
		else if (patternName == "combo") {
			return std::make_unique<BattleComboAttackState>();
		}

		// デフォルト
		return std::make_unique<BattleRushAttackState>();
	}

	/// <summary>
	/// パターンリストに指定の攻撃が含まれるか
	/// </summary>
	static bool HasPattern(const std::vector<std::string>& patterns,
		const std::string& name)
	{
		return std::find(patterns.begin(), patterns.end(), name) != patterns.end();
	}
};