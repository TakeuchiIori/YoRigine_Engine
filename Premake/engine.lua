-- =============================================================================
-- engine.lua （Engine リポジトリの Premake/ に配置）
--   Engine（YEngine / YMath / Externals）のプロジェクト定義とパスを Engine 側で管理する。
--   ゲーム側の premake5.lua は、これを include して YoRigine.* を呼ぶだけでよい。
--
--   使い方（ゲーム側 premake5.lua）:
--     include(root .. "/Engine/Premake/engine.lua")
--     YoRigine.init { root = root }
--     workspace "MyGame"
--         YoRigine.workspaceDefaults()
--     YoRigine.externals()
--     YoRigine.engine()
--     -- ここから先にゲーム側の project を書く
--
--   Engine のフォルダ構成やライブラリが変わっても、このファイルだけ直せば
--   全ゲームに反映される。
-- =============================================================================

YoRigine = YoRigine or {}
local Y = YoRigine

-- =============================================================================
-- 初期化・パス
-- =============================================================================

-- opt.root      : ゲームリポジトリのルート（絶対パス）。必須。
-- opt.engineDir : Engine のルート。省略時は root/Engine
function Y.init(opt)
    Y.root      = opt.root
    Y.engineDir = opt.engineDir or (opt.root .. "/Engine")

    -- 出力／中間ディレクトリはゲームリポジトリの 1 つ上の generated/ に集約する
    local parentDir = path.getabsolute(Y.root .. "/..")
    Y.outputDir = parentDir .. "/generated/outputs/%{cfg.buildcfg}"
    Y.intDir    = parentDir .. "/generated/intermediates/%{prj.name}/%{cfg.buildcfg}"

    Y.engine_includes = {
        Y.e"YEngine",
        Y.e"YEngine/Core",
        Y.e"YEngine/Core/DirectX",
        Y.e"YEngine/Generators",
        Y.e"YEngine/Graphics",
        Y.e"YEngine/Systems",
        Y.e"YEngine/Utilities",
        Y.e"YEngine/Model",
        Y.e"YMath",
        Y.e"Externals/nlohmann",
        Y.e"Externals/DirectXTex",
        Y.e"Externals/imgui",
        Y.e"Externals/assimp/include",
        Y.e"Externals/icon",
        Y.e"Externals/meshoptimizer/src",
        Y.e"Externals/DirectXMesh/DirectXMesh"
    }
end

-- Engine 配下の絶対パス（/ 区切り）
function Y.e(p)  return Y.engineDir .. "/" .. p end
-- 同上（xcopy 用に \ 区切り）
function Y.ew(p) return path.translate(Y.engineDir .. "/" .. p, "\\") end

Y.directx_libs = {
    "d3d12", "dxgi", "dxguid", "dxcompiler", "dinput8", "xinput"
}

-- =============================================================================
-- ワークスペース共通設定（workspace "名前" の直後に呼ぶ）
-- =============================================================================
function Y.workspaceDefaults()
    architecture "x64"
    configurations { "Debug", "Develop", "Release" }
    platforms { "x64" }

    language "C++"
    cppdialect "C++20"
    staticruntime "On"
    warnings "Extra"
    flags { "MultiProcessorCompile" }

    -- PlatformToolset (v145 = VS2026 のツールセット)
    toolset "v145"

    -- /FS: /MP(MultiProcessorCompile) で複数 cl.exe が同じ vc143.pdb へ書く際の
    --      書き込み競合(C1041)を防ぐ。並列ビルドや同時ビルドでも安全になる。
    buildoptions { "/utf-8", "/permissive-", "/FS" }
    defines { "NOMINMAX", "_WINDOWS" }

    targetdir (Y.outputDir)
    objdir    (Y.intDir)

    -- Debug と Develop は同じデバッグ設定 (symbols / _DEBUG)
    filter "configurations:Debug or Develop"
        defines { "_DEBUG" }
        symbols "On"
        -- Edit&Continue(/ZI) を無効化し /Zi にする。コンパイル/リンクが軽くなる。
        -- (デバッグ実行中のコード書き換え機能は使わない前提)
        editandcontinue "Off"

    -- Develop: Debug と同等のエディタ構成 + 起動シーンを DevelopScene にする。
    -- エンジン機能 (パーティクル/当たり判定/VFX) のテスト専用。
    -- Player を生成しないのでゲーム側のセーブは一切走らない。
    filter "configurations:Develop"
        defines { "DEVELOP_BUILD" }

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter {}
end

