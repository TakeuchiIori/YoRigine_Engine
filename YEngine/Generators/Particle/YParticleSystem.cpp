#include "YParticleSystem.h"

#include <Loaders/Texture/TextureManager.h>

#include "YParticleModuleFactory.h"
#include "Debugger/Logger.h"
#include <algorithm>
#include <cctype>
#include <cfloat>

#ifdef USE_IMGUI
#include "imgui.h"
#include <IconsFontAwesome5.h>
#endif


YParticleSystem::YParticleSystem(const std::string& name, uint32_t maxParticles)
	: name_(name), maxParticles_(maxParticles)
{
#ifdef USE_IMGUI
	textureBrowser_ = YoRigine::FileBrowser(textureDirectory_, { ".png", ".jpg", ".dds" }, YoRigine::FileBrowser::DisplayMode::Grid);
	textureBrowser_.SetThumbnailProvider([](const std::string& path) -> ImTextureID {
		TextureManager::GetInstance()->LoadTexture(path);
		auto handle = TextureManager::GetInstance()->GetsrvHandleGPU(path);
		return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
		});
	textureBrowser_.SetOnFileSelected([this](const std::string& path) {
		SetTexture(path);
		showTextureBrowser_ = false;
		});
#endif
	// 全パーティクル属性配列の初期化
	attributes_.resize(maxParticles_);

}

void YParticleSystem::AddSpawnModule(std::shared_ptr<ISpawnModule> module)
{
	if (module) {
		module->Initialize(maxParticles_);
		spawnModules_.push_back(module);
	}
}

void YParticleSystem::AddUpdateModule(std::shared_ptr<IUpdateModule> module)
{
	if (module) {
		module->Initialize(maxParticles_);
		updateModules_.push_back(module);
	}
}

void YParticleSystem::DeleteSpawnModule(const std::shared_ptr<ISpawnModule>& module) {
	spawnModules_.erase(
		std::remove(spawnModules_.begin(), spawnModules_.end(), module),
		spawnModules_.end()
	);
}

void YParticleSystem::DeleteUpdateModule(const std::shared_ptr<IUpdateModule>& module) {
	updateModules_.erase(
		std::remove(updateModules_.begin(), updateModules_.end(), module),
		updateModules_.end()
	);
}
void YParticleSystem::Update(float deltaTime)
{
	// 有効なモジュールだけを集め、フレーム単位の処理（共有時間など）を 1 回ずつ進める
	activeUpdateModules_.clear();
	for (auto& module : updateModules_) {
		if (!module->IsEnabled()) continue;
		module->OnFrameBegin(deltaTime);
		activeUpdateModules_.push_back(module.get());
	}

	for (uint32_t i = 0; i < maxParticles_; ++i) {
		auto& attr = attributes_[i];
		if (!attr.isActive) continue;

		// 個別の経過時間を進める
		attr.currentTime += deltaTime;

		// 不滅粒は寿命で死なず、モジュール更新だけ受け続ける（Persistent 用）
		if (attr.immortal) {
			for (auto* module : activeUpdateModules_) {
				module->OnUpdate(attributes_.data(), i, deltaTime);
			}
			continue;
		}

		// 寿命チェック（死亡の瞬間に OnDeath フックを発火 → サブエミッタ等）
		if (attr.currentTime >= attr.lifeTime) {
			for (auto* module : activeUpdateModules_) {
				module->OnDeath(attributes_.data(), i);
			}
			attr.isActive = false;
			continue;
		}

		// モジュールの更新
		for (auto* module : activeUpdateModules_) {
			module->OnUpdate(attributes_.data(), i, deltaTime);
		}
	}
}

