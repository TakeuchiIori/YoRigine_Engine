#pragma once

#include "../Core/Motion.h"

#include <string>
#include <vector>

// ============================================================
// モーションのファイル入出力とパス規約
//
// ・ImGui / エディタ状態に依存しない
// ・失敗は std::runtime_error で通知する（メッセージはそのまま UI に出せる）
// ============================================================
namespace MotionFiles {

// ランタイムが .anim を探しに行くディレクトリ（Model::binPath と同じ）
inline constexpr const char *kAnimDirectory = "Resources/Binary";
// モデル（glTF）の置き場
inline constexpr const char *kModelDirectory = "Resources/Models";

// .anim を読み込む
Motion LoadAnim(const std::string &path);

// .anim として保存する（ディレクトリが無ければ作成する）
void SaveAnim(const Motion &motion, const std::string &animationName,
              const std::string &path);

// glTF に含まれるアニメーション名の一覧を返す。
// 名前が空のアニメーションは "Animation<番号>" とする。
// path が存在しない場合は Object パスとして解釈し直して探す。
std::vector<std::string> ListGltfAnimationNames(const std::string &path);

// キャッシュキー（"<path>#<anim>" や
// "Binary:<path>"）から表示用の名前を取り出す
std::string DisplayNameFromKey(const std::string &key);

// フルパス / Resources/Models 相対パスを、Object3d が受け取る
// モデルパス（"Player/Player.gltf" → "Player.gltf" など）に変換する
std::string ToObjectModelPath(const std::string &fullPath);

// Object
// 用モデルパスから、実ファイルのパス（Resources/Models/<stem>/<file>）に戻す
std::string ToModelFilePath(const std::string &objectPath);

// ランタイムが自動で読む .anim のパス（Resources/Binary/<stem>_<anim>.anim）
std::string RuntimeAnimPath(const std::string &objectPath,
                            const std::string &animationName);

} // namespace MotionFiles
