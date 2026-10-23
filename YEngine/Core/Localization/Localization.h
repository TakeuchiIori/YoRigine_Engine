#pragma once
#include <string>

// =============================================================================
//  Localization ― エディタの表示言語 (日本語 / English)
//
//  文字列はコードに **日本語のまま** 書き、表示する所を TR("…") で包む。
//  English を選ぶと Resources/Lang/en.json (元の日本語 → 英語) を引いて差し替える。
//  表に無いものは元の日本語のまま出る (訳し忘れても壊れない)。
//  日本語を選んでいるときは表を引かず、渡された文字列をそのまま返す。
//
//    ImGui::Checkbox(TR("影を出す"), &on);
//    ImGui::Text(TR("選択数: %d"), n);       // 書式指定子は訳でも同じ並びにする
//    ImGui::Button(TR("保存##scene"));        // "##" 以降 (ImGui の ID) は訳さず残す
//
//  戻り値は表の中の文字列を指す。言語を切り替えても前の戻り値は生きている。
//  言語の保存先は Editor の editor_settings.ini (Editor::SaveSettings が書く)。
// =============================================================================
namespace YoRigine::loc {

enum class Language { Japanese = 0, English = 1 };

// 起動時に 1 回。langDir は en.json を置くフォルダ (末尾 '/' 付き)。
void Initialize(const std::string& langDir);

void        SetLanguage(Language lang);   // English の表は初めて必要になったときに読む
Language    Current();
const char* LanguageName(Language lang);  // メニュー表示用 («日本語» / «English»)
const char* LanguageCode(Language lang);  // 設定ファイル用 («ja» / «en»)
Language    LanguageFromCode(const std::string& code);

// 表示する文字列を引く。nullptr はそのまま返す。
const char* Tr(const char* src);
// std::string 版。動的に組み立てた名前 (パネル名など) 用。
const char* Tr(const std::string& src);

// 状態を持つ ImGui 要素 (ウィンドウ・タブ・CollapsingHeader) 用。«訳###元の名前» を返す。
// ImGui は "###" 以降だけで ID を作るので、言語を切り替えても
// 位置・ドッキング・開閉・選択中のタブが同じ要素として引き継がれる。
// (TR("名前") をそのまま渡すと、言語ごとに ID が変わって状態が飛ぶ)
// DockBuilderDockWindow などへ名前で渡すときは StableId(name) を使う。
const char* TrId(const std::string& name);
std::string StableId(const std::string& name);   // "###" + name

// Combo などに渡す «名前の並び» をまとめて訳す。返す配列は次に呼ぶまで有効。
const char* const* TrList(const char* const* items, int count);

// en.json を読み直す (訳を書き足した後、再起動せずに確かめたいとき)
void Reload();

} // namespace YoRigine::loc

#define TR(s) ::YoRigine::loc::Tr(s)