void YParticleSystem::Emit(const Vector3& position, int count, bool prewarmAge, bool immortal)
{
	for (int i = 0; i < count; ++i) {
		int index = FindEmptyIndex();
		if (index < 0) break;

		// ── 全フィールドをデフォルト値にリセット ──────────────────────
		// ※ lifeTime も必ずリセット（前回使用値が残るバグ防止）
		auto& a = attributes_[index];
		a.isActive       = true;
		a.immortal       = immortal; // 不滅（寿命で死なない）
		a.position       = position;
		a.currentTime    = 0.0f;
		a.lifeTime       = 1.0f;  // SpawnLifeTime モジュールが上書きする
		a.velocity       = {};
		a.scale          = { 1.0f, 1.0f, 1.0f };
		a.rotation       = {};
		a.color          = { 1.0f, 1.0f, 1.0f, 1.0f };
		a.uvOffset       = {};
		a.uvScale        = { 1.0f, 1.0f };
		// 拡張フィールド
		a.origin         = position;          // 生成位置を記録
		a.initialScale   = { 1.0f, 1.0f, 1.0f };
		a.initialColor   = { 1.0f, 1.0f, 1.0f, 1.0f };
		a.angularVelocity= {};
		a.phase          = 0.0f;
		a.flipbookFrame  = 0.0f;

		// ── Spawn モジュールを適用して初期状態を確定 ─────────────────
		for (auto& module : spawnModules_) {
			if (module->IsEnabled()) module->OnSpawn(attributes_.data(), index);
		}

		// Spawn モジュール適用後に initialScale/initialColor を確定
		// （UpdateSizeOverLifetime / UpdateColorOverLifetime の補間起点）
		a.initialScale = a.scale;
		a.initialColor = a.color;

		// プリウォーム：経過時間を寿命内のランダム値にして集団の年齢をばらけさせる。
		// SpawnLifeTime 適用後の lifeTime を使うため、この位置で行う。
		if (prewarmAge && a.lifeTime > 0.0f) {
			a.currentTime = ParticleMath::RandomRange(0.0f, a.lifeTime);
		}
	}
}

void YParticleSystem::SetTexture(const std::string& textureFilePath)
{
	textureFilePath_ = textureFilePath;

	if (!textureFilePath.empty()) {
		// テクスチャ読み込み
		TextureManager::GetInstance()->LoadTexture(textureFilePath);
		textureIndexSRV_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
	}
}

void YParticleSystem::SetMeshType(int meshType, const nlohmann::json& params)
{
	currentMeshType_ = meshType;

	// パラメータ配列を読み込み
	if (params.is_array()) {
		size_t paramCount = std::min(params.size(), size_t(4));
		for (size_t i = 0; i < paramCount; ++i) {
			meshParams_[i] = params[i].get<float>();
		}
	}

	// メッシュを再生成
	RefreshMesh();
}

void YParticleSystem::SetMeshTypeAndParams(int meshType, const float* params)
{
	currentMeshType_ = meshType;

	if (params) {
		for (int i = 0; i < 4; ++i) {
			meshParams_[i] = params[i];
		}
	}

	// メッシュを再生成
	RefreshMesh();
}

int YParticleSystem::FindEmptyIndex() const
{
	for (uint32_t i = 0; i < maxParticles_; ++i) {
		if (!attributes_[i].isActive) return i;
	}
	return -1;
}

void YParticleSystem::RefreshMesh()
{
	switch (currentMeshType_) {
	case 0: mesh_ = MeshPrimitive::CreatePlane(meshParams_[0], meshParams_[1]); break;
	case 1: mesh_ = MeshPrimitive::CreateBox(meshParams_[0], meshParams_[1], meshParams_[2]); break;
	case 2: mesh_ = MeshPrimitive::CreateRing(meshParams_[0], meshParams_[1], (uint32_t)meshParams_[2]); break;
	case 3: mesh_ = MeshPrimitive::CreateCylinder(meshParams_[0], meshParams_[1], (uint32_t)meshParams_[2], meshParams_[3]); break;
	case 4: mesh_ = MeshPrimitive::CreateFanShape(meshParams_[0], meshParams_[1], (uint32_t)meshParams_[2]); break;
	case 5: mesh_ = MeshPrimitive::CreateSphere(meshParams_[0], (uint32_t)meshParams_[2]); break;
	}
}

std::shared_ptr<IParticleModule> YParticleSystem::GetModule(const std::string& moduleName) const
{
	// Spawnモジュールから検索
	for (const auto& m : spawnModules_) {
		if (m->GetName() == moduleName) return m;
	}
	// Updateモジュールから検索
	for (const auto& m : updateModules_) {
		if (m->GetName() == moduleName) return m;
	}
	return nullptr;
}

//=================================================================
// モジュールの並べ替え（エディタ用）
//=================================================================

namespace {

