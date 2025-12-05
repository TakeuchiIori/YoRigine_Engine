#include "GPUEmitter.h"

// Engine
#include <Systems/GameTime/GameTime.h>
#include <ComputeShaderManager/ComputeShaderManager.h>
#include <ModelUtils.h>
#include "GpuParticleMethod.h"

/// <summary>
/// GPU エミッターの初期化（パーティクル生成・各種リソース作成）
/// </summary>
void GPUEmitter::Initialize(Camera* camera, std::string& texturePath)
{
	camera_ = camera;

	// GPU パーティクル本体
	gpuParticle_ = std::make_unique<GPUParticle>();
	gpuParticle_->Initialize(texturePath, camera_);

	CreateEmitterResources();
	CreatePerFrameResource();
	CreateParticleParametersResource();
	CreateMeshTriangleBuffer();

	ParticleParameters defaultParams{};
	defaultParams.lifeTime = 3.0f;
	defaultParams.lifeTimeVariance = 0.5f;
	defaultParams.scale = Vector3(1.0f, 1.0f, 1.0f);
	defaultParams.scaleVariance = Vector3(0.3f, 0.3f, 0.3f);
	defaultParams.rotation = 0.0f;
	defaultParams.rotationVariance = 0.0f;
	defaultParams.rotationSpeed = 0.0f;
	defaultParams.rotationSpeedVariance = 0.0f;
	defaultParams.velocity = Vector3(0.0f, 0.1f, 0.0f);
	defaultParams.velocityVariance = Vector3(0.1f, 0.05f, 0.1f);
	defaultParams.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	defaultParams.colorVariance = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	defaultParams.isBillboard = 1;
	SetParticleParameters(defaultParams);

	//-----------------------------------------
	// デフォルト形状（Cone）を設定
	//-----------------------------------------
	SetEmitterShape(EmitterShape::Cone);
	SetConeParams(
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.0f, 1.0f, 0.0f),
		10.0f,
		20.0f,
		100.0f,
		1.0f
	);
}

/// <summary>
/// エミッター更新処理（形状ごとに emit を制御）
/// </summary>
void GPUEmitter::Update()
{
	//-----------------------------------------
	// 共通データの更新
	//-----------------------------------------
	emitterCommonData_->emitterShape = static_cast<uint32_t>(currentShape_);
	perframeData_->time = YoRigine::GameTime::GetTotalTime();
	perframeData_->deltaTime = YoRigine::GameTime::GetUnscaledDeltaTime();

	// エミッターの更新
	UpdateEmitters();

	// トレイルの更新
	UpdateTrail();

	//-----------------------------------------
	// パーティクルのレンダリング更新
	//-----------------------------------------
	gpuParticle_->Update(perframeResource_.Get());

	//-----------------------------------------
	// ComputeShader 実行
	//-----------------------------------------
	Dispatch();
}

/// <summary>
/// GPU パーティクル描画
/// </summary>
void GPUEmitter::Draw()
{
	gpuParticle_->Draw();
}

void GPUEmitter::Reset()
{
	if (gpuParticle_) {
		gpuParticle_->Reset();
	}

	timeScalelastEmit_ = 0.0f;
}

/// <summary>
/// 使用するエミッター形状を変更
/// </summary>
void GPUEmitter::SetEmitterShape(EmitterShape shape)
{
	currentShape_ = shape;
}

/// <summary>
/// Sphere パラメータ設定
/// </summary>
void GPUEmitter::SetSphereParams(const Vector3& translate, float radius, float count, float emitInterval)
{
	if (!emitterSphereData_) return;

	emitterSphereData_->translate = translate;
	emitterSphereData_->radius = radius;
	emitterSphereData_->count = count;
	emitterSphereData_->emitInterval = emitInterval;
	emitterSphereData_->intervalTime = 0.0f;
	emitterSphereData_->isEmit = 1;
}

