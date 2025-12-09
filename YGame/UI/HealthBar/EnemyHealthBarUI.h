#pragma once

#include "Systems/UI/UIBase.h"
#include "Matrix4x4.h"
#include "Vector3.h"

// 前方宣言
class BattleEnemy;
class Camera;

class EnemyHealthBarUI {
public:
    // コンストラクタ
    EnemyHealthBarUI(const BattleEnemy* enemy, Camera* camera);

    // 初期化 (UIBase::Initializeをオーバーロード)
    void Initialize();

    // 更新処理 (ビルボード処理と表示ポリシーの適用)
    void Update();

	// 描画処理
    void Draw();
private:
    const BattleEnemy* targetEnemy_ = nullptr; // 追従対象の敵
    Camera* camera_ = nullptr;                 // カメラ

    // HPバーの背景とゲージ用
    UIBase* bgHP_;
    UIBase* barHP_;

    // HPの割合
    float currentRatio_ = 1.0f;

    // ビルボード（3D空間）の位置
    Vector3 worldPosition_;
};