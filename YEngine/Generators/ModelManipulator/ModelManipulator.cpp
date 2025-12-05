#include "ModelManipulator.h"

// C++
#include <json.hpp>
#include <iostream>

// Engine
#include "ModelManager.h"
#include <WinApp/WinApp.h>
#include <Editor/Editor.h>

namespace YoRigine {
	ModelManipulator* ModelManipulator::instance_ = nullptr;

	ModelManipulator* ModelManipulator::GetInstance() {
		if (!instance_) instance_ = new ModelManipulator;
		return instance_;
	}

	void ModelManipulator::Initialize(const std::string& sceneName)
	{
		// ObjectManagerの取得
		objectManager_ = ObjectManager::GetInstance();
		objectManager_->Initialize();

		// モデルフォルダスキャンなど
		ScanModelFolder();

#ifdef USE_IMGUI
		ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
		currentGizmoOperation_ = ImGuizmo::TRANSLATE;
		currentGizmoMode_ = ImGuizmo::WORLD;
#endif

		// シーン専用パスを生成
		jsonPath_ = "Resources/Json/Scenes/" + sceneName + ".json";

		// 必要なら自動ロード
		LoadScene(jsonPath_);

		isInitialized_ = true;
		ScanPrefabFolder();

#ifdef USE_IMGUI
		Editor::GetInstance()->RegisterMenuBar([this] { ImGuiMenuBar(); });
#endif
	}

	void ModelManipulator::Update()
	{
		if (!isInitialized_) return;

		// ObjectManagerが全オブジェクトの更新を行う
		objectManager_->Update();
	}

	void ModelManipulator::Draw()
	{
		if (!isInitialized_ || !camera_) return;

		// 全アクティブオブジェクトを描画
		auto objects = objectManager_->GetAllActiveObjects();
		for (auto* obj : objects) {
			if (obj && obj->object && obj->worldTransform) {
				obj->object->Draw(camera_, *obj->worldTransform);
			}
		}
	}

	void ModelManipulator::DrawImGui()
	{
#ifdef USE_IMGUI
		if (!isInitialized_) return;

		// キーボードショートカット（ImGuiがキーボードフォーカスを持っていない時のみ）
		if (!ImGui::GetIO().WantCaptureKeyboard) {
			if (ImGui::IsKeyPressed(ImGuiKey_T)) currentGizmoOperation_ = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation_ = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation_ = ImGuizmo::SCALE;

			// Delete キーで選択オブジェクトを削除
			if (ImGui::IsKeyPressed(ImGuiKey_Delete) && IsValidObjectId(selectedObjectId_)) {
				DeleteSelectedObject();
			}
		}

		// モデル選択ウィンドウ
		if (showModelSelector_) {
			DrawModelSelector();
		}

		// オブジェクトリストウィンドウ
		if (showObjectList_) {
			DrawObjectList();
		}

		// トランスフォーム操作ウィンドウ
		if (showTransformControls_) {
			DrawTransformControls();
		}

		// 複製ウィンドウ
		if (showDuplicateWindow_) {
			DrawDuplicateWindow();
		}

		// プレファブウィンドウ
		if (showPrefabWindow_) {
			DrawPrefabWindow();
		}
#endif // _DEBUG
	}

	void ModelManipulator::ScanModelFolder()
	{
		modelFiles_.clear();
		modelNames_.clear();
		filteredModelIndices_.clear();

		try {
			if (!std::filesystem::exists(modelFolderPath_)) {
				std::cout << "Model folder not found: " << modelFolderPath_ << std::endl;
				return;
			}

			for (const auto& entry : std::filesystem::recursive_directory_iterator(modelFolderPath_)) {
				if (entry.is_regular_file()) {
					std::string filePath = entry.path().string();
					std::string extension = entry.path().extension().string();

					// 拡張子を小文字に変換
					std::transform(extension.begin(), extension.end(), extension.begin(),
						[](char c) -> char {
							return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
						});

					// サポートする拡張子をチェック
					if (extension == ".obj" || extension == ".gltf" || extension == ".fbx" || extension == ".glb") {
						modelFiles_.push_back(filePath);
						std::filesystem::path relativePath = std::filesystem::relative(entry.path(), modelFolderPath_);
						modelNames_.push_back(relativePath.string());
					}
				}
			}

			// フィルタリング用インデックスを初期化
			for (int i = 0; i < static_cast<int>(modelNames_.size()); ++i) {
				filteredModelIndices_.push_back(i);
			}

			// 選択インデックスの検証
			if (selectedModelIndex_ >= static_cast<int>(modelFiles_.size())) {
				selectedModelIndex_ = -1;
			}

			std::cout << "Found " << modelFiles_.size() << " model files" << std::endl;
		}
		catch (const std::filesystem::filesystem_error& e) {
			std::cout << "Error scanning model folder: " << e.what() << std::endl;
		}
	}

