#pragma once

// Engine
#include <DirectXCommon.h>
#include "GPUEmitter.h"
#include <Systems/Camera/Camera.h>
#include <GPUParticle/GpuParticleParams.h>
// Math
#include <Vector3.h>

// C++
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

// JSON
#include <json.hpp>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // _DEBUG


/// <summary>
/// GPUパーティクルエミッター管理クラス
/// </summary>
namespace YoRigine {
	class GpuEmitManager
	{
	public:
		// エミッターデータ構造体
		struct EmitterData
		{
			std::string name;
			std::unique_ptr<GPUEmitter> emitter;
			EmitterShape shape;
			bool isActive;
			std::string texturePath;

			// 各形状のパラメータ
			struct SphereParams {
				Vector3 translate = { 0.0f, 0.0f, 0.0f };
				float radius = 10.0f;
				float count = 100.0f;
				float emitInterval = 1.0f;
			} sphereParams;

			struct BoxParams {
				Vector3 translate = { 0.0f, 0.0f, 0.0f };
				Vector3 size = { 10.0f, 10.0f, 10.0f };
				float count = 100.0f;
				float emitInterval = 1.0f;
			} boxParams;

			struct TriangleParams {
				Vector3 v1 = { -5.0f, 0.0f, 0.0f };
				Vector3 v2 = { 5.0f, 0.0f, 0.0f };
				Vector3 v3 = { 0.0f, 5.0f, 0.0f };
				Vector3 translate = { 0.0f, 0.0f, 0.0f };
				float count = 100.0f;
				float emitInterval = 1.0f;
			} triangleParams;

			struct ConeParams {
				Vector3 translate = { 0.0f, 0.0f, 0.0f };
				Vector3 direction = { 0.0f, 1.0f, 0.0f };
				float radius = 10.0f;
				float height = 20.0f;
				float count = 100.0f;
				float emitInterval = 1.0f;
			} coneParams;

			struct MeshParams {
				Model* model = nullptr;         // 使用するモデル (UIから指定する)
				Vector3 translate = { 0,0,0 };
				Vector3 scale = { 1,1,1 };
				Quaternion rotation = { 0,0,0,1 };
				float count = 100.0f;
				float emitInterval = 1.0f;
				MeshEmitMode emitMode = MeshEmitMode::Surface;
			} meshParams;

			// パーティクルパラメータ
			struct ParticleParams {
				// 生存時間
				float lifeTime = 3.0f;
				float lifeTimeVariance = 0.5f;

				// スケール
				Vector3 scale = { 1.0f, 1.0f, 1.0f };
				Vector3 scaleVariance = { 0.3f, 0.3f, 0.3f };

				// 回転
				float rotation = 0.0f;
				float rotationVariance = 0.0f;
				float rotationSpeed = 0.0f;
				float rotationSpeedVariance = 0.0f;

				// 速度
				Vector3 velocity = { 0.0f, 0.1f, 0.0f };
				Vector3 velocityVariance = { 0.1f, 0.05f, 0.1f };

				// 色
				Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
				Vector4 colorVariance = { 0.0f, 0.0f, 0.0f, 0.0f };

				// ビルボード
				bool isBillboard = true;
			} particleParams;

			TrailParams trailParams;
		};

		/// エミッターグループ（システム）データ構造体
		struct EmitterGroup
		{
			std::string name;

			bool isPlaying = false;           // 現在再生中かどうか
			float currentTime = 0.0f;         // 現在の再生時間
			float systemDuration = 0.0f;      // システムの総再生時間（0以下で無限ループ）

			// グループ全体の制御
			Vector3 translate = { 0.0f, 0.0f, 0.0f };	// グループ全体の原点移動
			bool isActive = true;						// グループ全体を有効/無効にするフラグ

			// このグループに属する全てのエミッターのコンテナ
			std::unordered_map<std::string, std::unique_ptr<EmitterData>> emitters;
		};

	public:
		///************************* 基本的な関数 *************************///
		static GpuEmitManager* GetInstance();
		void Initialize(Camera* camera);
		void Update();
		void Draw();
		void UpdateParticleParams(EmitterData* emitterData);
		void DrawImGui();
		bool DrawParticleParametersEditor(EmitterData* emitterData);

