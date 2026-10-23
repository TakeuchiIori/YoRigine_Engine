#include "MotionFiles.h"

#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <stdexcept>

namespace fs = std::filesystem;

namespace MotionFiles {

Motion LoadAnim(const std::string &path) { return Motion().LoadBinary(path); }

void SaveAnim(const Motion &motion, const std::string &animationName,
              const std::string &path) {
  // SaveBinary は引数の motion
  // だけを使うので、呼び出し用の空インスタンスで足りる
  Motion().SaveBinary(motion, animationName, path);
}

std::vector<std::string> ListGltfAnimationNames(const std::string &path) {
  std::string readPath = path;
  if (!fs::exists(readPath)) {
    readPath = ToModelFilePath(ToObjectModelPath(path));
  }

  std::ifstream file(readPath);
  if (!file.is_open()) {
    throw std::runtime_error("ファイルを開けません: " + readPath);
  }

  nlohmann::json gltfJson;
  file >> gltfJson;
  if (!gltfJson.contains("animations") || gltfJson["animations"].empty()) {
    throw std::runtime_error("アニメーションが見つかりません: " + readPath);
  }

  const auto &animations = gltfJson["animations"];
  std::vector<std::string> names;
  names.reserve(animations.size());
  for (size_t i = 0; i < animations.size(); ++i) {
    std::string name = animations[i].value("name", "");
    if (name.empty()) {
      name = "Animation" + std::to_string(i);
    }
    names.push_back(std::move(name));
  }
  return names;
}

std::string DisplayNameFromKey(const std::string &key) {
  const auto pos = key.find('#');
  return (pos != std::string::npos) ? key.substr(pos + 1) : key;
}

std::string ToObjectModelPath(const std::string &fullPath) {
  fs::path path(fullPath);
  fs::path rel = path;

  if (path.is_absolute()) {
    std::error_code ec;
    rel = fs::relative(path, fs::absolute(kModelDirectory, ec), ec);
    if (ec) {
      rel = path.filename();
    }
  } else {
    // "Resources/Models/" の接頭辞を取り除く
    rel = path.lexically_normal();
    auto it = rel.begin();
    if (it != rel.end() && it->generic_string() == "Resources") {
      ++it;
      if (it != rel.end() && it->generic_string() == "Models") {
        fs::path stripped;
        for (++it; it != rel.end(); ++it) {
          stripped /= *it;
        }
        rel = stripped;
      }
    }
  }

  // "Player/Player.gltf" のように、フォルダ名とファイル名が同じなら畳む
  if (rel.has_parent_path() && rel.parent_path().filename() == rel.stem()) {
    rel = rel.parent_path().parent_path() / rel.filename();
  }

  return rel.generic_string();
}

std::string ToModelFilePath(const std::string &objectPath) {
  fs::path objectRel(objectPath);
  fs::path base = objectRel;
  base.replace_extension("");
  return (fs::path(kModelDirectory) / base / objectRel.filename())
      .generic_string();
}

std::string RuntimeAnimPath(const std::string &objectPath,
                            const std::string &animationName) {
  const std::string stem = fs::path(objectPath).stem().string();
  return (fs::path(kAnimDirectory) / (stem + "_" + animationName + ".anim"))
      .generic_string();
}

} // namespace MotionFiles
