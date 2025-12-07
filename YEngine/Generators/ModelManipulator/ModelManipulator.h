#pragma once

// C++
#include <wrl.h>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <numbers>

// Engine
#include <DirectXCommon.h>
#include <Object3D/ObjectManager.h>
#include <Systems/Camera/Camera.h>
#include "Systems/Input/Input.h"

// Math
#include "Vector3.h"
#include "Matrix4x4.h"

// ImGui
#ifdef USE_IMGUI
#include "imgui.h"
#include "ImGuizmo.h"
#endif

/// <summary>
/// シーンエディタ―クラス
/// </summary>
namespace YoRigine {
	class ModelManipulator
	{
	public:
		///************************* 基本関数 *************************///

		static ModelManipulator* GetInstance();

		void Initialize(const std::string& sceneName);
		void Update();
		void Draw();
		void DrawShadow();
		void DrawImGui();
		void DrawGizmo();
		void Finalize();

		void SetCamera(Camera* camera) { camera_ = camera; }

	private:
		///************************* エディタUI *************************///

		void ScanModelFolder();
		void DrawModelSelector();
		void DrawObjectList();
		void DrawTransformControls();
		void DrawDuplicateWindow();
		void DrawPrefabWindow();
		void ImGuiMenuBar();

		///************************* モデル操作 *************************///

		void PlaceObject(const std::string& modelPath);
		void DeleteSelectedObject();
		void ClearAllObjects();

		///************************* ギズモ操作 *************************///

		void ExtractTransformFromMatrix(const Matrix4x4& matrix, ObjectManager::PlacedObject& obj);

		void MatrixToImGuizmo(const Matrix4x4& matrix, float* imguizmoMatrix);
		Matrix4x4 ImGuizmoToMatrix(const float* imguizmoMatrix);

		///************************* 複製機能 *************************///

		void DuplicateObject(int objectId, int count, const Vector3& offset, bool keepParent);

		///************************* プレファブ *************************///

		void ScanPrefabFolder();
		void CreatePrefab(const std::string& name, int objectId);
		void CreatePrefabFromAllObjects(const std::string& name);
		void LoadPrefab(const std::string& name);
		void DeletePrefab(const std::string& name);
		void SavePrefabToFile(const std::vector<ObjectManager::PlacedObject*>& objects, const std::string& filePath);

		///************************* ファイルIO *************************///

		bool SaveScene(const std::string& filePath);
		bool LoadScene(const std::string& filePath);

		///************************* ヘルパー *************************///

		void FilterModels();
		bool IsValidObjectId(int id) const;
		bool IsValidModelIndex(int index) const {
			return index >= 0 && index < static_cast<int>(modelFiles_.size());
		}

		// 度数法とラジアンの変換
		float DegToRad(float deg) const { return deg * (3.14159265359f / 180.0f); }
		float RadToDeg(float rad) const { return rad * (180.0f / 3.14159265359f); }

		///************************* マウス操作 *************************///

		void HandleMouse();
		void PerformRaycast(float normalizedX, float normalizedY);

	private:
		ModelManipulator() = default;
		~ModelManipulator() = default;
		ModelManipulator(const ModelManipulator&) = delete;
		ModelManipulator& operator=(const ModelManipulator&) = delete;
		ModelManipulator(ModelManipulator&&) = delete;
		ModelManipulator& operator=(ModelManipulator&&) = delete;

		static ModelManipulator* instance_;

		///************************* メンバ変数 *************************///

		Camera* camera_ = nullptr;
		ObjectManager* objectManager_ = nullptr;
		bool isInitialized_ = false;

		// 現在選択中のオブジェクトID
		int selectedObjectId_ = -1;

		// モデルファイルリスト
		std::vector<std::string> modelFiles_;
		std::vector<std::string> modelNames_;
		std::string modelFolderPath_ = "Resources/Models/";
		std::string jsonPath_ = "";

		// UI状態
		int selectedModelIndex_ = -1;
		bool showModelSelector_ = true;
		bool showObjectList_ = true;
		bool showTransformControls_ = true;
		bool showDuplicateWindow_ = false;
		bool showPrefabWindow_ = false;

#ifdef USE_IMGUI
		// ギズモ設定
		ImGuizmo::OPERATION currentGizmoOperation_ = ImGuizmo::TRANSLATE;
		ImGuizmo::MODE currentGizmoMode_ = ImGuizmo::WORLD;
#endif

		// スナップ設定
		bool useSnap_ = false;
		float snapValues_[3] = { 1.0f, 1.0f, 1.0f };
		float rotationSnapDeg_ = 15.0f;

		// フィルタリング
		char modelSearchBuffer_[256] = "";
		std::vector<int> filteredModelIndices_;
		bool showSearchBar_ = false;

		// プレファブ
		std::string currentPrefabName_ = "";
		std::vector<std::string> prefabList_;

		// マウス選択
		bool isMouseSelecting_ = true;
		bool wasMousePressed_ = false;
	};
}