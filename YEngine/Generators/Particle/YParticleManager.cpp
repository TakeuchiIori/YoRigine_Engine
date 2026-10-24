#include "YParticleManager.h"
#include "DirectXCommon.h"
#include "Graphics/LightManager/LightManager.h"
#include "YEmitterGroupManager.h"
#include "YParticleEmitter.h"
#include "YParticleModuleFactory.h"
#include <PipelineManager/YPipelineManager.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

//=================================================================
// 初期化・終了
//=================================================================

void YParticleManager::Initialize(YoRigine::SrvManager *srvManager,
                                  uint32_t maxTotalParticles) {
  if (initialized_)
    return;

  srvManager_ = srvManager;

  // 全モジュールを Factory に登録させる（これが無いと JSON
  // のモジュールが一部黙って欠落する）
  YParticleModuleFactory::EnsureAllModulesLinked();

  // レンダラーの初期化
  renderer_ = std::make_unique<YParticleRenderer>();
  renderer_->Initialize(srvManager, maxTotalParticles);

  // パイプライン取得
  auto pm = YPipelineManager::GetInstance();
  rootSignature_ = pm->GetRootSignature("YParticle");
  pipelineState_ = pm->GetPipeLineStateObject("YParticle");

  initialized_ = true;
}

void YParticleManager::Finalize() {
  if (!initialized_)
    return;

  // すべてのシステムを削除
  systems_.clear();

  // レンダラーの解放
  renderer_.reset();

  initialized_ = false;
}

//=================================================================
// システム管理
//=================================================================

YParticleSystem *YParticleManager::CreateSystem(const std::string &name,
                                                uint32_t maxParticles) {
  // すでに存在する場合は既存のものを返す
  if (systems_.find(name) != systems_.end()) {
    return systems_[name].get();
  }

  // 新しいシステムを作成
  auto system = std::make_unique<YParticleSystem>(name, maxParticles);
  YParticleSystem *ptr = system.get();

  // 既定のメッシュ（板ポリ）を作っておく。メッシュが無いシステムは描画で
  // 飛ばされるため、以前は「作った直後に発生させても何も見えない」状態だった。
  // JSON から読む場合は LoadFromJson が "mesh" の内容で必要に応じて作り直す。
  ptr->SetMeshTypeAndParams(ptr->GetCurrentMeshType(), ptr->GetMeshParams());
  systems_[name] = std::move(system);

  return ptr;
}

YParticleSystem *YParticleManager::GetSystem(const std::string &name) {
  auto it = systems_.find(name);
  if (it != systems_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void YParticleManager::RemoveSystem(const std::string &name) {
  systems_.erase(name);
}

bool YParticleManager::RenameSystem(const std::string &oldName,
                                    const std::string &newName) {
  // 新しい名前が既に存在する場合は失敗
  if (systems_.count(newName) > 0) {
    return false;
  }

  auto it = systems_.find(oldName);
  if (it == systems_.end()) {
    return false;
  }

  // システムを取り出して名前を変更
  auto system = std::move(it->second);
  systems_.erase(it);

  // 新しい名前で再登録（内部の name_ は YParticleSystem 側で変更する必要あり）
  systems_[newName] = std::move(system);

  return true;
}

std::vector<std::string> YParticleManager::GetAllSystemNames() const {
  std::vector<std::string> names;
  names.reserve(systems_.size());

  for (const auto &[name, system] : systems_) {
    names.push_back(name);
  }

  return names;
}

#include <fstream>
#include <set>

bool YParticleManager::LoadSystemsFromFile(const std::string &filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open())
      return false;

    nlohmann::json json;
    file >> json;
    // 読み込み元を記録しておく（エディタの Ctrl+S でここへ上書き保存する）
    if (auto *sys = LoadSystemsFromJson(json))
      sys->SetSourceFilePath(filePath);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

size_t YParticleManager::ScanDirectory(const std::string &dir) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(dir, ec))
    return 0;

  size_t loaded = 0;
  for (const auto &entry : fs::recursive_directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (entry.path().extension() != ".json")
      continue;
    if (LoadSystemsFromFile(entry.path().string()))
      ++loaded;
  }
  return loaded;
}

size_t YParticleManager::ScanEffectBundles(const std::string &dir) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(dir, ec))
    return 0;

  size_t loaded = 0;
  for (const auto &entry : fs::recursive_directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (entry.path().extension() != ".json")
      continue;
    if (LoadEffectBundle(entry.path().string()))
      ++loaded;
  }
  return loaded;
}

