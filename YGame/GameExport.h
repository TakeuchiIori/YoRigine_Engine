#pragma once

#ifdef GAME_BUILD_DLL
#define GAME_API __declspec(dllexport)
#else
#define GAME_API __declspec(dllimport)
#endif

extern "C" {
    // 引数なし、戻り値なしの単純な形に戻す
    GAME_API void GameInit();
    GAME_API void GameUpdate();
    GAME_API void GameRender();
    GAME_API void GameShutdown();
}