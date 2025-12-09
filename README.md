# デバッグ状態
[![DebugBuild](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Debug.yml/badge.svg)](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Debug.yml)

[![ReleaseBuild](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Release.yml/badge.svg)](https://github.com/TakeuchiIori/YoRigine/actions/workflows/Release.yml)

# ビルド方法
1. ダウンロードしたら **`premake.bat`** を起動します。
2. **`.sln`** (ソリューションファイル) が生成されるので、**Visual Studio** で開きます。
3. ビルドを開始します。

# ※ビルドできない場合
ダウンロードしたフォルダを含むディレクトリパスには、日本語や全角文字など、英数字と一部の記号（ASCII文字）以外の文字を使用しないでください。
【例】
  OK: C:\Users\username\Documents\project_name
  NG: C:\ユーザー\ドキュメント\プロジェクト名
Premakeなど一部のビルドツールが、非ASCII文字を含むパスを正しく処理できないため、実行に失敗します。