	void ModelManipulator::DrawModelSelector()
	{
#ifdef USE_IMGUI
		// ――― フォルダ操作 ―――――――――――――――――
		if (ImGui::Button("フォルダ更新")) {
			ScanModelFolder();
		}
		ImGui::SameLine();
		if (ImGui::Button("フォルダ変更...")) {
			/* TODO: ダイアログ呼び出し */
		}

		ImGui::Text("現在のフォルダ: %s", modelFolderPath_.c_str());
		ImGui::Separator();

		// ――― 検索バー（トグル表示） ―――――――――――――
		if (ImGui::IsKeyPressed(ImGuiKey_F) && ImGui::GetIO().KeyCtrl) {
			showSearchBar_ = !showSearchBar_;
		}
		if (showSearchBar_) {
			if (ImGui::CollapsingHeader("検索バー##ModelSearch", ImGuiTreeNodeFlags_FramePadding))
			{
				ImGui::SetItemAllowOverlap(); // これがないと、ラベルの上にカーソルを置いたときに閉じたり開いたりしてしまう
				ImGui::PushItemWidth(-1);
				if (ImGui::InputText("##SearchBar", modelSearchBuffer_, IM_ARRAYSIZE(modelSearchBuffer_))) {
					FilterModels();
				}
				ImGui::PopItemWidth();
				ImGui::Separator();
			}
		}

		// ――― モデル一覧 ―――――――――――――――――――――
		if (ImGui::BeginListBox("##ModelList", ImVec2(-1, 200))) {
			for (int idx : filteredModelIndices_) {
				if (idx < 0 || idx >= (int)modelNames_.size()) continue;

				bool selected = (selectedModelIndex_ == idx);
				if (ImGui::Selectable(modelNames_[idx].c_str(), selected)) {
					selectedModelIndex_ = idx;
				}
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
					if (idx < (int)modelFiles_.size()) {
						PlaceObject(modelFiles_[idx]);
					}
				}
			}
			ImGui::EndListBox();
		}

		// ――― 配置ボタン & 情報 ―――――――――――――――――
		if (IsValidModelIndex(selectedModelIndex_) && ImGui::Button("シーンに配置する")) {
			PlaceObject(modelFiles_[selectedModelIndex_]);
		}
		if (IsValidModelIndex(selectedModelIndex_)) {
			ImGui::Text("選択中: %s", modelNames_[selectedModelIndex_].c_str());
		}
#endif // _DEBUG
	}

	void ModelManipulator::DrawObjectList()
	{
#ifdef USE_IMGUI
		auto objects = objectManager_->GetAllActiveObjects();
		ImGui::Text("オブジェクト一覧: %d", static_cast<int>(objects.size()));
		ImGui::Separator();

		// 逆順で表示（最新のオブジェクトが上に）
		for (int i = static_cast<int>(objects.size()) - 1; i >= 0; --i) {
			auto* obj = objects[i];
			if (!obj) continue;

			bool isSelected = (selectedObjectId_ == obj->id);
			std::string label = "オブジェクト " + std::to_string(obj->id) +
				" (" + obj->modelName + ")";

			if (ImGui::Selectable(label.c_str(), isSelected)) {
				selectedObjectId_ = obj->id;
			}

			// 右クリックメニュー
			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Delete")) {
					objectManager_->DeleteObject(obj->id);
					if (selectedObjectId_ == obj->id) {
						selectedObjectId_ = -1;
					}
				}
				ImGui::EndPopup();
			}
		}

		ImGui::Separator();

		// 削除ボタン
		if (IsValidObjectId(selectedObjectId_) && ImGui::Button("選択されたオブジェクトを削除")) {
			DeleteSelectedObject();
		}

		ImGui::SameLine();
		if (ImGui::Button("全て削除")) {
			ClearAllObjects();
		}