YParticleSystem *
YParticleManager::LoadSystemsFromJson(const nlohmann::json &json) {
  if (!json.contains("name"))
    return nullptr;

  std::string name = json["name"];
  uint32_t maxP = json.value("maxParticles", 1000);

  // 同名システムがあれば既存を返す。中身は LoadFromJson
  // がモジュールごと作り直す （＝旧フォルダと YEffects
  // の両方に同名システムがあっても二重追加にならない）。
  auto *system = CreateSystem(name, maxP);
  if (!system)
    return nullptr;

  system->LoadFromJson(json);
  return system;
}

bool YParticleManager::SaveSystemsToFile(const std::string &filePath) const {
  try {
    nlohmann::json json;
    json["version"] = "1.0";
    json["systems"] = nlohmann::json::array();

    for (const auto &[name, system] : systems_) {
      nlohmann::json sysJson;
      system->SaveToJson(sysJson);
      // ... その他の設定
      json["systems"].push_back(sysJson);
    }

    std::ofstream file(filePath);
    if (!file.is_open())
      return false;
    file << json.dump(4);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

//=================================================================
// 更新
//=================================================================

void YParticleManager::Update(float deltaTime) {
  if (!initialized_)
    return;

  auto startTime = std::chrono::high_resolution_clock::now();

  // 統計情報のリセット
  stats_.totalSystems = systems_.size();
  stats_.activeSystems = 0;
  stats_.totalActiveParticles = 0;

  // すべてのシステムを更新
  for (auto &[name, system] : systems_) {
    system->Update(deltaTime);
    // アクティブなパーティクル数をカウント
    size_t activeCount = 0;
    const auto &attributes = system->GetAttributes();
    for (const auto &attr : attributes) {
      if (attr.isActive) {
        activeCount++;
      }
    }

    if (activeCount > 0) {
      stats_.activeSystems++;
      stats_.totalActiveParticles += activeCount;
    }
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime);
  stats_.updateTimeMs = duration.count() / 1000.0f;

  // EffectHandle::Play(loop) で登録された実行中エミッタを
  // tick（継続発生・追従）。 Stop()
  // 済み（非アクティブ）になったものはここで除去する。
  for (auto &e : activeEmitters_) {
    if (e)
      e->Update(deltaTime);
  }
  activeEmitters_.erase(
      std::remove_if(activeEmitters_.begin(), activeEmitters_.end(),
                     [](const std::shared_ptr<YParticleEmitter> &e) {
                       return !e || !e->IsActive();
                     }),
      activeEmitters_.end());

  YEmitterGroupManager::GetInstance().Update(deltaTime);
}

void YParticleManager::StopAndClearActiveEmitters() {
  for (auto &e : activeEmitters_) {
    if (e)
      e->SetActive(false);
  }
  activeEmitters_.clear();
}

//=================================================================
// 描画　（「描画設定が同じシステムをグループ化して、後でまとめて描画するための仕分け処理）
//=================================================================
void YParticleManager::CreateRenderBatches(std::vector<RenderBatch> &batches) {
  batches.clear();

  for (auto &[name, system] : systems_) {
    auto mesh = system->GetMesh();
    uint32_t texIndex = system->GetTextureIndex();

    // メッシュが未設定の場合はスキップ
    if (!mesh)
      continue;

    // アクティブなパーティクルがあるかチェック
    bool hasActiveParticles = false;
    const auto &attributes = system->GetAttributes();
    for (const auto &attr : attributes) {
      if (attr.isActive) {
        hasActiveParticles = true;
        break;
      }
    }

    if (!hasActiveParticles)
      continue;

    // 既存のバッチを検索（同じメッシュ＋テクスチャ＋ライト＋ブレンド＋ソフト設定）
    bool foundBatch = false;
    for (auto &batch : batches) {
      if (batch.mesh == mesh && batch.textureIndex == texIndex &&
          batch.lightSetting == system->GetLightSetting() &&
          batch.blendMode == system->GetBlendMode() &&
          batch.softParticle == system->IsSoftParticle() &&
          batch.softFadeDistance == system->GetSoftFadeDistance()) {
        batch.systems.push_back(system.get());
        foundBatch = true;
        break;
      }
    }

    // 新しいバッチを作成
    if (!foundBatch) {
      RenderBatch newBatch;
      newBatch.mesh = mesh;
      newBatch.textureIndex = texIndex;
      newBatch.lightSetting = system->GetLightSetting();
      newBatch.blendMode = system->GetBlendMode();
      newBatch.softParticle = system->IsSoftParticle();
      newBatch.softFadeDistance = system->GetSoftFadeDistance();
      newBatch.systems.push_back(system.get());
      batches.push_back(newBatch);
    }
  }
}

void YParticleManager::Draw() {
  if (!initialized_ || !renderer_ || !camera_)
    return;

  std::vector<RenderBatch> batches;
  CreateRenderBatches(batches);

  auto commandList = YoRigine::DirectXCommon::GetInstance()->GetCommandList();
  commandList->SetGraphicsRootSignature(rootSignature_.Get());

  // ソフトバッチが1つでもあるか
  bool anySoft = false;
  for (const auto &batch : batches) {
    if (batch.softParticle) {
      anySoft = true;
      break;
    }
  }

  renderer_->BeginFrame();

  // --- フェーズ1: 非ソフトバッチ（従来通りハードウェア深度テストで描画） ---
  for (const auto &batch : batches) {
    if (batch.softParticle)
      continue;
    renderer_->ApplyLightSetting(batch.lightSetting);
    renderer_->ApplySoftParticle(false, 0.0f);
    for (auto *system : batch.systems) {
      renderer_->AddSystem(*system, camera_);
    }
    renderer_->EndFrame(batch.mesh, batch.textureIndex, batch.blendMode);
  }

  // --- フェーズ2: ソフトバッチ（深度を読める状態にして PS で接地フェード） ---
  if (anySoft) {
    auto *dx = YoRigine::DirectXCommon::GetInstance();
    dx->BeginParticleSoftDepth(); // MainDepth を SRV 読み取り状態へ＋深度なしで
                                  // RT 再バインド
    for (const auto &batch : batches) {
      if (!batch.softParticle)
        continue;
      renderer_->ApplyLightSetting(batch.lightSetting);
      renderer_->ApplySoftParticle(true, batch.softFadeDistance);
      for (auto *system : batch.systems) {
        renderer_->AddSystem(*system, camera_);
      }
      renderer_->EndFrame(batch.mesh, batch.textureIndex, batch.blendMode);
    }
    dx->EndParticleSoftDepth(); // MainDepth を DEPTH_WRITE に戻して再バインド
  }
}
//=================================================================
// 便利メソッド
//=================================================================

void YParticleManager::Emit(const std::string &systemName,
                            const Vector3 &position, int count) {
  auto *system = GetSystem(systemName);
  if (system) {
    system->Emit(position, count);
  }
}

void YParticleManager::EmitBurst(const std::string &systemName,
                                 const Vector3 &position, int count) {
  auto *system = GetSystem(systemName);
  if (system) {
    system->Emit(position, count);
  }
}

void YParticleManager::RegisterEmitter(
    const std::shared_ptr<YParticleEmitter> &emitter) {
  if (emitter) {
    activeEmitters_.push_back(emitter);
  }
}

//=================================================================
// バンドルロード・セーブ
//=================================================================

bool YParticleManager::LoadEffectBundle(const std::string &filePath) {
  try {
    std::ifstream file(filePath);
    if (!file.is_open())
      return false;
    nlohmann::json j;
    file >> j;

    if (j.contains("systems")) {
      for (const auto &sysJson : j["systems"]) {
        if (auto *sys = LoadSystemsFromJson(sysJson))
          sys->SetSourceFilePath(filePath);
      }
    }
    if (j.contains("groups")) {
      YEmitterGroupManager::GetInstance().LoadAllFromJson(j);
    }
    return true;
  } catch (...) {
    return false;
  }
}

bool YParticleManager::SaveEffectBundle(const std::string &groupName,
                                        const std::string &filePath) {
  auto &groupMgr = YEmitterGroupManager::GetInstance();
  auto *group = groupMgr.GetGroup(groupName);
  if (!group)
    return false;

  try {
    nlohmann::json j;
    j["systems"] = nlohmann::json::array();
    j["groups"] = nlohmann::json::array();

    // グループが参照するシステム名を収集
    std::set<std::string> systemNames;
    for (size_t i = 0; i < group->GetEmitterCount(); ++i) {
      auto *emitter = group->GetEmitter(i);
      if (emitter)
        systemNames.insert(emitter->GetSystemName());
    }

    // 参照システムをシリアライズ
    for (const auto &sysName : systemNames) {
      auto *system = GetSystem(sysName);
      if (!system)
        continue;

      nlohmann::json sysJson;
      // 全フィールドを YParticleSystem::SaveToJson
      // に一本化（以前はソフトパーティクル/発光が抜けていた）
      system->SaveToJson(sysJson);

      j["systems"].push_back(sysJson);
    }

    j["groups"].push_back(group->SaveToJson());

    std::ofstream file(filePath);
    if (!file.is_open())
      return false;
    file << j.dump(4);
    return true;
  } catch (...) {
    return false;
  }
}