	template <class T>
	void MoveElement(std::vector<T>& v, size_t from, size_t to) {
		if (from >= v.size() || to >= v.size() || from == to) return;
		T item = std::move(v[from]);
		v.erase(v.begin() + from);
		v.insert(v.begin() + to, std::move(item));
	}

} // namespace

void YParticleSystem::MoveSpawnModule(size_t from, size_t to) { MoveElement(spawnModules_, from, to); }
void YParticleSystem::MoveUpdateModule(size_t from, size_t to) { MoveElement(updateModules_, from, to); }

void YParticleSystem::InsertSpawnModule(size_t at, std::shared_ptr<ISpawnModule> module)
{
	if (!module) return;
	module->Initialize(maxParticles_);
	at = (std::min)(at, spawnModules_.size());
	spawnModules_.insert(spawnModules_.begin() + at, std::move(module));
}

void YParticleSystem::InsertUpdateModule(size_t at, std::shared_ptr<IUpdateModule> module)
{
	if (!module) return;
	module->Initialize(maxParticles_);
	at = (std::min)(at, updateModules_.size());
	updateModules_.insert(updateModules_.begin() + at, std::move(module));
}

//=================================================================
// JSON（保存・読み込みはここに一本化）
//=================================================================

namespace {

	template <class TModule>
	nlohmann::json SaveModuleList(const std::vector<std::shared_ptr<TModule>>& modules, const nlohmann::json& unknown)
	{
		nlohmann::json arr = nlohmann::json::array();
		for (const auto& m : modules) {
			nlohmann::json mj;
			mj["type"] = m->GetTypeName();
			mj["name"] = m->GetDisplayName(); // 作者が付けた名前を保つ
			// 既定（有効）は書かない＝既存ファイルの差分を増やさない
			if (!m->IsEnabled()) mj["enabled"] = false;
			m->SaveToJson(mj["data"]);
			arr.push_back(std::move(mj));
		}
		// 読み込めなかったモジュールも消さずに書き戻す（エディタで保存し直しても失われない）
		for (const auto& u : unknown) arr.push_back(u);
		return arr;
	}

} // namespace

void YParticleSystem::SaveToJson(nlohmann::json& json) const
{
	json["name"]              = name_;
	json["maxParticles"]      = maxParticles_;
	json["texture"]           = textureFilePath_;
	json["isRelative"]        = isRelative_;
	json["billboardType"]     = GetBillboardTypeAsUInt();
	json["BlendMode"]         = static_cast<int>(blendMode_);
	json["Lighting"]          = IsEnableLight();
	json["softParticle"]      = softParticle_;
	json["softFadeDistance"]  = softFadeDistance_;
	json["emissiveIntensity"] = emissiveIntensity_;
	json["mesh"]["type"]      = currentMeshType_;
	json["mesh"]["params"]    = { meshParams_[0], meshParams_[1], meshParams_[2], meshParams_[3] };
	json["spawnModules"]      = SaveModuleList(spawnModules_, unknownSpawnModules_);
	json["updateModules"]     = SaveModuleList(updateModules_, unknownUpdateModules_);
}

