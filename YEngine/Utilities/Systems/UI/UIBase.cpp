#include "UIBase.h"
#include <fstream>
#include <chrono>
#include <thread>
#include "Sprite/SpriteCommon.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

const std::string UIBase::PRESET_DIRECTORY = "./Resources/Json/UI/";

UIBase::UIBase(const std::string& name) :
	sprite_(nullptr),
	hotReloadEnabled_(false),
	name_(name) {
}

UIBase::~UIBase() {
	if (!configPath_.empty()) {
		// 必要に応じて保存
		// SaveToJSON();
	}
}

void UIBase::Initialize(const std::string& jsonConfigPath) {
	configPath_ = jsonConfigPath;

	sprite_ = std::make_unique<Sprite>();

	bool jsonExists = std::filesystem::exists(jsonConfigPath);

	if (jsonExists) {
		LoadFromJSON(jsonConfigPath);
	} else {
		sprite_->Initialize("./Resources/images/white.png");
		texturePath_ = "./Resources/images/white.png";
		SaveToJSON();
	}

	if (std::filesystem::exists(configPath_)) {
		lastModTime_ = std::filesystem::last_write_time(configPath_);
	}
}

void UIBase::Update() {
	if (IsAnimating()) {
		UpdateAnimation(1.0f / 60.0f);
	}

	if (hotReloadEnabled_) {
		CheckForChanges();
	}

	if (sprite_ && visible_) {
		sprite_->Update();
	}

#ifdef USE_IMGUI
	ImGUi();
#endif
}

void UIBase::Draw() {
	if (sprite_ && visible_) {
		sprite_->Draw();
	}
}

/*==================================================================
						アニメーション機能
===================================================================*/

void UIBase::PlayPositionAnimation(const Vector3& from, const Vector3& to, float duration, bool loop) {
	currentAnimation_.type = UIAnimation::Type::Position;
	currentAnimation_.startPos = from;
	currentAnimation_.endPos = to;
	currentAnimation_.duration = duration;
	currentAnimation_.elapsed = 0.0f;
	currentAnimation_.loop = loop;
	SetPosition(from);
}

void UIBase::PlayScaleAnimation(const Vector2& from, const Vector2& to, float duration, bool loop) {
	currentAnimation_.type = UIAnimation::Type::Scale;
	currentAnimation_.startScale = from;
	currentAnimation_.endScale = to;
	currentAnimation_.duration = duration;
	currentAnimation_.elapsed = 0.0f;
	currentAnimation_.loop = loop;
	SetScale(from);
}

void UIBase::PlayAlphaAnimation(float from, float to, float duration, bool loop) {
	currentAnimation_.type = UIAnimation::Type::Alpha;
	currentAnimation_.startAlpha = from;
	currentAnimation_.endAlpha = to;
	currentAnimation_.duration = duration;
	currentAnimation_.elapsed = 0.0f;
	currentAnimation_.loop = loop;
	SetAlpha(from);
}

void UIBase::PlayColorAnimation(const Vector4& from, const Vector4& to, float duration, bool loop) {
	currentAnimation_.type = UIAnimation::Type::Color;
	currentAnimation_.startColor = from;
	currentAnimation_.endColor = to;
	currentAnimation_.duration = duration;
	currentAnimation_.elapsed = 0.0f;
	currentAnimation_.loop = loop;
	SetColor(from);
}

void UIBase::StopAnimation() {
	currentAnimation_.type = UIAnimation::Type::None;
	currentAnimation_.elapsed = 0.0f;
}