-- =============================================================================
-- グループ: Externals (外部ライブラリ)
-- =============================================================================
function Y.externals()
    group "Externals"

    --------------------- ImGui ---------------------
    project "ImGui"
        kind "StaticLib"
        language "C++"
        location (Y.e"Externals/ImGui")
        warnings "Default"

        -- project location が Externals/ImGui でも、生成物は他プロジェクトと同じ
        -- generated 以下へ集約する。ここで明示的に再指定して、
        -- 将来 workspace 設定を変更しても Externals/generated へ戻らないようにする。
        targetdir (Y.outputDir)
        objdir    (Y.intDir)

        files { Y.e"Externals/ImGui/**.h", Y.e"Externals/ImGui/**.cpp" }

        includedirs {
            Y.e"Externals/ImGui",
            Y.e"Externals/DirectXTex"
        }

    -- DirectXTex は毎回コンパイルすると遅く(21ファイル+fxcシェーダ生成)、かつ Rebuild 時に
    -- コミット済みシェーダ生成物(Shaders/Compiled/*.inc)を破壊するため、assimp/curl と同様に
    -- 「事前ビルド版 .lib を直接リンク」する方式に変更した。ビルドグラフからは外してある。
    --   .lib の場所: Externals/DirectXTex/lib/{Debug,Release}/DirectXTex.lib
    --   ★DirectXTex を更新したとき（滅多に無い）は、元 vcxproj
    --     (Externals/DirectXTex/DirectXTex_Desktop_2022_Win10.vcxproj) を Debug/Release で
    --     ビルドし、生成された DirectXTex.lib を上記フォルダへコピーして差し替えること。
    -- （DirectXMesh はシェーダ生成の罠が無く Rebuild を壊さないため従来どおりソースビルド）

    --------------------- DirectXMesh (既存のvcxprojを参照) ---------------------
    externalproject "DirectXMesh"
        location (Y.e"Externals/DirectXMesh/DirectXMesh")
        filename "DirectXMesh_Desktop_2022_Win10"
        kind "StaticLib"
        language "C++"
        toolset "v145"
        configmap { ["Develop"] = "Debug" }

    --------------------- meshoptimizer ---------------------
    project "meshoptimizer"
        kind "StaticLib"
        language "C++"
        location (Y.e"Externals/meshoptimizer")
        warnings "Default"

        files {
            Y.e"Externals/meshoptimizer/src/meshoptimizer.h",
            Y.e"Externals/meshoptimizer/src/**.cpp"
        }

        includedirs { Y.e"Externals/meshoptimizer/src" }
end

-- =============================================================================
-- グループ: Engine (エンジン・コア)
-- =============================================================================
function Y.engine()
    group "Engine"

    --------------------- YMath (Static Library) ---------------------
    project "YMath"
        kind "StaticLib"
        language "C++"
        cppdialect "C++20"
        staticruntime "On"
        location (Y.e"YMath")

        files {
            Y.e"YMath/**.h",
            Y.e"YMath/**.cpp"
        }

        includedirs {
            Y.e"YMath"
        }

        vpaths {
            ["*"] = Y.e"YMath/**"
        }

    --------------------- YEngine (Static Library) ---------------------
    project "YEngine"
        kind "StaticLib"
        location (Y.e"YEngine")
        -- ※ GAME_BUILD_DLL は不要なら削除してください
        defines { "GAME_BUILD_DLL" }

        fatalwarnings { "All" }
        linkoptions { "/ignore:4099" }

        -- プリコンパイルヘッダ (コンパイル時間短縮)。
        -- forceincludes で全 .cpp の先頭に pch.h を自動挿入するため、既存ソースは無改修。
        pchheader "pch.h"
        pchsource (Y.e"YEngine/pch.cpp")
        forceincludes { "pch.h" }

        files {
            Y.e"YEngine/**.h",
            Y.e"YEngine/**.cpp",
        }

        vpaths {
            ["YEngine/*"] = Y.e"YEngine/**",
        }

        -- インクルードパス（ヘッダのみ。cURL を含む）
        includedirs {
            Y.engine_includes,
            Y.e"Externals/curl/include"
        }

        -- YEngine は静的ライブラリ。外部 lib を links するとその obj が
        -- YEngine.lib に丸ごとマージされ、最終リンクで LNK4006(重複)になる。
        -- よってここでは link せず、build 順序のための dependson のみ残す。
        -- 実際のリンクは最終バイナリ(Debug/Develop=YGame.dll / Release=YMain.exe)で行う。
        -- DirectXTex は事前ビルド .lib 直リンクに変更したため dependson から外す。
        dependson { "YMath", "DirectXMesh", "meshoptimizer" }

        postbuildcommands {
            -- DXC/DXIL DLLのコピー
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}"',
            'xcopy /Q /Y /I "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}"'
        }

        -- USE_IMGUI は YEngine のコンパイルに必要（#ifdef 分岐）。ImGui は
        -- ヘッダ参照のみ。lib リンクは最終バイナリ側。dependson は build 順序用。
        filter "configurations:Debug or Develop"
            defines { "USE_IMGUI" }
            dependson { "ImGui" }

        filter "configurations:Release"
            undefines { "USE_IMGUI" }

        filter {}
end

-- =============================================================================
-- ゲーム側の最終バイナリ用ヘルパー（filter の中で呼ぶ）
--   「何をリンクするか」は Engine が知っているので Engine 側に置く。
-- =============================================================================

-- Debug/Develop 用: YGame(DLL) が YEngine の参照する全ライブラリをまとめてリンクする
function Y.linkDebug()
    libdirs {
        Y.outputDir,
        Y.e"Externals/curl/lib",
        Y.e"Externals/assimp/lib/Debug",
        Y.e"Externals/DirectXTex/lib/Debug"  -- 事前ビルド版 DirectXTex.lib
    }
    links {
        "YMath", "YEngine", "meshoptimizer", "ImGui",
        "DirectXTex.lib", "DirectXMesh.lib", "libcurl", "assimp-vc143-mtd"
    }
    links(Y.directx_libs)
end

-- Release 用: 全プロジェクトが static。最終バイナリ(EXE)で全部リンクする。
-- extraLinks にはゲーム側のプロジェクト名（例 { "YGame" }）を渡す。
function Y.linkRelease(extraLinks)
    libdirs {
        Y.outputDir,
        Y.e"Externals/curl/lib",
        Y.e"Externals/assimp/lib/Release",
        Y.e"Externals/DirectXTex/lib/Release"  -- 事前ビルド版 DirectXTex.lib
    }
    links(extraLinks or {})
    links {
        "YEngine", "YMath", "meshoptimizer",
        "DirectXTex.lib", "DirectXMesh.lib", "libcurl", "assimp-vc143-mt"
    }
    links(Y.directx_libs)
end

-- 実行に必要な Engine 同梱 DLL(libcurl)を出力先へコピーする（project の中で呼ぶ）
function Y.copyRuntimeDlls()
    postbuildcommands {
        'xcopy /Q /Y /I "' .. Y.ew("Externals/curl/bin/libcurl.dll") .. '" "%{cfg.targetdir}"'
    }
end
