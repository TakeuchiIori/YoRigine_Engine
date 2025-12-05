#pragma once
#include "WorldTransform/WorldTransform.h"
#include "Object3D/Object3d.h"
#include "Systems/Camera/Camera.h"
#include "Loaders/Json/JsonManager.h"
#include "Collision/Core/CollisionDirection.h"
#include "Collision/OBB/OBBCollider.h"
#include <Particle/ParticleEmitter.h>


class Player;
/// <summary>
/// プレイヤーの盾クラス
/// </summary>
class PlayerShield
{
public:
	///************************* 基本関数 *************************///
	~PlayerShield();
	void Initialize();

	void Update();

	void Draw();
	void DrawShadow();
	void DrawCollision();

	void SetPlayer(Player* player) { player_ = player; }
	void SetObject(Object3d* obj3d) { obj3d_ = obj3d; }
	void SetCamera(Camera* camera) { camera_ = camera; }

	bool IsJointValid() const { return isValidJoint_; }
	WorldTransform& GetWorldTransform() { return wt_; }

public:
	///************************* 当たり判定 *************************///
	void OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other);
	void OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir);

private:
	///************************* 内部処理関数 *************************///

	// 初期化関数
	void InitCollision();
	void InitJson();

	// 手ジョイントのインデックスを探す
	void FindHandJointIndex();

	// 武器の位置をプレイヤーに合わせる
	void SetPlayerWeaponPosition();

private:
	///************************* ポインタ *************************///
	Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	Object3d* obj3d_ = nullptr;

	WorldTransform wt_;
	std::unique_ptr<Object3d> obj_;
	std::unique_ptr<YoRigine::JsonManager> jsonManager_;
	std::unique_ptr<YoRigine::JsonManager> jsonCollider_;
	std::shared_ptr<OBBCollider> obbCollider_;
	std::unique_ptr<ParticleEmitter> testEmitter_;

	///************************* ジョイント関連 *************************///

	std::string handJointName_ = "mixamorig:LeftHand";   // 手ジョイント名
	int handleIndex_ = 0;                                // 手ジョイントのインデックス
	bool isValidJoint_ = false;                          // ジョイントが有効かどうか

	Vector3 offsetPos_{};
	Vector3 offsetRot_{};
	Vector3 offsetScale_{ 1.0f,1.0f,1.0f };
};

