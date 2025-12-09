#include <windows.h>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>
#include "Framework/Framework.h"

//-----------------------------------------------------------------------------
// デバッグ用 Logger 関数
// OutputDebugStringA で Visual Studio の出力ウィンドウにログを送信します
// DLL側からも同じ Logger 関数を定義し、利用する必要があります。
//-----------------------------------------------------------------------------
void Logger(const char* message) {
    OutputDebugStringA(message);
    OutputDebugStringA("\n"); // 改行コードを追加
}

//-----------------------------------------------------------------------------
// 設定
//-----------------------------------------------------------------------------
#ifdef _DEBUG
    // Debug構成の場合: generated/outputs/Debug/
const std::wstring CONFIG_PATH = L"../generated/outputs/Debug/";
#else
    // Release構成の場合: generated/outputs/Release/
const std::wstring CONFIG_PATH = L"../generated/outputs/Release/";
#endif

// DLLの実際のパスを、実行場所からの相対パスとして指定
const std::wstring DLL_NAME_ORIGIN = CONFIG_PATH + L"YGame.dll";      // コンパイラが出力する原本
const std::wstring DLL_NAME_COPY = CONFIG_PATH + L"YGame_Hot.dll";

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
    Logger("--- Hot Reload Attempt Start ---");

    // もし古いゲームが動いていたら終了させる
    if (gameInstance) {
        Logger("  Calling DestroyGameFn...");

        DestroyGameFn(gameInstance); // DestroyGame() の中で Finalize() と delete が呼ばれる
        gameInstance = nullptr;
    }

    // DLLを解放する
    if (hGameDLL) {
        Logger("  FreeLibrary...");
        FreeLibrary(hGameDLL);
        hGameDLL = nullptr;
    }

    // 原本(YGame.dll)があるか確認
    if (!std::filesystem::exists(DLL_NAME_ORIGIN)) {
        Logger("Error: YGame.dll not found.");
        return false;
    }

    // 更新日時を記録（次の変更検知のため）
    lastWriteTime = std::filesystem::last_write_time(DLL_NAME_ORIGIN);

    // DLLをコピーする
    // 【注意】DLL_NAME_ORIGINの場所は、EXEの実行場所からの相対パスとして解決されます。
    // Premake設定でdebugdirを%{wks.basedir}にした場合、YGame.dllは
    // %{wks.basedir}/generated/outputs/%{cfg.buildcfg}/YGame.dll にあるはずです。
    Logger("  Copying DLL (Origin -> Hot)...");
    std::filesystem::copy_file(DLL_NAME_ORIGIN, DLL_NAME_COPY, std::filesystem::copy_options::overwrite_existing);

    // コピーしたDLLをロード
    Logger("  LoadLibraryW (Hot DLL)...");
    hGameDLL = LoadLibraryW(DLL_NAME_COPY.c_str());
    if (!hGameDLL) {
        MessageBoxW(nullptr, L"DLLのロードに失敗しました", L"Error", MB_OK);
        return false;
    }

    // 関数ポインタを取り出す
    CreateGameFn = (CreateGameFunc)GetProcAddress(hGameDLL, "CreateGame");
    DestroyGameFn = (DestroyGameFunc)GetProcAddress(hGameDLL, "DestroyGame");

    if (!CreateGameFn || !DestroyGameFn) {
        MessageBoxW(nullptr, L"DLL内の関数が見つかりません", L"Error", MB_OK);
        FreeLibrary(hGameDLL);
        hGameDLL = nullptr;
        return false;
    }

    // 新しいゲームインスタンスを生成して初期化
    Logger("Step 8: Calling CreateGame().");
    gameInstance = CreateGameFn();
    Logger("Step 8 Success. Game should be initialized.");

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
    MSG msg = {};
    while (true) {

        // 【修正点】ウィンドウメッセージの取得と処理
        // PeekMessageW を使用し、メッセージがあれば処理し、なければゲームへ制御を渡す
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break; // 終了メッセージを受け取ったらループを抜ける
            }
            TranslateMessage(&msg); // キーボードメッセージを変換
            DispatchMessageW(&msg); // ウィンドウプロシージャへメッセージを渡す
        }

        // ホットリロードのチェック
        try {
            auto currentWriteTime = std::filesystem::last_write_time(DLL_NAME_ORIGIN);
            if (currentWriteTime != lastWriteTime) {
                // ファイルアクセス競合を防ぐために少し待つ
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                LoadGameDLL();
            }
        }
        catch (...) {
            // ファイルアクセス競合などでエラーが出ても落ちないように無視
        }

        // ゲームの更新と描画
        if (gameInstance) {
            // ウィンドウメッセージ処理（WM_QUITなど）によって IsEndRequst が設定される
            if (gameInstance->IsEndRequst()) {
                break;
            }

            gameInstance->Update();
            gameInstance->Draw();
        }
    }

    // 終了処理
    if (gameInstance) {
        DestroyGameFn(gameInstance);
    }

    return 0;
}