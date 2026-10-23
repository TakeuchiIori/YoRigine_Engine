#include "ImGuiManager.h"
// Engine
#include "DirectXCommon.h"
#include "WinApp/WinApp.h"
#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_internal.h"
#include <imgui_impl_dx12.cpp>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <implot/implot.h>
#endif
#include <iostream>

ImGuiManager *ImGuiManager::GetInstance() {
  static ImGuiManager instance;
  return &instance;
}

void ImGuiManager::Initialize(
    [[maybe_unused]] WinApp *winApp,
    [[maybe_unused]] YoRigine::DirectXCommon *dxCommon) {
#ifdef USE_IMGUI
  dxCommon_ = dxCommon;
  winApp_ = winApp;

  // コンテキスト生成
  ImGui::CreateContext();
  // ImPlot は ImGui コンテキストの上に乗るため、ImGui の直後に生成する
  ImPlot::CreateContext();

  // フォント / IO 設定を先に（フォントビルド前に色設定は不要）
  CustomizeEditor();

  // エディタテーマ適用
  CustomizeColor();

  // DirectX12 バックエンド初期化
  InitialzeDX12();
#endif
}

void ImGuiManager::Begin() {
#ifdef USE_IMGUI
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
#endif
}

void ImGuiManager::End() {
#ifdef USE_IMGUI
  // 1.91+ では EndFrame() → Render() の順が推奨
  ImGui::EndFrame();
  ImGui::Render();
#endif
}

void ImGuiManager::Draw() {
#ifdef USE_IMGUI
  ID3D12DescriptorHeap *ppHeaps[] = {
      YoRigine::SrvManager::GetInstance()->GetDescriptorHeap()};
  dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

  ImDrawData *drawData = ImGui::GetDrawData();
  if (!drawData || drawData->TotalVtxCount == 0)
    return;

  ImGui_ImplDX12_RenderDrawData(drawData, dxCommon_->GetCommandList().Get());
#endif
}

// =============================================================================
//  DirectX12 バックエンド
// =============================================================================
void ImGuiManager::InitialzeDX12() {
#ifdef USE_IMGUI
  IMGUI_CHECKVERSION();
  ImGui_ImplWin32_Init(winApp_->GetHwnd());

  uint32_t srvIndex = YoRigine::SrvManager::GetInstance()->Allocate();
  HRESULT hr = ImGui_ImplDX12_Init(
      dxCommon_->GetDevice().Get(), dxCommon_->GetBackBufferCount(),
      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      YoRigine::SrvManager::GetInstance()->GetDescriptorHeap(),
      YoRigine::SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex),
      YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
  assert(SUCCEEDED(hr));
#endif
}

