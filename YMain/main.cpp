// main.cpp

#include <windows.h>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>

// ★重要: 具体的なクラス(MyGame.h)はインクルードしない！
// 代わりに基底クラス(Framework.h)だけを知っていればOK
#include "Framework/Framework.h"

//-----------------------------------------------------------------------------
// 設定
//-----------------------------------------------------------------------------
const std::wstring DLL_NAME_ORIGIN = L"YGame.dll";      // コンパイラが出力する原本
const std::wstring DLL_NAME_COPY = L"YGame_Hot.dll";  // 実行用にコピーするやつ

//-----------------------------------------------------------------------------
// グローバル変数
//-----------------------------------------------------------------------------
HMODULE hGameDLL = nullptr;
Framework* gameInstance = nullptr;

// 関数ポインタの定義（DLLから取得する関数の型）
typedef Framework* (*CreateGameFunc)();
typedef void (*DestroyGameFunc)(Framework*);

CreateGameFunc CreateGameFn = nullptr;
DestroyGameFunc DestroyGameFn = nullptr;

std::filesystem::file_time_type lastWriteTime;

//-----------------------------------------------------------------------------
// DLLのロード・リロード処理
//-----------------------------------------------------------------------------
bool LoadGameDLL() {
    // 1. もし古いゲームが動いていたら終了させる
    if (gameInstance) {
        // ★修正点: Finalize() の呼び出しを削除 (DestroyGame内で処理される)
        // gameInstance->Finalize(); // 終了処理

        DestroyGameFn(gameInstance); // DestroyGame() の中で Finalize() と delete が呼ばれる
        gameInstance = nullptr;
    }

    // 2. DLLを解放する
    if (hGameDLL) {
        FreeLibrary(hGameDLL);
        hGameDLL = nullptr;
    }

    // 3. 原本(YGame.dll)があるか確認
    if (!std::filesystem::exists(DLL_NAME_ORIGIN)) {
        return false;
    }

    // 4. 更新日時を記録（次の変更検知のため）
    lastWriteTime = std::filesystem::last_write_time(DLL_NAME_ORIGIN);

    // 5. DLLをコピーする
    std::filesystem::copy_file(DLL_NAME_ORIGIN, DLL_NAME_COPY, std::filesystem::copy_options::overwrite_existing);

    // 6. コピーしたDLLをロード
    hGameDLL = LoadLibraryW(DLL_NAME_COPY.c_str());
    if (!hGameDLL) {
        MessageBoxW(nullptr, L"DLLのロードに失敗しました", L"Error", MB_OK);
        return false;
    }

    // 7. 関数ポインタを取り出す
    CreateGameFn = (CreateGameFunc)GetProcAddress(hGameDLL, "CreateGame");
    DestroyGameFn = (DestroyGameFunc)GetProcAddress(hGameDLL, "DestroyGame");

    if (!CreateGameFn || !DestroyGameFn) {
        MessageBoxW(nullptr, L"DLL内の関数が見つかりません", L"Error", MB_OK);
        FreeLibrary(hGameDLL);
        hGameDLL = nullptr;
        return false;
    }

    // 8. 新しいゲームインスタンスを生成して初期化
    gameInstance = CreateGameFn(); // CreateGame() の中で Initialize() が呼ばれる
    // ★修正点: Initialize() の呼び出しを削除
    // gameInstance->Initialize(); 

    return true;
}

//-----------------------------------------------------------------------------
// メインエントリーポイント
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    // メモリリーク検出
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // 初回のロード
    if (!LoadGameDLL()) {
        return -1;
    }

    //------------------------------------------------------------
    // メインループ
    //------------------------------------------------------------
    while (true) {

        // 1. ホットリロードのチェック
        try {
            auto currentWriteTime = std::filesystem::last_write_time(DLL_NAME_ORIGIN);
            if (currentWriteTime != lastWriteTime) {
                // ファイルが書き込み中かもしれないので少し待つ
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                LoadGameDLL();
            }
        }
        catch (...) {
            // ファイルアクセス競合などでエラーが出ても落ちないように無視
        }

        // 2. ゲームの更新と描画
        if (gameInstance) {
            // ウィンドウメッセージ処理（×ボタンで終了判定）
            if (gameInstance->IsEndRequst()) {
                break;
            }

            gameInstance->Update();
            gameInstance->Draw();
        }
    }

    // 終了処理
    if (gameInstance) {
        // ★修正点: Finalize() の呼び出しを削除
        // gameInstance->Finalize();
        DestroyGameFn(gameInstance);
    }

    return 0;
}