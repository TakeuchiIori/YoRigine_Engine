#pragma once

// Engine
#include <DirectXCommon.h>
#include <PipelineManager/PipelineManager.h>
#include <ComputeShaderManager/ComputeShaderManager.h>
#include <Systems/Camera/Camera.h>
#include <Mesh/Mesh.h>
#include <Mesh/MeshPrimitive.h>

// Math
#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>
#include <Matrix4x4.h>

// C++
#include <wrl.h>

/// <summary>
/// GPUパーティクルクラス
/// </summary>
class GPUParticle
{
public:
	// 定数
	static const uint32_t kMaxParticles = 2000000;		  // 最大パーティクル数
	static const uint32_t kParticlesPerThread = 128;		  // 1スレッド辺りの処理数
	static const uint32_t kThreadsPerGroup = 1024;		  // 1グループ当たりのスレッド数

	// 必要なスレッドグループ数を取得
	static constexpr uint32_t GetRequiredThreadGroups() {
		constexpr uint32_t particlesPerGroup = kThreadsPerGroup * kParticlesPerThread;
		return (kMaxParticles + particlesPerGroup - 1) / particlesPerGroup;
	}

	// 統計情報構造体
	struct ParticleStats {
		uint32_t maxParticles = kMaxParticles;
		int32_t freeListIndex = 0;
		uint32_t freeCount = 0;
		uint32_t activeCount = 0;
		float usagePercent = 0.0f;
		bool isValid = false;
	};
	// 統計情報取得（非同期・軽量版）
	ParticleStats GetCachedStats() const { return cachedStats_; }  // キャッシュ取得

	///************************* GPUバッファ用の構造体 *************************///
	struct ParticleCSForGPU {
		Vector3 translate;
		Vector3 scale;
		float rotation;

		float lifeTime;
		float currentTime;

		Vector3 velocity;
		Vector4 color;
		uint32_t isBillBoard;
		uint32_t isActive;
	};

	struct PerViewForGPU {
		Matrix4x4 viewProjection;
		Matrix4x4 billboardMatrix;
	};

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;
		float intensity;
		float padding[3];
	};

public:
	///************************* 基本関数 *************************///
	GPUParticle() = default;
	~GPUParticle() = default;

	void Initialize(const std::string& filepath, Camera* camera);
	void Update(ID3D12Resource* resource);
	void Draw();
	void Reset();

#ifdef USE_IMGUI
	void DrawStatsImGui();
#endif

public:
	///************************* アクセッサ *************************///
	uint32_t GetMaxParticles() const { return kMaxParticles; }
	uint32_t GetParticlesPerThread() const { return kParticlesPerThread; }
	uint32_t GetRequiredThreads() const { return (kMaxParticles + kParticlesPerThread - 1) / kParticlesPerThread; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleUavHandleGPU() const { return particleUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleSrvHandleGPU() const { return particleSrvHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetFreeListIndexUavHandleGPU() const { return freeListIndexUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetFreeListUavHandleGPU() const { return freeListUavHandleGPU_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetActiveCountUavHandleGPU() const { return activeCountUavHandleGPU_; }

	ID3D12Resource* GetParticleResource() const { return particleResource_.Get(); }
	ID3D12Resource* GetFreeListIndexResource() const { return freeListIndexResource_.Get(); }
	ID3D12Resource* GetFreeListResource() const { return freeListResource_.Get(); }
	ID3D12Resource* GetActiveCountResource() const { return activeCountResource_.Get(); }
	ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
	ID3D12Resource* GetLightResource() const { return lightResource_.Get(); }
	ID3D12Resource* GetPerViewResource() const { return perViewResource_.Get(); }
	ID3D12Resource* GetVertexResource() const { return mesh_->GetMeshResource().vertexResource.Get(); }

	Material* GetMaterialData() const { return materialData_; }
	DirectionalLight* GetLightData() const { return lightData_; }
	PerViewForGPU* GetPerViewData() const { return perViewData_; }

	void SetMesh(std::shared_ptr<Mesh> mesh) { mesh_ = mesh; }

private:
	///************************* 内部処理 *************************///
	void CreateMaterialResource();
	void CreateLightResource();
	void CreateGPUParticleResource();
	void CreatePerViewResource();
	void CreateVertexResource();

	void CreateUAV();
	void CreateTexture();

	void UpdateMaterial();
	void UpdateLight();
	void UpdatePerView();

	void DispatchInit();
	void DispatchUpdate(ID3D12Resource* resource);

private:
	///************************* メンバ変数 *************************///
	DirectXCommon* dxCommon_;
	PipelineManager* pipelineManager_;
	ComputeShaderManager* computeShaderManager_;
	Camera* camera_;

	// データポインタ
	Material* materialData_;
	DirectionalLight* lightData_;
	PerViewForGPU* perViewData_;

	// リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> activeCountResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> activeCountReadback_;

	// SRV/UAVハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE freeListIndexUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE freeListUavHandleGPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE activeCountUavHandleGPU_;

	// SRV/UAV/Texture/Counterインデックス
	uint32_t srvIndex_;
	uint32_t uavIndex_;
	uint32_t freeListIndexUavIndex_;
	uint32_t freeListUavIndex_;
	uint32_t textureIndexSRV_;
	uint32_t activeCountUavIndex_ = 0;
	uint32_t cachedActiveCount_ = 0;


	std::string textureFilePath_;

	// Mesh
	std::shared_ptr<Mesh> mesh_;
	BlendMode blendMode_ = BlendMode::kBlendModeAdd;



	// 統計情報用
	ParticleStats cachedStats_{};
	bool statsInitialized_ = false;
};