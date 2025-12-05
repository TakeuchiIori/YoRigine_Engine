#pragma once

// C++
#include <memory>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <type_traits>

// Engine
#include "BaseCollider.h"

// コライダーをプール管理するクラス
// 再利用可能なコライダーを保持し、生成コストを削減する
class ColliderPool
{
public:
	///************************* シングルトン *************************///

	// インスタンス取得
	static ColliderPool* GetInstance();

public:
	///************************* コライダー取得 *************************///

	// コライダーを取得または再利用
	// 使用中でないコライダーがあればそれを返す
	// なければ新規に生成してプールに追加
	template <typename T>
	std::shared_ptr<T> GetCollider();

public:
	///************************* プール管理 *************************///

	// 全コライダーのクリア
	void Clear();

private:
	///************************* 内部管理 *************************///

	~ColliderPool() = default;

	// 型ごとのコライダープール
	std::unordered_map<std::type_index, std::vector<std::shared_ptr<BaseCollider>>> pool_;
};

// コライダー取得処理
template<typename T>
inline std::shared_ptr<T> ColliderPool::GetCollider()
{
	static_assert(std::is_base_of<BaseCollider, T>::value, "T must be derived from BaseCollider");

	auto& it = pool_[typeid(T)];

	// 未使用のコライダーを検索
	for (auto& collider : it) {
		if (!collider->GetIsActive()) {
			collider->SetActive(true);
			return std::static_pointer_cast<T>(collider);
		}
	}

	// 未登録なら新規生成
	auto newCollider = std::make_shared<T>();
	newCollider->SetActive(true);
	it.push_back(newCollider);
	return newCollider;
}
