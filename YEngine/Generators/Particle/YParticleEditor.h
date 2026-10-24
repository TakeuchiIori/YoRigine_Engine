#pragma once

#ifdef USE_IMGUI
#include "FileOperations/FileBrowser.h"
#include "Vector3.h"
#include "YParticleSystemInspector.h"
#include <json.hpp>
#include <memory>
#include <string>
#include <unordered_map>

class YParticleSystem;

/// <summary>
/// CPU パーティクル（YParticleSystem）のエディタ。
///
///   ┌ ツールバー: 保存 / Undo / Redo / テスト発生 ────────────────┐
///   ├ システム一覧 ┬ スタック（モジュール） ┬ インスペクタ ─────────┤
///
/// ・Ctrl+S
/// で読み込み元ファイルへ上書き保存（バンドルなら中の同名システムだけ差し替え）
/// ・編集は全て Undo / Redo できる（システムごとに履歴を持つ）
/// ・結果は画面右下の通知に出る
/// </summary>
class YParticleEditor {
public:
  static YParticleEditor &GetInstance() {
    static YParticleEditor instance;
    return instance;
  }

  YParticleEditor(const YParticleEditor &) = delete;
  YParticleEditor &operator=(const YParticleEditor &) = delete;

  /// メインエディタウィンドウを表示（Editor の GameUI パネル内で呼ばれる）
  void ShowEditorWindow();

  void SetSelectedSystem(const std::string &systemName) {
    selectedSystemName_ = systemName;
  }
  const std::string &GetSelectedSystem() const { return selectedSystemName_; }

private:
  YParticleEditor();
  ~YParticleEditor() = default;

  //=================================================================
  // 画面の部品
  //=================================================================

  void DrawMenuBar();
  void DrawToolbar(YParticleSystem *system);
  void DrawSystemList();
  void DrawPopups();
  void HandleShortcuts(YParticleSystem *system);
  void UpdateTestEmit(YParticleSystem *system);

  //=================================================================
  // ファイル操作
  //=================================================================

  /// 選択中システムを保存する。読み込み元があればそこへ、無ければ「名前を付けて保存」を開く
  void SaveSelected();

  /// system を path へ保存する。path が既存のバンドル（"systems" 配列を持つ
  /// JSON）なら
  /// その中の同名システムだけを差し替え、他のシステムやグループは触らない
  bool SaveSystemIntoFile(YParticleSystem &system, const std::string &path);

  /// 単体ファイル / バンドル / 全システム一括ファイル のどれでも読み込む
  bool LoadFile(const std::string &path);

  /// 全システムを 1 ファイルへ（旧形式 {"version","systems":[...]}）
  bool SaveAllSystemsToFile(const std::string &path);

  //=================================================================
  // ヘルパー
  //=================================================================

  YParticleSystem *GetSelectedSystemPtr();
  YParticleSystemInspector &GetInspector(const std::string &systemName);
  std::string MakeUniqueSystemName(const std::string &base) const;
  void OpenSaveAs(const std::string &defaultFileName);

  //=================================================================
  // 状態
  //=================================================================

  std::string selectedSystemName_;

  // システムごとの編集状態（Undo 履歴・未保存判定）。切り替えても履歴が消えない
  std::unordered_map<std::string, std::unique_ptr<YParticleSystemInspector>>
      inspectors_;

  float listWidth_ = 200.0f;
  char listFilter_[64] = "";

  // 新規作成 / 削除確認
  bool showNewDialog_ = false;
  char newNameBuffer_[128] = "";
  int newMaxParticles_ = 1000;
  bool showDeleteConfirm_ = false;

  // テスト発生（エディタ上で見た目を確認するため、その場で粒を出す）
  int testEmitCount_ = 30;
  Vector3 testEmitPos_ = {0.0f, 1.0f, 0.0f};
  bool testEmitLoop_ = false;
  float testEmitInterval_ = 0.5f;
  float testEmitTimer_ = 0.0f;

  // ファイルダイアログ
  const std::string jsonDirectory_ = "Resources/Json/YParticleSystems/";
  YoRigine::FileBrowser saveAsBrowser_;
  YoRigine::FileBrowser loadBrowser_;
  YoRigine::FileBrowser saveAllBrowser_;
  bool showSaveAsPopup_ = false;
  bool showLoadPopup_ = false;
  bool showSaveAllPopup_ = false;
  char saveAsNameBuf_[256] = "";
};

#endif // USE_IMGUI