/// <summary>
/// Box パラメータ設定
/// </summary>
void GPUEmitter::SetBoxParams(const Vector3& translate, const Vector3& size, float count, float emitInterval)
{
	if (!emitterBoxData_) return;

	emitterBoxData_->translate = translate;
	emitterBoxData_->size = size;
	emitterBoxData_->count = count;
	emitterBoxData_->emitInterval = emitInterval;
	emitterBoxData_->intervalTime = 0.0f;
	emitterBoxData_->isEmit = 1;
}

/// <summary>
/// Triangle パラメータ設定
/// </summary>
void GPUEmitter::SetTriangleParams(const Vector3& v1, const Vector3& v2, const Vector3& v3,
	const Vector3& translate, float count, float emitInterval)
{
	if (!emitterTriangleData_) return;

	emitterTriangleData_->v1 = v1;
	emitterTriangleData_->v2 = v2;
	emitterTriangleData_->v3 = v3;
	emitterTriangleData_->translate = translate;
	emitterTriangleData_->count = count;
	emitterTriangleData_->emitInterval = emitInterval;
	emitterTriangleData_->intervalTime = 0.0f;
	emitterTriangleData_->isEmit = 1;
}

/// <summary>
/// Cone パラメータ設定
/// </summary>
void GPUEmitter::SetConeParams(const Vector3& translate, const Vector3& direction, float radius,
	float height, float count, float emitInterval)
{
	if (!emitterConeData_) return;

	emitterConeData_->translate = translate;
	emitterConeData_->direction = direction;
	emitterConeData_->radius = radius;
	emitterConeData_->height = height;
	emitterConeData_->count = count;
	emitterConeData_->emitInterval = emitInterval;
	emitterConeData_->intervalTime = 0.0f;
	emitterConeData_->isEmit = 1;
}

void GPUEmitter::SetMeshParams(Model* model, const Vector3& translate, const Vector3& scale, const Quaternion& rotation, float count, float emitInterval, MeshEmitMode mode)
{
	if (!emitterMeshData_ || !model) return;

	currentMeshModel_ = model;
	currentMeshMode_ = mode;

	emitterMeshData_->translate = translate;
	emitterMeshData_->scale = scale;
	emitterMeshData_->rotation = Vector4(rotation.x, rotation.y, rotation.z, rotation.w);
	emitterMeshData_->count = count;
	emitterMeshData_->emitInterval = emitInterval;
	emitterMeshData_->intervalTime = 0.0f;
	emitterMeshData_->isEmit = 1;
	emitterMeshData_->emitMode = static_cast<uint32_t>(mode);

	// メッシュの三角形データを更新
	UpdateMeshTriangleData(model);
	emitterMeshData_->triangleCount = static_cast<uint32_t>(meshTriangles_.size());
}

//-----------------------------------------
// UpdateXXXParams は初期化時との差分だけ更新
//-----------------------------------------

void GPUEmitter::UpdateSphereParams(const Vector3& translate, float radius, float count, float emitInterval)
{
	if (!emitterSphereData_) return;

	emitterSphereData_->translate = translate;
	emitterSphereData_->radius = radius;
	emitterSphereData_->count = count;
	emitterSphereData_->emitInterval = emitInterval;
}

void GPUEmitter::UpdateBoxParams(const Vector3& translate, const Vector3& size, float count, float emitInterval)
{
	if (!emitterBoxData_) return;

	emitterBoxData_->translate = translate;
	emitterBoxData_->size = size;
	emitterBoxData_->count = count;
	emitterBoxData_->emitInterval = emitInterval;
}

void GPUEmitter::UpdateTriangleParams(const Vector3& v1, const Vector3& v2, const Vector3& v3,
	const Vector3& translate, float count, float emitInterval)
{
	if (!emitterTriangleData_) return;

	emitterTriangleData_->v1 = v1;
	emitterTriangleData_->v2 = v2;
	emitterTriangleData_->v3 = v3;
	emitterTriangleData_->translate = translate;
	emitterTriangleData_->count = count;
	emitterTriangleData_->emitInterval = emitInterval;
}

void GPUEmitter::UpdateConeParams(const Vector3& translate, const Vector3& direction, float radius,
	float height, float count, float emitInterval)
{
	if (!emitterConeData_) return;

	emitterConeData_->translate = translate;
	emitterConeData_->direction = direction;
	emitterConeData_->radius = radius;
	emitterConeData_->height = height;
	emitterConeData_->count = count;
	emitterConeData_->emitInterval = emitInterval;
}

