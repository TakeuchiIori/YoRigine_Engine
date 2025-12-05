#include "GameExport.h"

// あなたのゲームのメインクラスをインクルード
// パスは実際の構成に合わせて調整してください（例: "Core/MyGame.h" など）
#include "Core/MyGame.h" 
#include <iostream>

// ゲームのインスタンスを保持する変数
// MyGameはFrameworkを継承しているので、Frameworkのポインタで持ちます
static Framework* s_GameInstance = nullptr;

// 初期化
void GameInit() {
    std::cout << "[DLL] Game Init" << std::endl;

    // インスタンスがなければ生成
    if (s_GameInstance == nullptr) {
        s_GameInstance = new MyGame(); // ここであなたの MyGame を生成！
        s_GameInstance->Initialize();
    }
}

// 更新
void GameUpdate() {
    if (s_GameInstance) {
        // あなたの Framework::Update() には引数がないようなので、
        // deltaTimeは渡さずに呼び出します（必要ならFramework側を改造してください）
        s_GameInstance->Update();
    }

    // ★ホットリロードの実験用ログ
    // ここを書き換えてビルドすると、実行中にログが変わるのが確認できます
    // std::cout << "[DLL] Update Running..." << std::endl;
}

// 描画
void GameRender() {
    if (s_GameInstance) {
        s_GameInstance->Draw();
    }
}

// 終了処理
void GameShutdown() {
    std::cout << "[DLL] Game Shutdown" << std::endl;

    if (s_GameInstance) {
        s_GameInstance->Finalize();
        delete s_GameInstance;
        s_GameInstance = nullptr;
    }
}