#endif // _DEBUG
	}

	void ModelManipulator::DrawTransformControls()
	{
#ifdef USE_IMGUI
		auto* obj = objectManager_->GetObjectById(selectedObjectId_);
		if (obj) {
			ImGui::Text("オブジェクトID %d: %s", obj->id, obj->modelName.c_str());
			ImGui::Separator();

			bool changed = false;

			// Position
			if (ImGui::DragFloat3("位置", &obj->position.x, 0.1f)) {
				changed = true;
			}

			// Rotation (度数法で表示)
			Vector3 rotationDegrees = {
				RadToDeg(obj->rotation.x),
				RadToDeg(obj->rotation.y),
				RadToDeg(obj->rotation.z)
			};
			if (ImGui::DragFloat3("回転", &rotationDegrees.x, 1.0f)) {
				obj->rotation = {
					DegToRad(rotationDegrees.x),
					DegToRad(rotationDegrees.y),
					DegToRad(rotationDegrees.z)
				};
				changed = true;
			}

			// Scale
			if (ImGui::DragFloat3("スケール", &obj->scale.x, 0.01f, 0.01f, 10.0f)) {
				changed = true;
			}

			if (changed) {
				objectManager_->UpdateObjectTransform(*obj);
			}

			ImGui::Separator();

			// Reset buttons
			if (ImGui::Button("位置　リセット")) {
				obj->position = { 0.0f, 0.0f, 0.0f };
				objectManager_->UpdateObjectTransform(*obj);
			}
			ImGui::SameLine();
			if (ImGui::Button("回転　リセット")) {
				obj->rotation = { 0.0f, 0.0f, 0.0f };
				objectManager_->UpdateObjectTransform(*obj);
			}
			ImGui::SameLine();
			if (ImGui::Button("スケール　リセット")) {
				obj->scale = { 1.0f, 1.0f, 1.0f };
				objectManager_->UpdateObjectTransform(*obj);
			}

			// 親子関係の設定
			int currentParent = obj->parentID;
			if (ImGui::BeginCombo("Parent", currentParent >= 0 ? std::to_string(currentParent).c_str() : "None")) {
				// 「親なし」オプション
				if (ImGui::Selectable("None", currentParent == -1)) {
					objectManager_->ClearParent(obj->id);
				}

				// 他のオブジェクト一覧から選択
				auto allObjects = objectManager_->GetAllActiveObjects();
				for (auto* candidate : allObjects) {
					if (!candidate || candidate->id == obj->id) continue;

					// 循環参照チェック
					if (objectManager_->HasCircularReference(obj->id, candidate->id)) {
						ImGui::BeginDisabled();
						ImGui::Selectable(("Object " + std::to_string(candidate->id) + " (循環参照)").c_str(), false);
						ImGui::EndDisabled();
					} else {
						std::string label = "Object " + std::to_string(candidate->id);
						bool isSelected = (candidate->id == currentParent);
						if (ImGui::Selectable(label.c_str(), isSelected)) {
							objectManager_->SetParent(obj->id, candidate->id);
						}
					}
				}
				ImGui::EndCombo();
			}

		} else {
			ImGui::Text("オブジェクトが選択されてません");
		}

		ImGui::Separator();

		// Gizmo settings
		ImGui::Text("ギズモ設定");

		// Operation mode
		if (ImGui::RadioButton("位置", currentGizmoOperation_ == ImGuizmo::TRANSLATE)) {
			currentGizmoOperation_ = ImGuizmo::TRANSLATE;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("回転", currentGizmoOperation_ == ImGuizmo::ROTATE)) {
			currentGizmoOperation_ = ImGuizmo::ROTATE;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("スケール", currentGizmoOperation_ == ImGuizmo::SCALE)) {
			currentGizmoOperation_ = ImGuizmo::SCALE;
		}

		// Coordinate system
		if (ImGui::RadioButton("ワールド座標", currentGizmoMode_ == ImGuizmo::WORLD)) {
			currentGizmoMode_ = ImGuizmo::WORLD;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("ローカル座標", currentGizmoMode_ == ImGuizmo::LOCAL)) {
			currentGizmoMode_ = ImGuizmo::LOCAL;
		}

		// Snap settings
		ImGui::Checkbox("スナップ", &useSnap_);
		if (useSnap_) {
			if (currentGizmoOperation_ == ImGuizmo::ROTATE) {
				ImGui::DragFloat("スナップ角度", &rotationSnapDeg_, 0.1f, 0.1f, 45.0f, "%.1f°");
			} else {
				ImGui::DragFloat3("スナップ値", snapValues_, 0.1f, 0.1f, 10.0f);
			}
		}
#endif // _DEBUG
	}

	void ModelManipulator::DrawGizmo()
	{
#ifdef USE_IMGUI
		if (!Editor::GetInstance()->GetShowEditor()) return;
		if (!IsValidObjectId(selectedObjectId_) || !camera_) return;

		auto* obj = objectManager_->GetObjectById(selectedObjectId_);
		if (!obj || !obj->worldTransform) return;

		// カメラ行列
		Matrix4x4 viewMatrix = camera_->GetViewMatrix();
		Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
		Matrix4x4 worldMatrix = obj->worldTransform->GetMatWorld();

		float view[16], proj[16], model[16];
		MatrixToImGuizmo(viewMatrix, view);
		MatrixToImGuizmo(projectionMatrix, proj);
		MatrixToImGuizmo(worldMatrix, model);

		// 専用の透明ウィンドウ
		ImVec2 pos = Editor::GetInstance()->GetGameViewPos();
		ImVec2 size = Editor::GetInstance()->GetGameViewSize();

		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(size);
		ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("GizmoOverlay", nullptr, flags);

		ImGuizmo::BeginFrame();
		ImGuizmo::SetDrawlist();

		ImVec2 rectPos = Editor::GetInstance()->GetGameViewPos();
		ImVec2 rectSize = Editor::GetInstance()->GetGameViewSize();
		ImGuizmo::SetRect(rectPos.x, rectPos.y, rectSize.x, rectSize.y);

		// スナップ
		float* snap = nullptr;
		float snapRad = DegToRad(rotationSnapDeg_);
		if (useSnap_) {
			if (currentGizmoOperation_ == ImGuizmo::ROTATE)
				snap = &snapRad;
			else
				snap = snapValues_;
		}

		bool manipulated = ImGuizmo::Manipulate(
			view, proj,
			currentGizmoOperation_,
			currentGizmoMode_,
			model,
			nullptr,
			snap
		);

		if (ImGuizmo::IsUsing()) {
			ImGui::GetIO().WantCaptureMouse = false;
		}

		if (manipulated) {
			Matrix4x4 newWorld = ImGuizmoToMatrix(model);
			ExtractTransformFromMatrix(newWorld, *obj);
			objectManager_->UpdateObjectTransform(*obj);
		}

		ImGui::End();
		ImGui::PopStyleVar();
#endif // _DEBUG
	}

	void ModelManipulator::Finalize()
	{
		if (objectManager_) {
			objectManager_->Finalize();
		}
		selectedObjectId_ = -1;
		delete instance_;
		instance_ = nullptr;
	}

	void ModelManipulator::PlaceObject(const std::string& modelPath)
	{
		try {
			if (modelPath.empty() || !std::filesystem::exists(modelPath)) {
				std::cout << "Invalid model path: " << modelPath << std::endl;
				return;
			}

			std::filesystem::path fullPath(modelPath);
			std::string fileName = fullPath.filename().string();  // 拡張子付き
			std::filesystem::path relPath = std::filesystem::relative(fullPath, modelFolderPath_);

			// アニメーションの判定（.gltf/.glbならアニメーション対応）
			std::string ext = fullPath.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			bool isAnimation = (ext == ".gltf" || ext == ".glb");

			// ObjectManagerでオブジェクト作成
			auto* obj = objectManager_->CreateObject(relPath.string(), isAnimation);
			if (obj) {
				selectedObjectId_ = obj->id;
				std::cout << "Object placed successfully: " << fileName << std::endl;
			}
		}
		catch (const std::exception& e) {
			std::cout << "Error placing object: " << e.what() << std::endl;
		}
	}

	void ModelManipulator::ExtractTransformFromMatrix([[maybe_unused]] const Matrix4x4& matrix,
		[[maybe_unused]] ObjectManager::PlacedObject& obj)
	{
#ifdef USE_IMGUI
		const float* m = reinterpret_cast<const float*>(&matrix.m[0][0]);

		float t[3]{}, rDeg[3]{}, s[3]{};
		ImGuizmo::DecomposeMatrixToComponents(const_cast<float*>(m), t, rDeg, s);

		obj.position = { t[0], t[1], t[2] };
		obj.rotation = { DegToRad(rDeg[0]), DegToRad(rDeg[1]), DegToRad(rDeg[2]) };
		obj.scale = { s[0], s[1], s[2] };
#endif // _DEBUG
	}

	void ModelManipulator::DeleteSelectedObject()
	{
		if (IsValidObjectId(selectedObjectId_)) {
			objectManager_->DeleteObject(selectedObjectId_);
			selectedObjectId_ = -1;
		}
	}

	void ModelManipulator::ClearAllObjects()
	{
		objectManager_->ClearAllObjects();
		selectedObjectId_ = -1;
	}

	void ModelManipulator::MatrixToImGuizmo(const Matrix4x4& matrix, float* imguizmoMatrix)
	{
		if (!imguizmoMatrix) return;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				imguizmoMatrix[i * 4 + j] = matrix.m[i][j];
			}
		}
	}

	Matrix4x4 ModelManipulator::ImGuizmoToMatrix(const float* imguizmoMatrix)
	{
		Matrix4x4 m{};
		if (!imguizmoMatrix) return m;
		for (int r = 0; r < 4; ++r) {
			for (int c = 0; c < 4; ++c) {
				m.m[r][c] = imguizmoMatrix[r * 4 + c];
			}
		}
		return m;
	}

	void ModelManipulator::FilterModels()
	{
		filteredModelIndices_.clear();

		std::string searchTerm = modelSearchBuffer_;
		std::transform(searchTerm.begin(), searchTerm.end(), searchTerm.begin(),
			[](char c) -> char {
				return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			});

		for (int i = 0; i < static_cast<int>(modelNames_.size()); ++i) {
			std::string modelName = modelNames_[i];
			std::transform(modelName.begin(), modelName.end(), modelName.begin(),
				[](char c) -> char {
					return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				});

			if (searchTerm.empty() || modelName.find(searchTerm) != std::string::npos) {
				filteredModelIndices_.push_back(i);
			}
		}
	}

	void ModelManipulator::ImGuiMenuBar()
	{
#ifdef USE_IMGUI
		if (ImGui::BeginMenu("Create Object"))
		{
			if (ImGui::BeginMenu("View")) {
				ImGui::MenuItem("Model Selector", nullptr, &showModelSelector_);
				ImGui::MenuItem("Object List", nullptr, &showObjectList_);
				ImGui::MenuItem("Transform Controls", nullptr, &showTransformControls_);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Gizmo")) {
				if (ImGui::MenuItem("Translate", "T", currentGizmoOperation_ == ImGuizmo::TRANSLATE))
					currentGizmoOperation_ = ImGuizmo::TRANSLATE;
				if (ImGui::MenuItem("Rotate", "R", currentGizmoOperation_ == ImGuizmo::ROTATE))
					currentGizmoOperation_ = ImGuizmo::ROTATE;
				if (ImGui::MenuItem("Scale", "S", currentGizmoOperation_ == ImGuizmo::SCALE))
					currentGizmoOperation_ = ImGuizmo::SCALE;

				ImGui::Separator();
				if (ImGui::MenuItem("World Space", nullptr, currentGizmoMode_ == ImGuizmo::WORLD))
					currentGizmoMode_ = ImGuizmo::WORLD;
				if (ImGui::MenuItem("Local Space", nullptr, currentGizmoMode_ == ImGuizmo::LOCAL))
					currentGizmoMode_ = ImGuizmo::LOCAL;

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("ファイル")) {
				if (ImGui::MenuItem("配置を保存")) { SaveScene(jsonPath_); }
				if (ImGui::MenuItem("配置を読み込み")) { LoadScene(jsonPath_); }
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Tools")) {
				ImGui::MenuItem("複製ツール", nullptr, &showDuplicateWindow_);
				ImGui::MenuItem("プレファブ", nullptr, &showPrefabWindow_);
				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}
#endif // _DEBUG
	}

	bool ModelManipulator::SaveScene(const std::string& filePath)
	{
		try {
			json j;
			j["version"] = 1;
			j["objects"] = json::array();

			auto objects = objectManager_->GetAllActiveObjects();
			for (const auto* obj : objects) {
				if (obj && obj->object) {
					json objData;
					objData["id"] = obj->id;
					objData["filePath"] = obj->modelPath;
					objData["modelName"] = obj->modelName;
					objData["position"] = { obj->position.x, obj->position.y, obj->position.z };
					objData["rotate"] = { obj->rotation.x, obj->rotation.y, obj->rotation.z };
					objData["scale"] = { obj->scale.x, obj->scale.y, obj->scale.z };
					objData["parentID"] = obj->parentID;
					objData["isAnimation"] = obj->isAnimation;
					objData["animationName"] = obj->animationName;
					j["objects"].push_back(objData);
				}
			}

			std::ofstream file(filePath);
			if (!file.is_open()) {
				std::cout << "Failed to open file for saving: " << filePath << std::endl;
				return false;
			}
			file << j.dump(4);
			file.close();
			std::cout << "Scene saved: " << filePath << std::endl;
			return true;
		}
		catch (const std::exception& e) {
			std::cout << "Error saving scene: " << e.what() << std::endl;
			return false;
		}
	}

	bool ModelManipulator::LoadScene(const std::string& filePath)
	{
		try {
			std::ifstream file(filePath);
			if (!file.is_open()) {
				std::cout << "File open failed: " << filePath << std::endl;
				return false;
			}

			json j;
			file >> j;
			if (j.value("version", 0) != 1) {
				std::cout << "Unsupported scene file version\n";
				return false;
			}

			ClearAllObjects();

			std::unordered_map<int, int> oldToNewId;

			for (const auto& o : j["objects"]) {
				std::filesystem::path fullPath =
					std::filesystem::path(modelFolderPath_) / o["filePath"].get<std::string>();

				bool isAnimation = o.value("isAnimation", false);
				std::string animationName = o.value("animationName", "");

				auto* obj = objectManager_->CreateObject(
					o["filePath"].get<std::string>(),
					isAnimation,
					animationName
				);

				if (!obj) {
					std::cout << "Skip (place failed): " << fullPath << std::endl;
					continue;
				}

				int oldId = o["id"];
				oldToNewId[oldId] = obj->id;

				obj->position = { o["position"][0], o["position"][1], o["position"][2] };
				obj->rotation = { o["rotate"][0], o["rotate"][1], o["rotate"][2] };
				obj->scale = { o["scale"][0], o["scale"][1], o["scale"][2] };

				// 親子関係は後で再マッピング
				if (o.contains("parentID")) {
					int oldParentId = o["parentID"];
					obj->parentID = oldParentId;
				}
			}

			// 親子関係を再マッピング
			auto objects = objectManager_->GetAllActiveObjects();
			for (auto* obj : objects) {
				if (obj->parentID != -1) {
					auto it = oldToNewId.find(obj->parentID);
					if (it != oldToNewId.end()) {
						objectManager_->SetParent(obj->id, it->second);
					} else {
						obj->parentID = -1;
					}
				}
				objectManager_->UpdateObjectTransform(*obj);
			}

			std::cout << "Scene loaded: " << filePath << std::endl;
			return true;
		}
		catch (const std::exception& e) {
			std::cout << "LoadScene error: " << e.what() << std::endl;
			return false;
		}
	}

	void ModelManipulator::HandleMouse()
	{
#ifdef USE_IMGUI
		if (!isInitialized_ || !camera_ || !isMouseSelecting_) return;
		if (!Editor::GetInstance()->GetShowEditor()) return;
		if (ImGui::GetIO().WantCaptureMouse) return;

		ImVec2 gameViewPos = Editor::GetInstance()->GetGameViewPos();
		ImVec2 gameViewSize = Editor::GetInstance()->GetGameViewSize();
		ImVec2 mousePos = ImGui::GetMousePos();

		// マウスがゲームビューの外にある場合は処理しない
		if (mousePos.x < gameViewPos.x || mousePos.x > gameViewPos.x + gameViewSize.x ||
			mousePos.y < gameViewPos.y || mousePos.y > gameViewPos.y + gameViewSize.y) {
			return;
		}

		// 左クリックでオブジェクト選択
		bool isMousePressed = Input::GetInstance()->IsTriggerMouse(0);

		if (!wasMousePressed_ && isMousePressed) {
			// ゲームビュー内での相対座標を計算
			float relativeX = mousePos.x - gameViewPos.x;
			float relativeY = mousePos.y - gameViewPos.y;

			// 正規化座標系に変換 (-1.0 to 1.0)
			float normalizedX = (relativeX / gameViewSize.x) * 2.0f - 1.0f;
			float normalizedY = -((relativeY / gameViewSize.y) * 2.0f - 1.0f); // Yは反転

			// レイキャスティング実行
			PerformRaycast(normalizedX, normalizedY);
		}

		wasMousePressed_ = isMousePressed;
#endif // _DEBUG
	}

	void ModelManipulator::PerformRaycast([[maybe_unused]] float normalizedX, [[maybe_unused]] float normalizedY)
	{
		// TODO: レイキャスティングによるオブジェクト選択
		// 実装する場合は、以下の流れで行う:
		// 1. カメラの逆行列を使ってワールド空間のレイを生成
		// 2. 全オブジェクトとの交差判定
		// 3. 最も近いオブジェクトを選択
	}

	void ModelManipulator::DrawDuplicateWindow()
	{
#ifdef USE_IMGUI
		static Vector3 offset = { 1.0f, 0.0f, 0.0f };
		static int count = 1;
		static bool keepParent = false;

		ImGui::Text("オブジェクト複製");
		ImGui::Separator();

		auto* obj = objectManager_->GetObjectById(selectedObjectId_);
		if (obj) {
			ImGui::Text("複製対象: %s (ID: %d)", obj->modelName.c_str(), obj->id);

			ImGui::DragInt("複製数", &count, 1, 1, 50);
			ImGui::DragFloat3("オフセット", &offset.x, 0.1f);
			ImGui::Checkbox("親子関係を保持", &keepParent);

			if (ImGui::Button("複製実行")) {
				DuplicateObject(selectedObjectId_, count, offset, keepParent);
			}
		} else {
			ImGui::Text("オブジェクトを選択してください");
		}

		ImGui::Separator();
		if (ImGui::Button("閉じる")) {
			showDuplicateWindow_ = false;
		}
#endif // _DEBUG
	}

	void ModelManipulator::DuplicateObject(int objectId, int count, const Vector3& offset, bool keepParent)
	{
		auto* original = objectManager_->GetObjectById(objectId);
		if (!original) return;

		for (int i = 0; i < count; ++i) {
			Vector3 positionOffset = offset * static_cast<float>(i + 1);
			auto* duplicate = objectManager_->DuplicateObject(objectId, positionOffset);

			if (duplicate && !keepParent) {
				objectManager_->ClearParent(duplicate->id);
			}
		}

		std::cout << "Duplicated " << count << " objects successfully" << std::endl;
	}

	void ModelManipulator::DrawPrefabWindow()
	{
#ifdef USE_IMGUI
		ImGui::Text("プレファブシステム");
		ImGui::Separator();

		// プレファブ作成
		if (ImGui::CollapsingHeader("プレファブ作成")) {
			static char prefabName[64] = "";
			ImGui::InputText("プレファブ名", prefabName, sizeof(prefabName));

			if (ImGui::Button("選択オブジェクトからプレファブ作成") && strlen(prefabName) > 0) {
				if (IsValidObjectId(selectedObjectId_)) {
					CreatePrefab(prefabName, selectedObjectId_);
					memset(prefabName, 0, sizeof(prefabName));
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("全オブジェクトからプレファブ作成") && strlen(prefabName) > 0) {
				CreatePrefabFromAllObjects(prefabName);
				memset(prefabName, 0, sizeof(prefabName));
			}
		}

		// プレファブ読み込み
		if (ImGui::CollapsingHeader("プレファブ読み込み")) {
			if (ImGui::Button("プレファブ更新")) {
				ScanPrefabFolder();
			}

			if (ImGui::BeginListBox("##PrefabList", ImVec2(-1, 150))) {
				for (const auto& prefab : prefabList_) {
					if (ImGui::Selectable(prefab.c_str(), currentPrefabName_ == prefab)) {
						currentPrefabName_ = prefab;
					}
				}
				ImGui::EndListBox();
			}

			if (!currentPrefabName_.empty()) {
				if (ImGui::Button("プレファブ配置")) {
					LoadPrefab(currentPrefabName_);
				}
				ImGui::SameLine();
				if (ImGui::Button("プレファブ削除")) {
					DeletePrefab(currentPrefabName_);
				}
			}
		}

		ImGui::Separator();
		if (ImGui::Button("閉じる")) {
			showPrefabWindow_ = false;
		}
#endif // _DEBUG
	}

	void ModelManipulator::ScanPrefabFolder()
	{
		prefabList_.clear();
		std::string prefabPath = "Resources/Json/Prefabs/";

		if (!std::filesystem::exists(prefabPath)) {
			std::filesystem::create_directories(prefabPath);
			return;
		}

		for (const auto& entry : std::filesystem::directory_iterator(prefabPath)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				prefabList_.push_back(entry.path().stem().string());
			}
		}
	}

	void ModelManipulator::CreatePrefab(const std::string& name, int objectId)
	{
		auto* obj = objectManager_->GetObjectById(objectId);
		if (!obj) return;

		std::vector<ObjectManager::PlacedObject*> prefabObjects;
		objectManager_->CollectObjectHierarchy(objectId, prefabObjects);

		std::string filePath = "Resources/Json/Prefabs/" + name + ".json";
		SavePrefabToFile(prefabObjects, filePath);

		ScanPrefabFolder();
		std::cout << "Prefab created: " << name << std::endl;
	}

	void ModelManipulator::CreatePrefabFromAllObjects(const std::string& name)
	{
		auto objects = objectManager_->GetAllActiveObjects();
		if (objects.empty()) return;

		std::string filePath = "Resources/Json/Prefabs/" + name + ".json";
		SavePrefabToFile(objects, filePath);

		ScanPrefabFolder();
		std::cout << "Prefab created from all objects: " << name << std::endl;
	}

	void ModelManipulator::LoadPrefab(const std::string& name)
	{
		std::string filePath = "Resources/Json/Prefabs/" + name + ".json";

		try {
			std::ifstream file(filePath);
			if (!file.is_open()) return;

			json j;
			file >> j;

			std::unordered_map<int, int> idMapping;

			for (const auto& objData : j["objects"]) {
				bool isAnimation = objData.value("isAnimation", false);
				std::string animationName = objData.value("animationName", "");

				auto* obj = objectManager_->CreateObject(
					objData["filePath"].get<std::string>(),
					isAnimation,
					animationName
				);

				if (!obj) continue;

				int oldId = objData["id"];
				idMapping[oldId] = obj->id;

				obj->position = { objData["position"][0], objData["position"][1], objData["position"][2] };
				obj->rotation = { objData["rotate"][0], objData["rotate"][1], objData["rotate"][2] };
				obj->scale = { objData["scale"][0], objData["scale"][1], objData["scale"][2] };

				if (objData.contains("parentID") && objData["parentID"] != -1) {
					obj->parentID = objData["parentID"];
				}
			}

			// 親子関係を再マッピング
			auto objects = objectManager_->GetAllActiveObjects();
			for (auto* obj : objects) {
				if (obj->parentID != -1 && idMapping.find(obj->parentID) != idMapping.end()) {
					objectManager_->SetParent(obj->id, idMapping[obj->parentID]);
				}
				objectManager_->UpdateObjectTransform(*obj);
			}

			std::cout << "Prefab loaded: " << name << std::endl;
		}
		catch (const std::exception& e) {
			std::cout << "Error loading prefab: " << e.what() << std::endl;
		}
	}

	void ModelManipulator::DeletePrefab(const std::string& name)
	{
		std::string filePath = "Resources/Json/Prefabs/" + name + ".json";

		if (std::filesystem::exists(filePath)) {
			std::filesystem::remove(filePath);
			ScanPrefabFolder();
			currentPrefabName_ = "";
			std::cout << "Prefab deleted: " << name << std::endl;
		}
	}

	void ModelManipulator::SavePrefabToFile(const std::vector<ObjectManager::PlacedObject*>& objects,
		const std::string& filePath)
	{
		try {
			json j;
			j["version"] = 1;
			j["objects"] = json::array();

			for (const auto* obj : objects) {
				if (!obj) continue;

				json objData;
				objData["id"] = obj->id;
				objData["filePath"] = obj->modelPath;
				objData["modelName"] = obj->modelName;
				objData["position"] = { obj->position.x, obj->position.y, obj->position.z };
				objData["rotate"] = { obj->rotation.x, obj->rotation.y, obj->rotation.z };
				objData["scale"] = { obj->scale.x, obj->scale.y, obj->scale.z };
				objData["parentID"] = obj->parentID;
				objData["isAnimation"] = obj->isAnimation;
				objData["animationName"] = obj->animationName;
				j["objects"].push_back(objData);
			}

			std::ofstream file(filePath);
			file << j.dump(4);
			file.close();
		}
		catch (const std::exception& e) {
			std::cout << "Error saving prefab: " << e.what() << std::endl;
		}
	}

	bool ModelManipulator::IsValidObjectId(int id) const
	{
		return objectManager_->GetObjectById(id) != nullptr;
	}
}