void YParticleSystem::LoadFromJson(const nlohmann::json& json)
{
	// 同名システムの再読込や Undo で二重追加しないよう、モジュールは必ず作り直す
	ClearModules();

	// テクスチャ・メッシュは変わったときだけ作り直す（Undo のたびに GPU リソースを作り直さないため）
	if (json.contains("texture") && json["texture"].get<std::string>() != textureFilePath_) {
		SetTexture(json["texture"]);
	}
	if (json.contains("isRelative"))        isRelative_ = json["isRelative"];
	if (json.contains("billboardType"))     billboardType_ = static_cast<BillboardType>(json["billboardType"].get<uint32_t>());
	if (json.contains("BlendMode"))         blendMode_ = static_cast<BlendMode>(json["BlendMode"].get<int>());
	if (json.contains("Lighting")) {
		const bool on = json["Lighting"].get<bool>();
		lightSetting_.enableDirectionalLight = on;
		lightSetting_.enablePointLight       = on;
		lightSetting_.enableSpotLight        = on;
	}
	if (json.contains("softParticle"))      softParticle_ = json["softParticle"];
	if (json.contains("softFadeDistance"))  softFadeDistance_ = json["softFadeDistance"];
	if (json.contains("emissiveIntensity")) emissiveIntensity_ = json["emissiveIntensity"];

	if (json.contains("mesh")) {
		const auto& meshJson = json["mesh"];
		if (meshJson.contains("type") && meshJson.contains("params")) {
			const int type = meshJson["type"];
			bool same = (type == currentMeshType_) && meshJson["params"].is_array();
			for (size_t i = 0; same && i < 4 && i < meshJson["params"].size(); ++i) {
				same = (meshJson["params"][i].get<float>() == meshParams_[i]);
			}
			if (!same || !mesh_) SetMeshType(type, meshJson["params"]);
		}
	}

	auto& factory = YParticleModuleFactory::GetInstance();
	if (json.contains("spawnModules")) {
		for (const auto& mj : json["spawnModules"]) {
			auto module = factory.CreateSpawnModule(mj.value("type", ""));
			if (!module) {
				unknownSpawnModules_.push_back(mj);
				Logger("YParticleSystem[" + name_ + "]: 未登録の生成モジュール \"" + mj.value("type", "") + "\" を保持して読み飛ばしました");
				continue;
			}
			if (mj.contains("data")) module->LoadFromJson(mj["data"]);
			module->SetEnabled(mj.value("enabled", true));
			module->SetLabel(mj.value("name", std::string{}));
			AddSpawnModule(module);
		}
	}
	if (json.contains("updateModules")) {
		for (const auto& mj : json["updateModules"]) {
			auto module = factory.CreateUpdateModule(mj.value("type", ""));
			if (!module) {
				unknownUpdateModules_.push_back(mj);
				Logger("YParticleSystem[" + name_ + "]: 未登録の更新モジュール \"" + mj.value("type", "") + "\" を保持して読み飛ばしました");
				continue;
			}
			if (mj.contains("data")) module->LoadFromJson(mj["data"]);
			module->SetEnabled(mj.value("enabled", true));
			module->SetLabel(mj.value("name", std::string{}));
			AddUpdateModule(module);
		}
	}
}

