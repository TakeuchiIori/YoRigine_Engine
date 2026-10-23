# ImGui 拡張ライブラリ一覧

本体: Dear ImGui 1.92.6 (docking)。フォルダ直下の拡張（ImGuizmo / ImSequencer / ImCurveEdit / ImGradient / GraphEditor / imgui-node-editor）は以前からあるもの。
以下はエフェクト系エディタ改良のためにサブフォルダで追加したもの。

| フォルダ | 取得元 | コミット | ライセンス | 用途 |
|---|---|---|---|---|
| `implot/` | github.com/epezent/implot | 851cdbd (2026-10-10) | MIT | カーブ/統計グラフ（パーティクル数・寿命カーブのプレビュー、GPU時間） |
| `imgui_gradient/` | github.com/CoolLibs/imgui_gradient | 6fcb6b8 (2026-01-30) | Boost 1.0 | Color over Lifetime などのグラデーション編集（既存 ImGradient の上位互換） |
| `ImGuiFileDialog/` | github.com/aiekick/ImGuiFileDialog | d0e97b2 (2026-03-11) | MIT | テクスチャ / JSON のファイル選択ダイアログ |
| `ImGuiNotify/` | github.com/TyomaVader/ImGuiNotify | d00e45f (2026-03-17) | MIT | 保存・読込結果のトースト通知（ヘッダオンリー） |
| `ImGuiColorTextEdit/` | github.com/goossens/ImGuiColorTextEdit | bff6757 (2026-10-18) | MIT | HLSL / JSON のシンタックスハイライト付きテキスト編集 |
| `imgui_knobs/` | github.com/altschuler/imgui-knobs | 6b4b592 (2026-05-13) | MIT | ノブ型スライダー（強度・角度などのパラメータ） |

## 取り込みルール
- サブフォルダにはライブラリ本体のソースと LICENSE だけを置く。premake は `Externals/ImGui/**.cpp` を全てビルドするため、サンプルやテストを置くとビルド対象になる。
- 更新するときは上の表のコミットを書き換える。ローカル改変は下に残す。

## ローカル改変
- `ImGuiNotify/ImGuiNotify.hpp`: `IconsFontAwesome6.h` → `IconsFontAwesome5.h`。アイコン名を FA5 のものに置換（`// YoRigine:` コメント箇所）。
- `imconfig.h`: `IMGUI_DEFINE_MATH_OPERATORS` を有効化。imgui_gradient の公開ヘッダが内部で ImVec2/ImVec4 の演算子を使うため、エンジン側の TU でも必要になる。ImGui 公式の切り替えで、既存コードとの衝突がないことを確認済み。
- `imgui_gradient`: ソース改変なし。

## インクルード（YEngine 側）
```cpp
#include <implot/implot.h>
#include <imgui_gradient/imgui_gradient.hpp>      // Externals/imgui/imgui_gradient/include を includedirs に追加済み
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <ImGuiNotify/ImGuiNotify.hpp>
#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui_knobs/imgui-knobs.h>
```
すべて `#ifdef USE_IMGUI` の中で使うこと（Release では ImGui をリンクしない）。

## 初期化
- ImPlot のコンテキストは `ImGuiManager::Initialize / Finalize` で生成・破棄済み。
- ImGuiNotify は毎フレーム、全ウィンドウ描画の後に `ImGui::RenderNotifications();` を呼ぶ必要がある（未配線）。
- ImGuiNotify の通知リストはヘッダ内の `inline` 変数なので、モジュール（exe / DLL）ごとに別インスタンスになる。Debug で DLL になる YGame から直接 `InsertNotification` を呼ぶと表示されない可能性があるため、YEngine 側に薄いラッパーを置いて経由させること。