void UIBase::UpdateAnimation(float deltaTime) {
	if (!IsAnimating()) return;

	currentAnimation_.elapsed += deltaTime;
	float t = currentAnimation_.elapsed / currentAnimation_.duration;

	if (t >= 1.0f) {
		if (currentAnimation_.loop) {
			currentAnimation_.elapsed = 0.0f;
			t = 0.0f;
		} else {
			t = 1.0f;
			currentAnimation_.type = UIAnimation::Type::None;
		}
	}

	switch (currentAnimation_.type) {
	case UIAnimation::Type::Position: {
		Vector3 pos;
		pos.x = currentAnimation_.startPos.x + (currentAnimation_.endPos.x - currentAnimation_.startPos.x) * t;
		pos.y = currentAnimation_.startPos.y + (currentAnimation_.endPos.y - currentAnimation_.startPos.y) * t;
		pos.z = currentAnimation_.startPos.z + (currentAnimation_.endPos.z - currentAnimation_.startPos.z) * t;
		SetPosition(pos);
		break;
	}
	case UIAnimation::Type::Scale: {
		Vector2 scale;
		scale.x = currentAnimation_.startScale.x + (currentAnimation_.endScale.x - currentAnimation_.startScale.x) * t;
		scale.y = currentAnimation_.startScale.y + (currentAnimation_.endScale.y - currentAnimation_.startScale.y) * t;
		SetScale(scale);
		break;
	}
	case UIAnimation::Type::Alpha: {
		float alpha = currentAnimation_.startAlpha + (currentAnimation_.endAlpha - currentAnimation_.startAlpha) * t;
		SetAlpha(alpha);
		break;
	}
	case UIAnimation::Type::Color: {
		Vector4 color;
		color.x = currentAnimation_.startColor.x + (currentAnimation_.endColor.x - currentAnimation_.startColor.x) * t;
		color.y = currentAnimation_.startColor.y + (currentAnimation_.endColor.y - currentAnimation_.startColor.y) * t;
		color.z = currentAnimation_.startColor.z + (currentAnimation_.endColor.z - currentAnimation_.startColor.z) * t;
		color.w = currentAnimation_.startColor.w + (currentAnimation_.endColor.w - currentAnimation_.startColor.w) * t;
		SetColor(color);
		break;
	}
	}
}

/*==================================================================
						グリッド・スナップ
===================================================================*/

Vector3 UIBase::SnapToGrid(const Vector3& position) const {
	if (!gridEnabled_) return position;

	Vector3 snapped;
	snapped.x = std::round(position.x / gridSize_) * gridSize_;
	snapped.y = std::round(position.y / gridSize_) * gridSize_;
	snapped.z = position.z;
	return snapped;
}

/*==================================================================
						プリセット機能
===================================================================*/

bool UIBase::SaveAsPreset(const std::string& presetName) {
	if (!std::filesystem::exists(PRESET_DIRECTORY)) {
		std::filesystem::create_directories(PRESET_DIRECTORY);
	}

	std::string presetPath = PRESET_DIRECTORY + presetName + ".json";
	return SaveToJSON(presetPath);
}

bool UIBase::LoadPreset(const std::string& presetName) {
	std::string presetPath = PRESET_DIRECTORY + presetName + ".json";
	if (!std::filesystem::exists(presetPath)) {
		return false;
	}
	return LoadFromJSON(presetPath);
}

std::vector<std::string> UIBase::GetAvailablePresets() const {
	std::vector<std::string> presets;

	if (!std::filesystem::exists(PRESET_DIRECTORY)) {
		return presets;
	}

	for (const auto& entry : std::filesystem::directory_iterator(PRESET_DIRECTORY)) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			presets.push_back(entry.path().stem().string());
		}
	}

	std::sort(presets.begin(), presets.end());
	return presets;
}

/*==================================================================
						プロパティコピー
===================================================================*/

void UIBase::CopyPropertiesFrom(const UIBase* other) {
	if (!other) return;

	SetPosition(other->GetPosition());
	SetRotation(other->GetRotation());
	SetScale(other->GetScale());
	SetColor(other->GetColor());
	SetFlipX(other->GetFlipX());
	SetFlipY(other->GetFlipY());
	SetAnchorPoint(other->GetAnchorPoint());
	SetTextureLeftTop(other->GetTextureLeftTop());
	SetTextureSize(other->GetTextureSize());
	SetUVTranslation(other->GetUVTranslation());
	SetUVRotation(other->GetUVRotation());
	SetUVScale(other->GetUVScale());
}

/*==================================================================
						ImGui拡張
===================================================================*/

void UIBase::ImGuiGridSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("グリッド設定")) {
		ImGui::Checkbox("グリッドを有効化", &gridEnabled_);

		if (gridEnabled_) {
			ImGui::DragFloat("グリッドサイズ", &gridSize_, 1.0f, 1.0f, 100.0f);

			if (ImGui::Button("位置をグリッドにスナップ")) {
				SetPosition(SnapToGrid(GetPosition()));
			}
		}
	}
#endif
}