		// エミッター管理
		EmitterData* CreateEmitter(const std::string& groupName, const std::string& emitterName, std::string& texturePath, EmitterShape shape = EmitterShape::Sphere);
		void DeleteEmitter(const std::string& groupName, const std::string& emitterName);
		void DeleteAllEmitters();

		///************************* エミッターグループ管理 *************************///

		// グループ取得
		EmitterGroup* GetGroup(const std::string& groupName);

		// グループの作成：削除
		EmitterGroup* CreateEmitterGroup(const std::string& groupName);
		void DeleteEmitterGroup(const std::string& groupName);
		void DeleteAllEmitterGroups();

		// グループの再生・停止制御
		void PlayEmitterGroup(const std::string& groupName);
		void StopEmitterGroup(const std::string& groupName);

	public:
		///************************* アクセッサ *************************///
		EmitterData* GetEmitter(const std::string& groupName, const std::string& emitterName);
		std::vector<std::string> GetEmitterNames() const;
		bool HasEmitter(const std::string& emitterName) const;

	private:
		///************************* 内部処理 *************************///

		// シングルトンパターン
		GpuEmitManager() = default;
		~GpuEmitManager() = default;
		GpuEmitManager(const GpuEmitManager&) = delete;
		GpuEmitManager& operator=(const GpuEmitManager&) = delete;

		bool DrawShapeEditor(EmitterData* emitterData);
		bool DrawSphereEditor(EmitterData* emitterData);
		bool DrawBoxEditor(EmitterData* emitterData);
		bool DrawTriangleEditor(EmitterData* emitterData);
		bool DrawConeEditor(EmitterData* emitterData);
		bool DrawMeshEditor(EmitterData* emitterData);

		void DrawGroupManagementTab();
		void DrawEmitterManagementTab();
		void DrawTextureBrowser(bool& isOpen);
		void DrawEditorTab();
		void DrawDeleteDialog();

		// エミッターパラメータ更新
		void UpdateEmitterParams(EmitterData* emitterData);


	private:
		///************************* ImGui : Json *************************///

		// JSON保存・読み込み
		bool SaveToFile(const std::string& filepath);
		bool LoadFromFile(const std::string& filepath);
		// JSON変換
		nlohmann::json ToJson() const;
		bool FromJson(const nlohmann::json& json);
		bool LoadEmitterFromJson(const std::string& groupName, const nlohmann::json& j);
		void ScanTextureDirectory(const std::string& directory);
		void ScanJsonDirectory(const std::string& directory);
	private:
		///************************* メンバ変数 *************************///
		Camera* camera_ = nullptr;
		std::unordered_map<std::string, std::unique_ptr<EmitterGroup>> groups_;

		// ImGui用変数
		char newEmitterName_[256] = "";
		std::string selectedEmitterName_ = "";
		char newEmitterTexturePath_[260] = "";
		// エミッター形状の名前配列
		static const char* shapeNames_[];
		char newGroupName_[256] = "";       // ★ 新規グループ名入力用
		std::string selectedGroupName_ = ""; // ★ 選択中のグループ名


		int selectedShapeIndex_ = 0;
		bool showCreateDialog_ = false;
		bool showDeleteDialog_ = false;
		char saveFilePath_[256] = "Resources/Json/GpuEmitters/emitters.json";
		char loadFilePath_[256] = "Resources/Json/GpuEmitters/emitters.json";


		std::vector<std::string> availableTextures_;
		std::vector<std::string> availableFolders_;
		bool showTextureBrowser_ = false;
		char textureFolder_[260] = "Resources/Textures/";
		std::string currentTextureDir_;

		std::string currentJsonDir_ = "Resources/Json/GpuEmitters/";	// 現在スキャン中のJSONディレクトリ
		std::vector<std::string> availableJsonFiles_;					// 見つかったJSONファイル名一覧
		bool shouldRescanJson_ = true;									// JSONディレクトリを再スキャンするかどうか
	};
}