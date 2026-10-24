#pragma once
#include "Modules/IParticleModule.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

/// <summary>
/// エディタでの分類（一覧の色タグと追加メニューの見出しに使う）。
/// 実行順を決める Spawn / Update とは別の「何をするモジュールか」の軸。
/// </summary>
enum class ParticleModuleCategory {
  Emission,  // 発生（寿命・バースト）
  Color,     // 色
  Transform, // サイズ・回転
  Motion,    // 動き（速度・軌道）
  Force,     // 力場・物理
  Look,      // 見た目（UV・フリップブック）
  Child,     // 子エフェクト
  Other,
  Count
};

/// <summary>
/// パーティクルモジュールのファクトリークラス
/// タイプ名からモジュールの実体を生成
/// </summary>
class YParticleModuleFactory {
public:
  using SpawnCreator = std::function<std::shared_ptr<ISpawnModule>()>;
  using UpdateCreator = std::function<std::shared_ptr<IUpdateModule>()>;

  //=================================================================
  // シングルトン
  //=================================================================

  static YParticleModuleFactory &GetInstance() {
    static YParticleModuleFactory instance;
    return instance;
  }

  YParticleModuleFactory(const YParticleModuleFactory &) = delete;
  YParticleModuleFactory &operator=(const YParticleModuleFactory &) = delete;

  //=================================================================
  // モジュール登録
  //=================================================================

  /// <summary>
  /// Spawnモジュールを登録
  /// </summary>
  /// <param name="typeName">タイプ名（例: "SpawnVelocity"）</param>
  /// <param name="creator">生成関数</param>
  void RegisterSpawnModule(const std::string &typeName, SpawnCreator creator) {
    spawnCreators_[typeName] = creator;
  }

  /// <summary>
  /// Updateモジュールを登録
  /// </summary>
  /// <param name="typeName">タイプ名（例: "UpdateGravity"）</param>
  /// <param name="creator">生成関数</param>
  void RegisterUpdateModule(const std::string &typeName,
                            UpdateCreator creator) {
    updateCreators_[typeName] = creator;
  }

  //=================================================================
  // モジュール生成
  //=================================================================

  /// <summary>
  /// タイプ名からSpawnモジュールを生成
  /// </summary>
  /// <param name="typeName">タイプ名</param>
  /// <returns>生成されたモジュール（存在しない場合nullptr）</returns>
  std::shared_ptr<ISpawnModule>
  CreateSpawnModule(const std::string &typeName) const {
    auto it = spawnCreators_.find(typeName);
    if (it != spawnCreators_.end()) {
      return it->second();
    }
    return nullptr;
  }

  /// <summary>
  /// タイプ名からUpdateモジュールを生成
  /// </summary>
  /// <param name="typeName">タイプ名</param>
  /// <returns>生成されたモジュール（存在しない場合nullptr）</returns>
  std::shared_ptr<IUpdateModule>
  CreateUpdateModule(const std::string &typeName) const {
    auto it = updateCreators_.find(typeName);
    if (it != updateCreators_.end()) {
      return it->second();
    }
    return nullptr;
  }

  //=================================================================
  // 登録済みモジュール一覧
  //=================================================================

  /// <summary>
  /// 登録されているSpawnモジュールのタイプ名一覧を取得
  /// </summary>
  std::vector<std::string> GetSpawnModuleTypes() const {
    std::vector<std::string> types;
    types.reserve(spawnCreators_.size());
    for (const auto &[type, _] : spawnCreators_) {
      types.push_back(type);
    }
    return types;
  }

  /// <summary>
  /// 登録されているUpdateモジュールのタイプ名一覧を取得
  /// </summary>
  std::vector<std::string> GetUpdateModuleTypes() const {
    std::vector<std::string> types;
    types.reserve(updateCreators_.size());
    for (const auto &[type, _] : updateCreators_) {
      types.push_back(type);
    }
    return types;
  }

  /// <summary>
  /// 全モジュールの登録を保証する（YParticleModuleRegistry.cpp
  /// を必ずリンクさせるための呼び出し）。
  /// 静的ライブラリでは参照されないモジュールの .obj
  /// が捨てられ、登録ごと消えてしまうため。
  /// </summary>
  static void EnsureAllModulesLinked();

  /// <summary>型名からカテゴリを返す（表は
  /// YParticleModuleRegistry.cpp。未登録は Other）</summary>
  static ParticleModuleCategory GetCategory(const std::string &typeName);

  /// <summary>カテゴリの表示名（日本語）</summary>
  static const char *GetCategoryName(ParticleModuleCategory category);

  /// <summary>一行説明（追加メニュー・インスペクタ用。未登録は空文字）</summary>
  static const char *GetDescription(const std::string &typeName);

  /// <summary>
  /// 旧モジュールなら推奨の置き換え先の型名（追加メニューに出さず、インスペクタで変換を勧める）。
  /// 置き換え先は旧モジュールの JSON をそのまま読めること。旧モジュールでなければ nullptr。
  /// </summary>
  static const char *GetReplacement(const std::string &typeName);

  /// <summary>速度だけを書き換えるモジュールか（位置への反映に UpdateVelocity が必要）</summary>
  static bool UsesVelocity(const std::string &typeName);

private:
  YParticleModuleFactory() = default;
  ~YParticleModuleFactory() = default;

  //=================================================================
  // メンバ変数
  //=================================================================

  std::unordered_map<std::string, SpawnCreator> spawnCreators_;
  std::unordered_map<std::string, UpdateCreator> updateCreators_;
};

//=================================================================
// モジュール登録用マクロ
//=================================================================

/// <summary>
/// Spawnモジュールを自動登録するマクロ
/// 使用例: REGISTER_SPAWN_MODULE(SpawnVelocity)
/// </summary>
#define REGISTER_SPAWN_MODULE(ModuleClass)                                     \
  namespace {                                                                  \
  struct ModuleClass##Registrar {                                              \
    ModuleClass##Registrar() {                                                 \
      YParticleModuleFactory::GetInstance().RegisterSpawnModule(               \
          #ModuleClass, []() { return std::make_shared<ModuleClass>(); });     \
    }                                                                          \
  };                                                                           \
  static ModuleClass##Registrar g_##ModuleClass##Registrar;                    \
  }

/// <summary>
/// Updateモジュールを自動登録するマクロ
/// 使用例: REGISTER_UPDATE_MODULE(UpdateGravity)
/// </summary>
#define REGISTER_UPDATE_MODULE(ModuleClass)                                    \
  namespace {                                                                  \
  struct ModuleClass##Registrar {                                              \
    ModuleClass##Registrar() {                                                 \
      YParticleModuleFactory::GetInstance().RegisterUpdateModule(              \
          #ModuleClass, []() { return std::make_shared<ModuleClass>(); });     \
    }                                                                          \
  };                                                                           \
  static ModuleClass##Registrar g_##ModuleClass##Registrar;                    \
  }