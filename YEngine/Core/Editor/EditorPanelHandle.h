#pragma once
#ifdef USE_IMGUI

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

class Editor;

// =============================================================================
//  EditorPanelHandle
//  Editor に登録したパネルの「登録証」。RAII
//  でパネルの寿命を持ち主に結びつける。
//
//  使い方:
//    class Foo {
//        EditorPanelHandle panel_;   // ← Foo と同じ寿命になる
//        void RegisterEditorUI(Editor& editor) {
//            panel_ = editor.RegisterPanel("Foo", [this]{ DrawImGui(); },
//            "AllScene", "システム");
//        }
//    };
//    Foo が破棄される → panel_ のデストラクタが走る → Editor
//    から自動で登録解除。 ラムダが this
//    をキャプチャしていても、持ち主より長く生き残らない。
//
//  設計上の注意:
//    ・ムーブ専用 (コピー不可)。二重解除を防ぐため。
//    ・Editor より後に破棄されても安全 (alive_ が expired
//    になるので何もしない)。
//      Editor / 各マネージャはどちらも static
//      変数なので、破棄順が保証されない。
//    ・解除は「名前 +
//    id」で照合する。同名パネルが別の持ち主に上書きされた後でも、
//      古いハンドルが新しい登録を巻き添えで消さない。
// =============================================================================
class EditorPanelHandle {
public:
  EditorPanelHandle() = default;

  // Editor::RegisterPanel だけが使う。外から直接作ることは想定しない
  EditorPanelHandle(Editor *editor, std::string name, uint64_t id,
                    std::weak_ptr<void> alive)
      : editor_(editor), name_(std::move(name)), id_(id),
        alive_(std::move(alive)) {}

  ~EditorPanelHandle() { Reset(); }

  EditorPanelHandle(const EditorPanelHandle &) = delete;
  EditorPanelHandle &operator=(const EditorPanelHandle &) = delete;

  EditorPanelHandle(EditorPanelHandle &&other) noexcept { MoveFrom(other); }
  EditorPanelHandle &operator=(EditorPanelHandle &&other) noexcept {
    if (this != &other) {
      Reset(); // 自分が持っていた登録を先に解除してから受け取る
      MoveFrom(other);
    }
    return *this;
  }

  // 登録を解除して空のハンドルに戻す (実装は Editor.cpp。Editor
  // の定義が必要なため)
  void Reset();

  // 何かの登録を保持しているか
  bool IsValid() const { return editor_ != nullptr; }

private:
  void MoveFrom(EditorPanelHandle &other) noexcept {
    editor_ = std::exchange(other.editor_, nullptr);
    name_ = std::move(other.name_);
    id_ = std::exchange(other.id_, 0);
    alive_ = std::move(other.alive_);
  }

  Editor *editor_ = nullptr;
  std::string name_;
  uint64_t id_ = 0;
  std::weak_ptr<void> alive_; // Editor が生きているかの確認用
};

#endif // USE_IMGUI