void UIBase::ImGuiAnimationSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("アニメーション")) {
		if (IsAnimating()) {
			ImGui::Text("アニメーション再生中...");
			ImGui::ProgressBar(currentAnimation_.elapsed / currentAnimation_.duration);

			if (ImGui::Button("停止")) {
				StopAnimation();
			}
		} else {
			static int animType = 0;
			static float duration = 1.0f;
			static bool loop = false;

			ImGui::Combo("アニメーションタイプ", &animType,
				"位置\0スケール\0アルファ\0色\0");
			ImGui::DragFloat("時間(秒)", &duration, 0.1f, 0.1f, 10.0f);
			ImGui::Checkbox("ループ", &loop);

			if (ImGui::Button("簡易アニメーション再生")) {
				Vector3 currentPos = GetPosition();
				Vector2 currentScale = GetScale();

				switch (animType) {
				case 0:
					PlayPositionAnimation(currentPos,
						{ currentPos.x + 100.0f, currentPos.y, currentPos.z },
						duration, loop);
					break;
				case 1:
					PlayScaleAnimation(currentScale,
						{ currentScale.x * 1.5f, currentScale.y * 1.5f },
						duration, loop);
					break;
				case 2:
					PlayAlphaAnimation(GetAlpha(), 0.0f, duration, loop);
					break;
				case 3:
					PlayColorAnimation(GetColor(),
						{ 1.0f, 0.0f, 0.0f, 1.0f }, duration, loop);
					break;
				}
			}
		}
	}
#endif
}

