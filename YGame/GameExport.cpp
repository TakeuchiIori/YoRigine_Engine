// GameExport.cpp

#include "GameExport.h"
#include "Core/MyGame.h" // ★ あなたのゲームのメインクラスをインクルード
#include <iostream>
#include <Debugger/Logger.h>

// Framework* インスタンスを生成して返す
// 初期化処理（Initialize）もこの関数内で行う
GAME_API Framework* CreateGame() {
    Logger("[DLL] Create Game Instance");

    // MyGame インスタンスを生成
    Framework* gameInstance = new MyGame();

    // ★修正点1: DLL内で Initialize() を実行する！
    // Singletonの不整合を防ぐため、初期化処理をDLLに閉じ込める
    gameInstance->Initialize();

    return gameInstance;
}

// Framework* インスタンスを破棄する
// 終了処理（Finalize）もこの関数内で行う
GAME_API void DestroyGame(Framework* pGame) {
    // ★ホットリロードの実験用ログ (ここを書き換えてビルドし、ホットリロードを確認)
    Logger("[DLL] Hot Reload Running... (New Code V3.0)");

    if (pGame) {
        // ★修正点2: DLL内で Finalize() を実行する！
        // DLLが確保したリソースを、DLL内で解放する
        pGame->Finalize();

        Logger("[DLL] Destroy Game Instance");
        // Framework* 型のポインタを delete
        delete pGame;
    }
}