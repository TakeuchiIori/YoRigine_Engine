#pragma once

/// <summary>
/// シェーダーソース(.hlsl/.hlsli)の置き場所を一元管理する。
///
/// シェーダーはエンジンの持ち物なので Engine/Shaders に置く（ゲーム側の
/// Resources には置かない）。 実行時に DXC
/// でコンパイルするため、パスは「実行ディレクトリ基準」で解決される。
///   Debug / Develop : debugdir = リポジトリルート → Engine/Shaders
///   をそのまま参照 Release         : ビルド後に出力先へ Engine/Shaders
///   をコピーする（Tools/premake5.lua の postbuild）
///
/// 文字列リテラルとの連結で使えるようマクロにしている（constexpr
/// では連結できないため）。
///   例) dxCommon->CompileShader(YENGINE_SHADER_DIR_W L"Sprite/Sprite.VS.hlsl",
///   L"vs_6_0");
///
/// 置き場所を変えるときはここだけを書き換えること。
/// ※ DXIL
/// ディスクキャッシュ(Resources/Binary/Shader/)は生成物なので別扱い（DirectXCommon.cpp
/// 参照）。
/// </summary>
#define YENGINE_SHADER_DIR                                                     \
  "Engine/Shaders/" // narrow 文字列用 (std::filesystem::path など)
#define YENGINE_SHADER_DIR_W                                                   \
  L"Engine/Shaders/" // wide 文字列用   (CompileShader など)