void GPUEmitter::UpdateMeshParams(Model* model, const Vector3& translate, const Vector3& scale, const Quaternion& rotation, float count, float emitInterval, MeshEmitMode mode)
{
	if (!emitterMeshData_) return;

	if (model != currentMeshModel_) {
		// モデルが変わった場合は再設定
		SetMeshParams(model, translate, scale, rotation, count, emitInterval, mode);
		return;
	}

	emitterMeshData_->translate = translate;
	emitterMeshData_->scale = scale;
	emitterMeshData_->rotation = Vector4(rotation.x, rotation.y, rotation.z, rotation.w);
	emitterMeshData_->count = count;
	emitterMeshData_->emitInterval = emitInterval;
	emitterMeshData_->emitMode = static_cast<uint32_t>(mode);
	currentMeshMode_ = mode;
}

void GPUEmitter::SetParticleParameters(const ParticleParameters& params)
{
	if (particleParameters_) {
		*particleParameters_ = params;
	}
}

void GPUEmitter::SetLifeTime(float lifeTime, float variance)
{
	if (particleParameters_) {
		particleParameters_->lifeTime = lifeTime;
		particleParameters_->lifeTimeVariance = variance;
	}
}

void GPUEmitter::SetScale(const Vector3& scale, const Vector3& variance)
{
	if (particleParameters_) {
		particleParameters_->scale = scale;
		particleParameters_->scaleVariance = variance;
	}
}

void GPUEmitter::SetRotation(float rotation, float variance, float speed, float speedVariance)
{
	if (particleParameters_) {
		particleParameters_->rotation = rotation;
		particleParameters_->rotationVariance = variance;
		particleParameters_->rotationSpeed = speed;
		particleParameters_->rotationSpeedVariance = speedVariance;
	}
}

void GPUEmitter::SetVelocity(const Vector3& velocity, const Vector3& variance)
{
	if (particleParameters_) {
		particleParameters_->velocity = velocity;
		particleParameters_->velocityVariance = variance;
	}
}

void GPUEmitter::SetColor(const Vector4& color, const Vector4& variance)
{
	if (particleParameters_) {
		particleParameters_->color = color;
		particleParameters_->colorVariance = variance;
	}
}

void GPUEmitter::SetBillboard(bool enabled)
{
	if (particleParameters_) {
		particleParameters_->isBillboard = enabled ? 1 : 0;
	}
}

void GPUEmitter::EmitAtPosition(const Vector3& position, float count)
{
	lastEmitWorldPos_ = position;
	hasLastEmitWorldPos_ = true;

	switch (currentShape_) {
	case EmitterShape::Sphere:
		emitterSphereData_->translate = position;
		emitterSphereData_->count = count;
		emitterSphereData_->isEmit = 1;
		break;

	case EmitterShape::Box:
		emitterBoxData_->translate = position;
		emitterBoxData_->count = count;
		emitterBoxData_->isEmit = 1;
		break;

	case EmitterShape::Triangle:
		emitterTriangleData_->translate = position;
		emitterTriangleData_->count = count;
		emitterTriangleData_->isEmit = 1;
		break;

	case EmitterShape::Cone:
		emitterConeData_->translate = position;
		emitterConeData_->count = count;
		emitterConeData_->isEmit = 1;
		break;

	case EmitterShape::Mesh:
		emitterMeshData_->translate = position;
		emitterMeshData_->count = count;
		emitterMeshData_->isEmit = 1;
		break;
	}
}