// =============================================================================
//  エディタテーマ ― Unreal 風の落ち着いたダークテーマ
//
//  コンセプト (AIGameEngine の EditorTheme と同じ考え方):
//    ・背景は真っ黒にしない。0.11 前後のグレーにして、パネル (0.16) との
//      明度差だけで「どこが入力欄か」が分かるようにする。
//    ・アクセントは青 1 色だけ。ホバー・選択・チェック・スライダーを
//      同じ色で揃えると、「押せる場所」が一目で拾える。
//    ・枠線は描かず、境界は黒の半透明で締める。線が多いと情報より線が目立つ。
//  色は GetAccentColor() など外部からも引けるので、パネル側で
//  直接 RGB を書かずにここを参照すること。
// =============================================================================
void ImGuiManager::CustomizeColor() {
#ifdef USE_IMGUI
  ImGui::StyleColorsDark();

  ImGuiStyle &s = ImGui::GetStyle();
  ImVec4 *c = s.Colors;

  // ── パレット ────────────────────────────────────────────────────────────
  const ImVec4 kBg = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);     // ウィンドウ背景
  const ImVec4 kBgDark = ImVec4(0.08f, 0.08f, 0.09f, 1.00f); // タイトル・タブ列
  const ImVec4 kBgBar = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);  // メニューバー
  const ImVec4 kPanel = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);  // 入力欄・ボタン
  const ImVec4 kPanelHi = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);   // 入力欄ホバー
  const ImVec4 kPanelAct = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);  // 入力欄操作中
  const ImVec4 kAccent = GetAccentColor();                      // 青
  const ImVec4 kAccentAct = ImVec4(0.16f, 0.38f, 0.74f, 1.00f); // 押し込み
  const ImVec4 kAccentDim = GetAccentColorDim();                // 薄い青
  const ImVec4 kText = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
  const ImVec4 kTextDim = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
  const ImVec4 kLine = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);

  // ── テキスト ────────────────────────────────────────────────────────────
  c[ImGuiCol_Text] = kText;
  c[ImGuiCol_TextDisabled] = kTextDim;
  c[ImGuiCol_TextSelectedBg] = kAccentDim;

  // ── ウィンドウ / 背景 ───────────────────────────────────────────────────
  c[ImGuiCol_WindowBg] = kBg;
  c[ImGuiCol_ChildBg] = kBg;
  c[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.15f, 0.98f);
  c[ImGuiCol_MenuBarBg] = kBgBar;
  c[ImGuiCol_TitleBg] = kBgDark;
  c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
  c[ImGuiCol_TitleBgCollapsed] = kBgDark;
  c[ImGuiCol_Border] = kLine;
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);

  // ── 入力欄 (InputText / Checkbox / Slider の地) ─────────────────────────
  c[ImGuiCol_FrameBg] = kPanel;
  c[ImGuiCol_FrameBgHovered] = kPanelHi;
  c[ImGuiCol_FrameBgActive] = kPanelAct;

  // ── ボタン ──────────────────────────────────────────────────────────────
  c[ImGuiCol_Button] = kPanel;
  c[ImGuiCol_ButtonHovered] = kAccent;
  c[ImGuiCol_ButtonActive] = kAccentAct;

  // ── ヘッダ (TreeNode / Selectable / CollapsingHeader) ───────────────────
  c[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
  c[ImGuiCol_HeaderHovered] = kAccent;
  c[ImGuiCol_HeaderActive] = kAccent;

  // ── インジケータ ────────────────────────────────────────────────────────
  c[ImGuiCol_CheckMark] = kAccent;
  c[ImGuiCol_SliderGrab] = kAccent;
  c[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.56f, 0.95f, 1.00f);
  c[ImGuiCol_PlotHistogram] = kAccent;
  c[ImGuiCol_PlotLines] = ImVec4(0.55f, 0.65f, 0.85f, 1.00f);

  // ── タブ / ドッキング ───────────────────────────────────────────────────
  c[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
  c[ImGuiCol_TabHovered] = kAccent;
  c[ImGuiCol_TabSelected] = ImVec4(0.18f, 0.30f, 0.52f, 1.00f);
  c[ImGuiCol_TabSelectedOverline] = kAccent;
  c[ImGuiCol_TabDimmed] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
  c[ImGuiCol_TabDimmedSelected] = ImVec4(0.14f, 0.18f, 0.28f, 1.00f);
  c[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_DockingPreview] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.55f);
  c[ImGuiCol_DockingEmptyBg] = kBgDark;

  // ── 区切り / リサイズ ───────────────────────────────────────────────────
  c[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
  c[ImGuiCol_SeparatorHovered] = kAccentDim;
  c[ImGuiCol_SeparatorActive] = kAccent;
  c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ResizeGripHovered] = kAccentDim;
  c[ImGuiCol_ResizeGripActive] = kAccent;

  // ── スクロールバー ──────────────────────────────────────────────────────
  c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.27f, 1.00f);
  c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
  c[ImGuiCol_ScrollbarGrabActive] = kAccent;

  // ── 表 ──────────────────────────────────────────────────────────────────
  c[ImGuiCol_TableHeaderBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
  c[ImGuiCol_TableBorderStrong] = kLine;
  c[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.00f, 0.00f, 0.25f);
  c[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.025f);

  c[ImGuiCol_NavCursor] = kAccent;
  c[ImGuiCol_DragDropTarget] = ImVec4(0.95f, 0.75f, 0.25f, 0.90f);

  // ── 形状 ────────────────────────────────────────────────────────────────
  // 角は小さく丸めるだけ。ウィンドウの角は丸めない (ドッキング時に隙間が出る)。
  s.WindowRounding = 0.0f;
  s.ChildRounding = 0.0f;
  s.FrameRounding = 3.0f;
  s.PopupRounding = 3.0f;
  s.GrabRounding = 3.0f;
  s.TabRounding = 3.0f;
  s.ScrollbarRounding = 3.0f;

  s.WindowBorderSize = 1.0f;
  s.FrameBorderSize = 0.0f; // 枠線ではなく明度差で表現する
  s.PopupBorderSize = 1.0f;
  s.TabBorderSize = 0.0f;

  s.WindowPadding = ImVec2(8.0f, 6.0f);
  s.FramePadding = ImVec2(6.0f, 4.0f);
  s.ItemSpacing = ImVec2(8.0f, 5.0f);
  s.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
  s.IndentSpacing = 16.0f;
  s.ScrollbarSize = 12.0f;
  s.GrabMinSize = 10.0f;
  s.WindowTitleAlign = ImVec2(0.0f, 0.5f); // 左寄せ
  s.SeparatorTextBorderSize = 2.0f;
  s.SeparatorTextPadding = ImVec2(12.0f, 3.0f);
#endif
}

