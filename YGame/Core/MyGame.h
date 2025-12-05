#pragma once
#include "CoreScenes/SceneFactory.h"
#include "SceneSystems/SceneManager.h"
#include "Framework/Framework.h"
#include "OffScreen/OffScreen.h"
#include <SceneSystems/AbstractSceneFactory.h>
#include <../YGame/GameAPI.h >

/// <summary>
/// ゲームのメインクラス
/// </summary>
class  MyGame : public Framework
{

public:

	// 初期化
	void Initialize() override;

	// 終了
	void Finalize() override;

	// 更新
	void Update() override;

	// 描画
	void Draw() override;

private:
	OffScreen* offScreen_;
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
};

// DLL側でメモリを確保・解放するための関数
extern "C" {
	GAME_API Framework* CreateGame();
	GAME_API void DestroyGame(Framework* game);
}