/// <summary>
/// 各エミッター形状の GPU リソース（ConstantBuffer）を生成
/// </summary>
void GPUEmitter::CreateEmitterResources()
{
	auto* dx = DirectXCommon::GetInstance();

	//-----------------------------------------
	// 共通データ
	//-----------------------------------------
	emitterCommonResource_ = dx->CreateBufferResource(sizeof(EmitterCommonData));
	emitterCommonResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterCommonData_));
	emitterCommonData_->emitterShape = static_cast<uint32_t>(EmitterShape::Sphere);

	//-----------------------------------------
	// Sphere
	//-----------------------------------------
	emitterSphereResource_ = dx->CreateBufferResource(sizeof(EmitterSphereData));
	emitterSphereResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterSphereData_));

	//-----------------------------------------
	// Box
	//-----------------------------------------
	emitterBoxResource_ = dx->CreateBufferResource(sizeof(EmitterBoxData));
	emitterBoxResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterBoxData_));

	//-----------------------------------------
	// Triangle
	//-----------------------------------------
	emitterTriangleResource_ = dx->CreateBufferResource(sizeof(EmitterTriangleData));
	emitterTriangleResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterTriangleData_));

	//-----------------------------------------
	// Cone
	//-----------------------------------------
	emitterConeResource_ = dx->CreateBufferResource(sizeof(EmitterConeData));
	emitterConeResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterConeData_));

	//-----------------------------------------
	// Mesh（新規追加）
	//-----------------------------------------
	emitterMeshResource_ = dx->CreateBufferResource(sizeof(EmitterMeshData));
	emitterMeshResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterMeshData_));

}

void GPUEmitter::CreateParticleParametersResource()
{
	auto* dx = DirectXCommon::GetInstance();
	particleParametersResource_ = dx->CreateBufferResource(sizeof(ParticleParameters));
	particleParametersResource_->Map(0, nullptr, reinterpret_cast<void**>(&particleParameters_));
}

/// <summary>
/// 毎フレーム用定数バッファ作成（時間・デルタ）
/// </summary>
void GPUEmitter::CreatePerFrameResource()
{
	auto* dx = DirectXCommon::GetInstance();

	perframeResource_ = dx->CreateBufferResource(sizeof(PerFrameData));
	perframeResource_->Map(0, nullptr, reinterpret_cast<void**>(&perframeData_));
}