void UIBase::ImGuiPresetSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("プリセット")) {
		static char presetName[128] = "";
		ImGui::InputText("プリセット名", presetName, sizeof(presetName));

		if (ImGui::Button("現在の設定を保存")) {
			if (strlen(presetName) > 0) {
				if (SaveAsPreset(presetName)) {
					ImGui::OpenPopup("PresetSaved");
				}
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("プリセットから読み込み")) {
			ImGui::OpenPopup("LoadPresetPopup");
		}

		if (ImGui::BeginPopupModal("PresetSaved", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("プリセットを保存しました!");
			if (ImGui::Button("OK")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("LoadPresetPopup")) {
			ImGui::Text("プリセットを選択:");
			ImGui::Separator();

			auto presets = GetAvailablePresets();
			for (const auto& preset : presets) {
				if (ImGui::Selectable(preset.c_str())) {
					LoadPreset(preset);
					ImGui::CloseCurrentPopup();
				}
			}

			if (presets.empty()) {
				ImGui::TextDisabled("プリセットがありません");
			}

			ImGui::EndPopup();
		}
	}
#endif
}

void UIBase::ImGuiQuickAlignment() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("クイック配置")) {
		ImGui::Text("画面位置:");

		if (ImGui::Button("左上")) {
			SetPosition({ 0.0f, 0.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央上")) {
			SetPosition({ 640.0f, 0.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右上")) {
			SetPosition({ 1280.0f, 0.0f, GetPosition().z });
		}

		if (ImGui::Button("左中央")) {
			SetPosition({ 0.0f, 360.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央")) {
			SetPosition({ 640.0f, 360.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右中央")) {
			SetPosition({ 1280.0f, 360.0f, GetPosition().z });
		}

		if (ImGui::Button("左下")) {
			SetPosition({ 0.0f, 720.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央下")) {
			SetPosition({ 640.0f, 720.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右下")) {
			SetPosition({ 1280.0f, 720.0f, GetPosition().z });
		}
	}
#endif
}

void UIBase::ImGuiUVSRTSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("UV SRT")) {
		// UV Translation
		Vector2 uvTrans = GetUVTranslation();
		if (ImGui::DragFloat2("UV Translation", &uvTrans.x, 0.01f, -10.0f, 10.0f)) {
			SetUVTranslation(uvTrans);
		}

		// UV Rotation
		float uvRot = GetUVRotation();
		if (ImGui::DragFloat("UV Rotation", &uvRot, 0.01f, -3.14159f * 2.0f, 3.14159f * 2.0f)) {
			SetUVRotation(uvRot);
		}

		// UV Scale
		Vector2 uvSc = GetUVScale();
		if (ImGui::DragFloat2("UV Scale", &uvSc.x, 0.01f, -10.0f, 10.0f)) {
			SetUVScale(uvSc);
		}

		ImGui::Separator();

		// リセットボタン
		if (ImGui::Button("UVリセット")) {
			SetUVTranslation({ 0.0f, 0.0f });
			SetUVRotation(0.0f);
			SetUVScale({ 1.0f, 1.0f });
		}

		ImGui::SameLine();

		// プリセットボタン
		if (ImGui::Button("UV反転X")) {
			SetUVScale({ -GetUVScale().x, GetUVScale().y });
		}

		ImGui::SameLine();

		if (ImGui::Button("UV反転Y")) {
			SetUVScale({ GetUVScale().x, -GetUVScale().y });
		}

		// UV Tiling プリセット
		ImGui::Text("UV Tiling:");
		if (ImGui::Button("1x1")) {
			SetUVScale({ 1.0f, 1.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("2x2")) {
			SetUVScale({ 2.0f, 2.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("4x4")) {
			SetUVScale({ 4.0f, 4.0f });
		}
	}
#endif
}

void UIBase::ImGUi() {
#ifdef USE_IMGUI
	if (!sprite_) return;


	bool modified = false;

	char nameBuffer[256];
	strncpy_s(nameBuffer, name_.c_str(), sizeof(nameBuffer) - 1);
	nameBuffer[sizeof(nameBuffer) - 1] = '\0';
	if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer))) {
		name_ = nameBuffer;
		modified = true;
	}

	if (ImGui::CollapsingHeader("トランスフォーム", ImGuiTreeNodeFlags_DefaultOpen)) {
		Vector2 scale = GetScale();
		if (ImGui::DragFloat2("拡大縮小", &scale.x, 0.5f)) {
			SetScale(scale);
			modified = true;
		}

		Vector3 rotation = GetRotation();
		if (ImGui::DragFloat3("回転", &rotation.x, 0.1f)) {
			SetRotation(rotation);
			modified = true;
		}

		Vector3 position = GetPosition();
		if (ImGui::DragFloat3("位置", &position.x, 1.0f)) {
			SetPosition(position);
			modified = true;
		}
	}

	if (ImGui::CollapsingHeader("マテリアル", ImGuiTreeNodeFlags_DefaultOpen)) {
		Vector4 color = GetColor();
		if (ImGui::ColorEdit4("色", &color.x)) {
			SetColor(color);
			modified = true;
		}

		bool flipX = GetFlipX();
		if (ImGui::Checkbox("X軸反転", &flipX)) {
			SetFlipX(flipX);
			modified = true;
		}

		ImGui::SameLine();

		bool flipY = GetFlipY();
		if (ImGui::Checkbox("Y軸反転", &flipY)) {
			SetFlipY(flipY);
			modified = true;
		}
	}

	if (ImGui::CollapsingHeader("テクスチャ", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("現在のテクスチャ: %s", texturePath_.c_str());

		// 🔍 フィルタ入力
		static char textureFilter[128] = "";
		ImGui::InputTextWithHint("##filter", "ファイル名で検索...", textureFilter, sizeof(textureFilter));

		if (ImGui::Button("テクスチャを変更")) {
			ImGui::OpenPopup("TextureSelectPopup");
		}

		if (ImGui::BeginPopup("TextureSelectPopup")) {
			ImGui::Text("📁 画像を選択:");
			ImGui::Separator();

			std::string baseDir = "./Resources/Textures/";
			std::function<void(const std::filesystem::path&)> DrawFolderTree;

			DrawFolderTree = [&](const std::filesystem::path& folder) {
				for (const auto& entry : std::filesystem::directory_iterator(folder)) {
					if (entry.is_directory()) {
						// フォルダ表示（アイコン付き）
						std::string folderName = "📂 " + entry.path().filename().string();
						if (ImGui::TreeNode(folderName.c_str())) {
							DrawFolderTree(entry.path());
							ImGui::TreePop();
						}
					} else if (entry.is_regular_file()) {
						auto ext = entry.path().extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(),
							[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

						if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
							std::string filename = entry.path().filename().string();

							// 検索フィルタ適用
							if (strlen(textureFilter) > 0 && filename.find(textureFilter) == std::string::npos) {
								continue;
							}

							std::string displayName = filename;
							bool isCurrent = (texturePath_ == entry.path().string());

							// 現在のテクスチャを強調表示
							if (isCurrent) {
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
							}

							if (ImGui::Selectable(displayName.c_str(), isCurrent, ImGuiSelectableFlags_AllowDoubleClick)) {
								std::string fullPath = entry.path().string();
								SetTexture(fullPath);
								ImGui::CloseCurrentPopup();
							}

							if (isCurrent) {
								ImGui::SameLine();
								ImGui::TextDisabled("（使用中）");
								ImGui::PopStyleColor();
							}
						}
					}
				}
				};

			if (std::filesystem::exists(baseDir)) {
				DrawFolderTree(baseDir);
			} else {
				ImGui::TextDisabled("Resources/images/ が存在しません。");
			}

			ImGui::EndPopup();
		}

		Vector2 leftTop = sprite_->GetTextureLeftTop();
		if (ImGui::DragFloat2("左上座標", &leftTop.x, 1.0f)) {
			sprite_->SetTextureLeftTop(leftTop);
			modified = true;
		}

		Vector2 textureSize = sprite_->GetTextureSize();
		if (ImGui::DragFloat2("テクスチャサイズ", &textureSize.x, 1.0f)) {
			sprite_->SetTextureSize(textureSize);
			modified = true;
		}

		Vector2 anchor = sprite_->GetAnchorPoint();
		if (ImGui::DragFloat2("アンカーポイント", &anchor.x, 0.01f, 0.0f, 1.0f)) {
			sprite_->SetAnchorPoint(anchor);
			modified = true;
		}
	}

	ImGuiGridSettings();
	ImGuiAnimationSettings();
	ImGuiPresetSettings();
	ImGuiQuickAlignment();
	ImGuiUVSRTSettings();

	if (ImGui::CollapsingHeader("表示設定")) {
		ImGui::Checkbox("表示", &visible_);
		ImGui::DragInt("レイヤー", &layer_, 1.0f, 0, 100);
	}

	bool hotReload = hotReloadEnabled_;
	if (ImGui::Checkbox("ホットリロード", &hotReload)) {
		EnableHotReload(hotReload);
	}

	ImGui::Separator();

	if (ImGui::Button("変更を保存")) {
		if (SaveToJSON()) {
			ImGui::OpenPopup("SaveSuccessPopup");
		} else {
			ImGui::OpenPopup("SaveFailedPopup");
		}
	}

	if (ImGui::BeginPopupModal("SaveSuccessPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("設定が正常に保存されました。");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("SaveFailedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("設定の保存に失敗しました。");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (modified) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "* 未保存の変更があります");
	}

#endif
}

void UIBase::EnableHotReload(bool enable) {
	hotReloadEnabled_ = enable;
}

void UIBase::CheckForChanges() {
	if (configPath_.empty() || !std::filesystem::exists(configPath_)) {
		return;
	}

	auto currentModTime = std::filesystem::last_write_time(configPath_);

	if (currentModTime != lastModTime_) {
		LoadFromJSON(configPath_);
		lastModTime_ = currentModTime;
	}
}

bool UIBase::LoadFromJSON(const std::string& jsonPath) {
	try {
		std::ifstream file(jsonPath);
		if (!file.is_open()) {
			return false;
		}

		nlohmann::json data;
		file >> data;
		file.close();

		ApplyJSONToState(data);

		return true;
	}
	catch (const std::exception& e) {
		printf("JSONからUIの読み込み中にエラー発生: %s\n", e.what());
		return false;
	}
}

bool UIBase::SaveToJSON(const std::string& jsonPath) {
	std::string savePath = jsonPath.empty() ? configPath_ : jsonPath;

	if (savePath.empty()) {
		return false;
	}

	try {
		std::filesystem::path dirPath = std::filesystem::path(savePath).parent_path();

		if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
			std::filesystem::create_directories(dirPath);
		}

		nlohmann::json data = CreateJSONFromCurrentState();

		std::ofstream file(savePath);
		if (!file.is_open()) {
			return false;
		}

		file << std::setw(4) << data << std::endl;
		file.close();

		return true;
	}
	catch (const std::exception& e) {
		printf("JSONへのUI保存中にエラー発生: %s\n", e.what());
		return false;
	}
}

void UIBase::SetPosition(const Vector3& position) {
	if (sprite_) {
		sprite_->SetTranslate(position);
	}
}

Vector3 UIBase::GetPosition() const {
	if (sprite_) {
		return sprite_->GetTranslate();
	}
	return { 0.0f, 0.0f, 0.0f };
}

void UIBase::SetRotation(const Vector3& rotation) {
	if (sprite_) {
		sprite_->SetRotate(rotation);
	}
}

Vector3 UIBase::GetRotation() const {
	if (sprite_) {
		return sprite_->GetRotate();
	}
	return { 0.0f, 0.0f, 0.0f };
}

void UIBase::SetScale(const Vector2& scale) {
	if (sprite_) {
		sprite_->SetSize(scale);
	}
}

Vector2 UIBase::GetScale() const {
	if (sprite_) {
		return sprite_->GetSize();
	}
	return { 1.0f, 1.0f };
}

void UIBase::SetColor(const Vector4& color) {
	if (sprite_) {
		sprite_->SetColor(color);
	}
}

Vector4 UIBase::GetColor() const {
	if (sprite_) {
		return sprite_->GetColor();
	}
	return { 1.0f, 1.0f, 1.0f, 1.0f };
}

void UIBase::SetAlpha(float alpha) {
	if (sprite_) {
		sprite_->SetAlpha(alpha);
	}
}

float UIBase::GetAlpha() const {
	if (sprite_) {
		return sprite_->GetColor().w;
	}
	return 1.0f;
}

void UIBase::SetTexture(const std::string& texturePath) {
	if (sprite_) {
		sprite_->ChangeTexture(texturePath);
		texturePath_ = texturePath;
	}
}

std::string UIBase::GetTexturePath() const {
	return texturePath_;
}

void UIBase::SetCamera(Camera* camera) {
	if (sprite_) {
		sprite_->SetCamera(camera);
	}
}

void UIBase::SetName(const std::string& name) {
	name_ = name;
}

std::string UIBase::GetName() const {
	return name_;
}

void UIBase::SetFlipX(bool flipX) {
	if (sprite_) {
		sprite_->SetIsFlipX(flipX);
	}
}

void UIBase::SetFlipY(bool flipY) {
	if (sprite_) {
		sprite_->SetIsFlipY(flipY);
	}
}

bool UIBase::GetFlipX() const {
	if (sprite_) {
		return sprite_->GetIsFlipX();
	}
	return false;
}

bool UIBase::GetFlipY() const {
	if (sprite_) {
		return sprite_->GetIsFlipY();
	}
	return false;
}

void UIBase::SetTextureLeftTop(const Vector2& leftTop) {
	if (sprite_) {
		sprite_->SetTextureLeftTop(leftTop);
	}
}

Vector2 UIBase::GetTextureLeftTop() const {
	if (sprite_) {
		return sprite_->GetTextureLeftTop();
	}
	return { 0.0f, 0.0f };
}

void UIBase::SetTextureSize(const Vector2& size) {
	if (sprite_) {
		sprite_->SetTextureSize(size);
	}
}

Vector2 UIBase::GetTextureSize() const {
	if (sprite_) {
		return sprite_->GetTextureSize();
	}
	return { 1.0f, 1.0f };
}

void UIBase::SetAnchorPoint(const Vector2& anchor) {
	if (sprite_) {
		sprite_->SetAnchorPoint(anchor);
	}
}

Vector2 UIBase::GetAnchorPoint() const {
	if (sprite_) {
		return sprite_->GetAnchorPoint();
	}
	return { 0.0f, 0.0f };
}

/*==================================================================
						UV SRT制御
===================================================================*/

void UIBase::SetUVTranslation(const Vector2& translation) {
	uvTranslation_ = translation;
	if (sprite_) {
		sprite_->SetUVTranslation(translation);
	}
}

Vector2 UIBase::GetUVTranslation() const {
	return uvTranslation_;
}

void UIBase::SetUVRotation(float rotation) {
	uvRotation_ = rotation;
	if (sprite_) {
		sprite_->SetUVRotation(rotation);
	}
}

float UIBase::GetUVRotation() const {
	return uvRotation_;
}

void UIBase::SetUVScale(const Vector2& scale) {
	uvScale_ = scale;
	if (sprite_) {
		sprite_->SetUVScale(scale);
	}
}

Vector2 UIBase::GetUVScale() const {
	return uvScale_;
}

nlohmann::json UIBase::CreateJSONFromCurrentState() {
	nlohmann::json data;

	data["name"] = name_;
	data["texturePath"] = texturePath_;

	data["position"] = {
		{"x", GetPosition().x},
		{"y", GetPosition().y},
		{"z", GetPosition().z}
	};

	data["rotation"] = {
		{"x", GetRotation().x},
		{"y", GetRotation().y},
		{"z", GetRotation().z}
	};

	data["scale"] = {
		{"x", GetScale().x},
		{"y", GetScale().y}
	};

	data["color"] = {
		{"r", GetColor().x},
		{"g", GetColor().y},
		{"b", GetColor().z},
		{"a", GetColor().w}
	};

	data["flipX"] = GetFlipX();
	data["flipY"] = GetFlipY();

	if (sprite_) {
		data["textureLeftTop"] = {
			{"x", sprite_->GetTextureLeftTop().x},
			{"y", sprite_->GetTextureLeftTop().y}
		};

		data["anchorPoint"] = {
			{"x", sprite_->GetAnchorPoint().x},
			{"y", sprite_->GetAnchorPoint().y}
		};

		data["textureSize"] = {
			{"x", sprite_->GetTextureSize().x},
			{"y", sprite_->GetTextureSize().y}
		};
	}

	data["visible"] = visible_;
	data["layer"] = layer_;

	// UV SRT
	data["uvTranslation"] = {
		{"x", uvTranslation_.x},
		{"y", uvTranslation_.y}
	};
	data["uvRotation"] = uvRotation_;
	data["uvScale"] = {
		{"x", uvScale_.x},
		{"y", uvScale_.y}
	};

	return data;
}

void UIBase::ApplyJSONToState(const nlohmann::json& data) {
	if (data.contains("texturePath")) {
		texturePath_ = data["texturePath"];

		if (!sprite_) {
			sprite_ = std::make_unique<Sprite>();
			sprite_->Initialize(texturePath_);
		} else {
			sprite_->Initialize(texturePath_);
		}
	} else if (!sprite_) {
		sprite_ = std::make_unique<Sprite>();
		sprite_->Initialize("./Resources/images/white.png");
		texturePath_ = "./Resources/images/white.png";
	}

	if (data.contains("name")) {
		name_ = data["name"];
	}

	if (data.contains("position")) {
		Vector3 position;
		position.x = data["position"]["x"];
		position.y = data["position"]["y"];
		position.z = data["position"]["z"];
		SetPosition(position);
	}

	if (data.contains("rotation")) {
		Vector3 rotation;
		rotation.x = data["rotation"]["x"];
		rotation.y = data["rotation"]["y"];
		rotation.z = data["rotation"]["z"];
		SetRotation(rotation);
	}

	if (data.contains("scale")) {
		Vector2 scale;
		scale.x = data["scale"]["x"];
		scale.y = data["scale"]["y"];
		SetScale(scale);
	}

	if (data.contains("color")) {
		Vector4 color;
		color.x = data["color"]["r"];
		color.y = data["color"]["g"];
		color.z = data["color"]["b"];
		color.w = data["color"]["a"];
		SetColor(color);
	}

	if (data.contains("flipX")) {
		SetFlipX(data["flipX"]);
	}

	if (data.contains("flipY")) {
		SetFlipY(data["flipY"]);
	}

	if (sprite_) {
		if (data.contains("textureLeftTop")) {
			Vector2 leftTop;
			leftTop.x = data["textureLeftTop"]["x"];
			leftTop.y = data["textureLeftTop"]["y"];
			sprite_->SetTextureLeftTop(leftTop);
		}

		if (data.contains("anchorPoint")) {
			Vector2 anchor;
			anchor.x = data["anchorPoint"]["x"];
			anchor.y = data["anchorPoint"]["y"];
			sprite_->SetAnchorPoint(anchor);
		}

		if (data.contains("textureSize")) {
			Vector2 size;
			size.x = data["textureSize"]["x"];
			size.y = data["textureSize"]["y"];
			sprite_->SetTextureSize(size);
		}
	}

	if (data.contains("visible")) {
		visible_ = data["visible"];
	}

	if (data.contains("layer")) {
		layer_ = data["layer"];
	}

	// UV SRT
	if (data.contains("uvTranslation")) {
		Vector2 uvTranslation;
		uvTranslation.x = data["uvTranslation"]["x"];
		uvTranslation.y = data["uvTranslation"]["y"];
		SetUVTranslation(uvTranslation);
	}

	if (data.contains("uvRotation")) {
		SetUVRotation(data["uvRotation"]);
	}

	if (data.contains("uvScale")) {
		Vector2 uvScale;
		uvScale.x = data["uvScale"]["x"];
		uvScale.y = data["uvScale"]["y"];
		SetUVScale(uvScale);
	}
}

void UIBase::WatchFileChanges() {
	// 将来的な拡張用
}