#ifdef USE_IMGUI
void YParticleSystem::DrawSettingsEditor()
{

	// --- テクスチャ設定 ---
	if (ImGui::TreeNode("テクスチャ設定")) {
		ImGui::Text("パス: %s", textureFilePath_.c_str());

		if (ImGui::Button("テクスチャを開く")) {
			textureBrowser_.Scan(); // 最新状態に更新してから開く
			showTextureBrowser_ = true;
		}

		// ポップアップでインラインブラウザを表示
		if (showTextureBrowser_) {
			ImGui::OpenPopup("##TextureBrowser");
		}
		ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Appearing);
		if (ImGui::BeginPopupModal("##TextureBrowser", &showTextureBrowser_,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
		{
			ImGui::Text("テクスチャを選択");
			ImGui::Separator();
			// Grid モードで表示（クリックでコールバックが発火し自動クローズ）
			textureBrowser_.Draw("##TexBrowserChild", ImVec2(0, 340));
			ImGui::Separator();
			if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
				showTextureBrowser_ = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::TreePop();
	}


	// --- メッシュ設定 ---
	if (ImGui::TreeNode("メッシュ設定")) {
		bool changed = false;
		const char* meshTypes[] = { "平面", "ボックス", "リング", "シリンダー","扇形","円形"};

		// Comboでタイプ選択
		if (ImGui::Combo("プリミティブタイプ", &currentMeshType_, meshTypes, IM_ARRAYSIZE(meshTypes))) {
			changed = true;
		}

		// 選択されたタイプに応じて表示するスライダーを切り替え
		switch (currentMeshType_) {
		case 0: // Plane(w, h)
			changed |= ImGui::DragFloat("幅", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("高さ", &meshParams_[1], 0.1f, 0.1f, 100.0f);
			break;
		case 1: // Box(w, h, d)
			changed |= ImGui::DragFloat("幅", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("高さ", &meshParams_[1], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("奥行き", &meshParams_[2], 0.1f, 0.1f, 100.0f);
			break;
		case 2: // Ring(outer, inner, divide)
			changed |= ImGui::DragFloat("外側半径", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("内側半径", &meshParams_[1], 0.1f, 0.0f, 100.0f);
			{
				int div = (int)meshParams_[2];
				if (ImGui::DragInt("分割数", &div, 1, 3, 128)) {
					meshParams_[2] = (float)div;
					changed = true;
				}
			}
			break;
		case 3: // Cylinder(outer, inner, divide, height)
			changed |= ImGui::DragFloat("外側半径", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("内側半径", &meshParams_[1], 0.1f, 0.0f, 100.0f);
			changed |= ImGui::DragFloat("高さ", &meshParams_[3], 0.1f, 0.1f, 100.0f);
			{
				int div = (int)meshParams_[2];
				if (ImGui::DragInt("分割数", &div, 1, 3, 128)) {
					meshParams_[2] = (float)div;
					changed = true;
				}
			}
			break;

		case 4: // Fan Shape (radius, angle, divide)
			changed |= ImGui::DragFloat("半径", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("角度", &meshParams_[1], 1.0f, 1.0f, 360.0f);
			{
				int div = (int)meshParams_[2];
				if (ImGui::DragInt("分割数", &div, 1, 3, 128)) {
					meshParams_[2] = (float)div;
					changed = true;
				}
			}
			break;

		case 5: // Sphere (radius, -, divide)
			changed |= ImGui::DragFloat("半径", &meshParams_[0], 0.1f, 0.1f, 100.0f);
			{
				int div = (int)meshParams_[2];
				if (ImGui::DragInt("分割数", &div, 1, 3, 128)) {
					meshParams_[2] = (float)div;
					changed = true;
				}
			}
			break;
		}

		// 値が変わった場合のみ再生成（メモリリーク防止のため）
		if (changed || mesh_ == nullptr) {
			RefreshMesh();
		}
		ImGui::TreePop();
	}

	// --- ビルボード設定 ---
	if (ImGui::TreeNode("ビルボード設定")) {
		const char* billboardTypes[] = { "なし", "完全ビルボード", "Y軸固定" };
		int currentType = static_cast<int>(billboardType_);

		if (ImGui::Combo("ビルボードタイプ", &currentType, billboardTypes, IM_ARRAYSIZE(billboardTypes))) {
			billboardType_ = static_cast<BillboardType>(currentType);
		}

		// 説明文
		ImGui::Separator();
		ImGui::TextWrapped("ビルボードタイプ:");
		ImGui::BulletText("なし: ビルボードなしの通常メッシュレンダリング");
		ImGui::BulletText("完全ビルボード: 常にカメラに向く");
		ImGui::BulletText("Y軸固定: Y軸のみ回転（地面エフェクト用）");

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("ライティング設定")) {
		ImGui::Separator();

		// --- 各ライトの有効/無効 ---
		ImGui::Text("ライトの有効化");
		ImGui::Checkbox("ディレクショナルライト", &lightSetting_.enableDirectionalLight);

		ImGui::Spacing();

		// --- 強度スライダー（有効なライトのみ表示）---
		if (lightSetting_.enableDirectionalLight) {
			ImGui::SliderFloat("ディレクショナル強度", &lightSetting_.directionalIntensity, 0.0f, 2.0f);
		}

		ImGui::TreePop();
	}

	// ブレンドモード
	const char* blendNames[] = { "なし", "通常", "加算", "減算", "乗算", "スクリーン" };
	int currentMode = (int)blendMode_;
	if (ImGui::Combo("ブレンドモード", &currentMode, blendNames, IM_ARRAYSIZE(blendNames))) {
		SetBlendMode((BlendMode)currentMode);
	}

	// ソフトパーティクル（深度フェード）
	ImGui::Checkbox("ソフトパーティクル", &softParticle_);
	if (softParticle_) {
		ImGui::SliderFloat("フェード距離", &softFadeDistance_, 0.01f, 5.0f);
		ImGui::TextDisabled("地形・壁・敵に近づくほど薄くなり接地感が出ます。");
	}

	// エミッシブ強度（明るさ → Bloom の乗り）
	ImGui::DragFloat("エミッシブ強度", &emissiveIntensity_, 0.05f, 0.0f, 20.0f);
	ImGui::TextDisabled("rgbを明るくして Bloom を強く乗せます（ド派手用）。要 Bloom 有効。");

}
#endif