void GPUEmitter::CreateMeshTriangleBuffer()
{
	auto* dx = DirectXCommon::GetInstance();

	// 最大三角形数分のバッファを確保
	size_t bufferSize = sizeof(MeshTriangle) * kMaxTriangles_;

	meshTriangleBuffer_ = dx->CreateBufferResource(bufferSize);
	meshTriangleBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&meshTriangleData_));

	// SRV作成
	auto* srvManager = SrvManager::GetInstance();
	meshTriangleBufferSrvIndex_ = srvManager->Allocate();

	srvManager->CreateSRVforStructuredBuffer(
		meshTriangleBufferSrvIndex_,
		meshTriangleBuffer_.Get(),
		static_cast<UINT>(kMaxTriangles_),
		sizeof(MeshTriangle)
	);
}
/// <summary>
/// モデルからメッシュ三角形情報を収集してGPUバッファへ転送
/// </summary>
void GPUEmitter::UpdateMeshTriangleData(Model* model)
{
	if (!model || !meshTriangleData_) {
		return;
	}

	meshTriangles_.clear();

	// エッジと、それを共有する三角形のインデックスリストのマップ
	// Key: エッジ(2頂点), Value: 三角形のインデックス(std::vector)
	std::map<EdgeKey, std::vector<size_t>> edgeMap;

	const auto& meshes = model->GetMeshes();

	// 1. 全三角形を走査してリスト化 & エッジ登録
	for (const auto& meshPtr : meshes)
	{
		if (!meshPtr) continue;

		const Mesh::MeshData& meshData = meshPtr->GetMeshData();
		const auto& vertices = meshData.vertices;
		const auto& indices = meshData.indices;

		size_t indexCount = indices.size();
		for (size_t i = 0; i + 2 < indexCount; i += 3)
		{
			MeshTriangle tri{};

			// 頂点取得
			auto p0 = vertices[indices[i + 0]].position;
			auto p1 = vertices[indices[i + 1]].position;
			auto p2 = vertices[indices[i + 2]].position;

			tri.v0 = { p0.x, p0.y, p0.z };
			tri.v1 = { p1.x, p1.y, p1.z };
			tri.v2 = { p2.x, p2.y, p2.z };

			// 法線と面積
			Vector3 edge1 = tri.v1 - tri.v0;
			Vector3 edge2 = tri.v2 - tri.v0;
			tri.normal = Normalize(Cross(edge1, edge2));
			tri.area = Length(Cross(edge1, edge2)) * 0.5f;

			// 初期状態は全エッジ有効 (ビットフラグ 111 = 7)
			tri.activeEdges = 7;

			// 現在の三角形インデックス
			size_t currentTriIndex = meshTriangles_.size();

			// エッジをマップに登録
			// (v0-v1), (v1-v2), (v2-v0) の3辺
			edgeMap[EdgeKey(tri.v0, tri.v1)].push_back(currentTriIndex);
			edgeMap[EdgeKey(tri.v1, tri.v2)].push_back(currentTriIndex);
			edgeMap[EdgeKey(tri.v2, tri.v0)].push_back(currentTriIndex);

			meshTriangles_.push_back(tri);

			if (meshTriangles_.size() >= kMaxTriangles_) break;
		}
		if (meshTriangles_.size() >= kMaxTriangles_) break;
	}

	// 2. マップを使って隣接判定（ここが高速化の肝）
	// マップを走査し、2つの三角形に共有されているエッジを探す
	for (const auto& pair : edgeMap)
	{
		const std::vector<size_t>& sharedTriangles = pair.second;

		// 同じエッジを共有する三角形が2つある場合（＝内部のエッジ）
		if (sharedTriangles.size() == 2)
		{
			size_t idxA = sharedTriangles[0];
			size_t idxB = sharedTriangles[1];

			MeshTriangle& triA = meshTriangles_[idxA];
			MeshTriangle& triB = meshTriangles_[idxB];

			// 法線の内積をとる (平行なら 1.0 に近くなる)
			float dot = Dot(triA.normal, triB.normal);

			// 法線がほぼ同じ向きなら「平面上の不要なエッジ」とみなす
			if (dot > 0.99f)
			{
				// エッジの座標情報
				const EdgeKey& key = pair.first; // ソート済みのp1, p2

				// triA のどのエッジを無効にするか特定
				// (EdgeKeyを使って比較するのは少し重いので、距離で判定)
				auto DisableEdge = [&](MeshTriangle& t, const EdgeKey& k) {
					// 各辺とキーが一致するかチェック
					if (EdgeKey(t.v0, t.v1).p1.x == k.p1.x /*簡易チェック...本来は完全比較*/) { /*...*/ }

					// 簡易実装: 3辺それぞれの中心点が、EdgeKeyの中点と一致するかで判定するのが楽
					Vector3 keyMid = { (k.p1.x + k.p2.x) * 0.5f, (k.p1.y + k.p2.y) * 0.5f, (k.p1.z + k.p2.z) * 0.5f };

					auto check = [&](Vector3 a, Vector3 b, int bit) {
						Vector3 edgeMid = (a + b) * 0.5f;
						float distSq = (edgeMid.x - keyMid.x) * (edgeMid.x - keyMid.x) +
							(edgeMid.y - keyMid.y) * (edgeMid.y - keyMid.y) +
							(edgeMid.z - keyMid.z) * (edgeMid.z - keyMid.z);
						if (distSq < 0.0001f) {
							t.activeEdges &= ~(1 << bit);
						}
						};

					check(t.v0, t.v1, 0);
					check(t.v1, t.v2, 1);
					check(t.v2, t.v0, 2);
					};

				DisableEdge(triA, key);
				DisableEdge(triB, key);
			}
		}
	}

	// GPUバッファへコピー
	if (!meshTriangles_.empty())
	{
		const size_t copySize = sizeof(MeshTriangle) * meshTriangles_.size();
		std::memcpy(meshTriangleData_, meshTriangles_.data(), copySize);
	}

	// エミッター側のカウント更新
	if (emitterMeshData_) {
		emitterMeshData_->triangleCount = static_cast<uint32_t>(meshTriangles_.size());
	}
}