// =============================================================================
//  エディタ設定（IO / フォント）
// =============================================================================
#ifdef USE_IMGUI
namespace {

// 等幅の欧文 + 日本語 + アイコンを 1 つの ImFont に合成する。
//
// 欧文に FiraMono (等幅)
// を使うのは、数値のパラメータが並ぶパネルで桁が揃うため。 FiraMono
// は日本語グリフを持たないので、IPAex ゴシックを後ろにマージして
// 欠けた文字だけを補う
// (マージは「先に登録したフォントに無い文字」だけが使われる)。
//
// ※ このプロジェクトの DX12 バックエンドは ImGui 1.92 の動的フォント
//   (ImGuiBackendFlags_RendererHasTextures)
//   に未対応。起動時にアトラスを焼き切る
//   旧方式なので、焼く文字範囲を明示し、最後に Fonts->Build()
//   を呼ぶ必要がある。
//   範囲を渡さないと既定の欧文だけが焼かれ、日本語が全部「?」になる。
const ImWchar *JapaneseGlyphRanges(ImGuiIO &io) {
  // ranges は Build() まで生きている必要があるので static で持つ
  static ImVector<ImWchar> ranges;
  if (ranges.empty()) {
    ImFontGlyphRangesBuilder b;
    b.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    // GetGlyphRangesJapanese() は記号類を含まないので、UI で使うものを足す
    b.AddText(reinterpret_cast<const char *>(
        u8"●○◆◇■□▶◀▲▼…※→←↑↓✓×±°—─「」『』【】・"));
    b.BuildRanges(&ranges);
  }
  return ranges.Data;
}

ImFont *AddEditorFont(ImGuiIO &io, float size, bool withIcons) {
  // ── 欧文 (等幅) ──
  ImFontConfig baseCfg;
  baseCfg.OversampleH = 2;
  ImFont *font = io.Fonts->AddFontFromFileTTF(
      "Resources/Fonts/FiraMono-Medium.ttf", size, &baseCfg);
  if (!font) {
    // 最低限は表示できる状態を保証する (日本語は下のマージで補う)
    ImFontConfig fallbackCfg;
    fallbackCfg.SizePixels = size;
    font = io.Fonts->AddFontDefault(&fallbackCfg);
  }

  // ── 日本語 ──
  // 和文は欧文より字面が小さく見えるので 1 割ほど大きくして高さを揃える
  ImFontConfig jpCfg;
  jpCfg.MergeMode = true;
  jpCfg.OversampleH = 2;
  jpCfg.GlyphOffset = ImVec2(0.0f, 1.0f);
  if (!io.Fonts->AddFontFromFileTTF("Resources/Fonts/ipaexg.ttf", size * 1.10f,
                                    &jpCfg, JapaneseGlyphRanges(io))) {
    std::cout << "[ImGuiManager] 日本語フォントが見つかりません。"
                 "日本語は '?' で表示されます\n";
  }

  // ── アイコン (Font Awesome 5) ──
  if (withIcons) {
    static const ImWchar kIconRanges[] = {0xf000, 0xf8ff, 0};
    ImFontConfig iconCfg;
    iconCfg.MergeMode = true;
    iconCfg.PixelSnapH = true;
    iconCfg.GlyphOffset = ImVec2(0.0f, 1.0f);
    iconCfg.GlyphMinAdvanceX =
        size; // アイコン幅を揃えてメニューの文字頭を合わせる
    io.Fonts->AddFontFromFileTTF("Resources/Fonts/Free-Solid-900.otf",
                                 size * 0.90f, &iconCfg, kIconRanges);
  }
  return font;
}

} // namespace
#endif

void ImGuiManager::CustomizeEditor() {
#ifdef USE_IMGUI
  ImGuiIO &io = ImGui::GetIO();

  // --------- IO フラグ -----------------------------------------------------
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // エディタ向け挙動
  io.ConfigDockingWithShift = false; // Shift なしでドック可
  io.ConfigDockingAlwaysTabBar = false;
  io.ConfigWindowsMoveFromTitleBarOnly = true; // タイトルバーのみドラッグ

  // ini 保存先
  io.IniFilename = "imgui_editor.ini";

  // --------- フォント ------------------------------------------------------
  // 先頭に登録したものが既定フォントになる
  fontNormal_ = AddEditorFont(io, kFontSizeNormal, true);
  fontLarge_ = AddEditorFont(io, kFontSizeLarge, true);
  fontSmall_ = AddEditorFont(io, kFontSizeSmall, false);

  // バックエンドが動的フォント未対応なので、ここでアトラスを焼き切る。
  // これを省くと ImGui_ImplDX12_NewFrame 後の最初のフレームで
  // 「font atlas is not built」の assert で落ちる。
  io.Fonts->Build();

#endif // USE_IMGUI
}
void ImGuiManager::Finalize() {
#ifdef USE_IMGUI
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  // 生成と逆順に破棄（ImPlot → ImGui）
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
#endif
}