/// <summary>
/// ComputeShader “EmitCS” を用いてパーティクルを生成
/// </summary>
void GPUEmitter::Dispatch()
{
	auto* dx = DirectXCommon::GetInstance();
	auto* cmd = dx->GetCommandList().Get();

	//-----------------------------------------
	// UAV バリア（書き込み準備）
	//-----------------------------------------
	dx->TransitionBarrier(
		gpuParticle_->GetParticleResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	dx->TransitionBarrier(
		gpuParticle_->GetFreeListIndexResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	dx->TransitionBarrier(
		gpuParticle_->GetFreeListResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	//-----------------------------------------
	// CS パイプライン設定
	//-----------------------------------------
	cmd->SetComputeRootSignature(ComputeShaderManager::GetInstance()->GetRootSignature("EmitCS"));
	cmd->SetPipelineState(ComputeShaderManager::GetInstance()->GetComputePipelineState("EmitCS"));

	ID3D12DescriptorHeap* heaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
	cmd->SetDescriptorHeaps(_countof(heaps), heaps);

	//-----------------------------------------
	// RootCBV 設定
	//-----------------------------------------
	cmd->SetComputeRootConstantBufferView(0, emitterCommonResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(1, emitterSphereResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(2, emitterBoxResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(3, emitterTriangleResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(4, emitterConeResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(5, emitterMeshResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(6, perframeResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(7, particleParametersResource_->GetGPUVirtualAddress());

	//-----------------------------------------
	// UAV テーブル
	//-----------------------------------------
	cmd->SetComputeRootDescriptorTable(8, gpuParticle_->GetParticleUavHandleGPU());
	cmd->SetComputeRootDescriptorTable(9, gpuParticle_->GetFreeListIndexUavHandleGPU());
	cmd->SetComputeRootDescriptorTable(10, gpuParticle_->GetFreeListUavHandleGPU());
	cmd->SetComputeRootDescriptorTable(11, gpuParticle_->GetActiveCountUavHandleGPU());

	//-----------------------------------------
	// Mesh用のSRV設定（新規追加）
	//-----------------------------------------
	if (currentShape_ == EmitterShape::Mesh) {
		auto* srvManager = SrvManager::GetInstance();
		cmd->SetComputeRootDescriptorTable(12,
			srvManager->GetGPUDescriptorHandle(meshTriangleBufferSrvIndex_));
	}

	//-----------------------------------------
	// Emit 数を形状ごとに決定
	//-----------------------------------------
	uint32_t emitCount = 0;

	switch (currentShape_)
	{
	case EmitterShape::Sphere:
		emitCount = static_cast<uint32_t>(emitterSphereData_->count);
		break;

	case EmitterShape::Box:
		emitCount = static_cast<uint32_t>(emitterBoxData_->count);
		break;

	case EmitterShape::Triangle:
		emitCount = static_cast<uint32_t>(emitterTriangleData_->count);
		break;

	case EmitterShape::Cone:
		emitCount = static_cast<uint32_t>(emitterConeData_->count);
		break;
	case EmitterShape::Mesh:
		emitCount = static_cast<uint32_t>(emitterMeshData_->count);
		break;
	}

	//-----------------------------------------
	// Dispatch（スレッドグループ計算）
	//-----------------------------------------
	uint32_t groupX = (emitCount + threadsPerGroup_ - 1) / threadsPerGroup_;
	if (groupX == 0) groupX = 1;

	cmd->Dispatch(groupX, 1, 1);

	//-----------------------------------------
	// UAV → VERTEX&CB に戻す
	//-----------------------------------------
	dx->TransitionBarrier(
		gpuParticle_->GetParticleResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dx->TransitionBarrier(
		gpuParticle_->GetFreeListIndexResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dx->TransitionBarrier(
		gpuParticle_->GetFreeListResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);


}

void GPUEmitter::UpdateEmitters()
{
	//-----------------------------------------
	// 現在の形状ごとに Emit 制御
	//-----------------------------------------
	switch (currentShape_)
	{
	case EmitterShape::Sphere:
		emitterSphereData_->intervalTime += YoRigine::GameTime::GetUnscaledDeltaTime();
		emitterSphereData_->isEmit =
			(emitterSphereData_->intervalTime >= emitterSphereData_->emitInterval);
		if (emitterSphereData_->isEmit) emitterSphereData_->intervalTime = 0.0f;
		break;

	case EmitterShape::Box:
		emitterBoxData_->intervalTime += YoRigine::GameTime::GetUnscaledDeltaTime();
		emitterBoxData_->isEmit =
			(emitterBoxData_->intervalTime >= emitterBoxData_->emitInterval);
		if (emitterBoxData_->isEmit) emitterBoxData_->intervalTime = 0.0f;
		break;

	case EmitterShape::Triangle:
		emitterTriangleData_->intervalTime += YoRigine::GameTime::GetUnscaledDeltaTime();
		emitterTriangleData_->isEmit =
			(emitterTriangleData_->intervalTime >= emitterTriangleData_->emitInterval);
		if (emitterTriangleData_->isEmit) emitterTriangleData_->intervalTime = 0.0f;
		break;

	case EmitterShape::Cone:
		emitterConeData_->intervalTime += YoRigine::GameTime::GetUnscaledDeltaTime();
		emitterConeData_->isEmit =
			(emitterConeData_->intervalTime >= emitterConeData_->emitInterval);
		if (emitterConeData_->isEmit) emitterConeData_->intervalTime = 0.0f;
		break;
	case EmitterShape::Mesh:
		emitterMeshData_->intervalTime += YoRigine::GameTime::GetUnscaledDeltaTime();
		emitterMeshData_->isEmit =
			(emitterMeshData_->intervalTime >= emitterMeshData_->emitInterval);
		if (emitterMeshData_->isEmit) emitterMeshData_->intervalTime = 0.0f;
		break;
	}
}
void GPUEmitter::UpdateTrail()
{
	// ================================
	// Trail の適用
	// ================================
	auto& trail = trail_;
	if (!trail.isTrail) return;

	// 今のエミッター位置
	Vector3 current = GetEmitterPosition();

	// まだ基準位置が無いなら「覚えるだけ」で終わり（出さない）
	if (!trailHasLast_) {
		trailLastPos_ = current;
		trailHasLast_ = true;
		return;
	}

	// 前回からどれくらい動いたか
	Vector3 delta = current - trailLastPos_;
	float dist = Length(delta);

	// 全く動いてないなら出さない
	if (dist <= 0.0f) {
		return;
	}

	// -------------------------------
	// しきい値 0 以下 → 「ちょっとでも動いたら出す」モード
	// -------------------------------
	if (trail.minDistance <= 0.0f) {
		EmitAtPosition(current, trail.emissionCount);
		trailLastPos_ = current;

		if (trail.lifeTime > 0.0f) {
			particleParameters_->lifeTime = trail.lifeTime;
		}
		return;
	}

	// -------------------------------
	// 通常モード：一定距離ごとに出す
	// -------------------------------
	if (dist < trail.minDistance) {
		// まだ閾値未満なので何もしない
		return;
	}

	// 高速移動対策：dist が大きい場合は補間して複数発生
	int steps = static_cast<int>(dist / trail.minDistance);
	if (steps < 1) {
		steps = 1;
	}

	Vector3 step = delta / static_cast<float>(steps);
	Vector3 pos = trailLastPos_;

	for (int i = 0; i < steps; ++i) {
		pos += step;
		EmitAtPosition(pos, trail.emissionCount);
	}

	trailLastPos_ = current;

	if (trail.lifeTime > 0.0f) {
		particleParameters_->lifeTime = trail.lifeTime;
	}
}

Vector3 GPUEmitter::GetEmitterPosition() const
{
	switch (currentShape_) {
	case EmitterShape::Sphere:   return emitterSphereData_->translate;
	case EmitterShape::Box:      return emitterBoxData_->translate;
	case EmitterShape::Triangle: return emitterTriangleData_->translate;
	case EmitterShape::Cone:     return emitterConeData_->translate;
	case EmitterShape::Mesh:     return emitterMeshData_->translate;
	}
	return { 0,0,0 };
}
