#include "MeshPrimitive.h"
#include <algorithm>
#include <numbers>


std::shared_ptr<Mesh> MeshPrimitive::CreatePlane(float w, float h)
{
	auto mesh = std::make_shared<Mesh>();

	float halfW = w;
	float halfH = h;

	std::vector<Mesh::VertexData> vertices = {
		{ {  halfW,  halfH, 0.0f, 1.0f },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }, // 0: 右上
		{ { -halfW,  halfH, 0.0f, 1.0f },{ 0.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }, // 1: 左上
		{ { -halfW, -halfH, 0.0f, 1.0f },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } }, // 2: 左下
		{ {  halfW, -halfH, 0.0f, 1.0f },{ 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } }, // 3: 右下
	};

	std::vector<uint32_t> indices = {
		0, 1, 2,
		0, 2, 3,
	};

	mesh->Initialize(vertices, indices);
	mesh->TransferData();
	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateBox(float w, float h, float d) {
	auto mesh = std::make_shared<Mesh>();

	float hw = w * 0.5f;
	float hh = h * 0.5f;
	float hd = d * 0.5f;

	using V = Mesh::VertexData;
	std::vector<V> vertices = {
		// 前面
		{{-hw, -hh, -hd, 1}, {0, 1}, {0, 0, -1}},
		{{ hw, -hh, -hd, 1}, {1, 1}, {0, 0, -1}},
		{{ hw,  hh, -hd, 1}, {1, 0}, {0, 0, -1}},
		{{-hw,  hh, -hd, 1}, {0, 0}, {0, 0, -1}},

		// 背面
		{{-hw, -hh,  hd, 1}, {1, 1}, {0, 0, 1}},
		{{-hw,  hh,  hd, 1}, {1, 0}, {0, 0, 1}},
		{{ hw,  hh,  hd, 1}, {0, 0}, {0, 0, 1}},
		{{ hw, -hh,  hd, 1}, {0, 1}, {0, 0, 1}},

		// 左
		{{-hw, -hh,  hd, 1}, {0, 1}, {-1, 0, 0}},
		{{-hw, -hh, -hd, 1}, {1, 1}, {-1, 0, 0}},
		{{-hw,  hh, -hd, 1}, {1, 0}, {-1, 0, 0}},
		{{-hw,  hh,  hd, 1}, {0, 0}, {-1, 0, 0}},

		// 右
		{{ hw, -hh, -hd, 1}, {0, 1}, {1, 0, 0}},
		{{ hw, -hh,  hd, 1}, {1, 1}, {1, 0, 0}},
		{{ hw,  hh,  hd, 1}, {1, 0}, {1, 0, 0}},
		{{ hw,  hh, -hd, 1}, {0, 0}, {1, 0, 0}},

		// 上
		{{-hw,  hh, -hd, 1}, {0, 1}, {0, 1, 0}},
		{{ hw,  hh, -hd, 1}, {1, 1}, {0, 1, 0}},
		{{ hw,  hh,  hd, 1}, {1, 0}, {0, 1, 0}},
		{{-hw,  hh,  hd, 1}, {0, 0}, {0, 1, 0}},

		// 下
		{{-hw, -hh,  hd, 1}, {0, 1}, {0, -1, 0}},
		{{ hw, -hh,  hd, 1}, {1, 1}, {0, -1, 0}},
		{{ hw, -hh, -hd, 1}, {1, 0}, {0, -1, 0}},
		{{-hw, -hh, -hd, 1}, {0, 0}, {0, -1, 0}},
	};

	std::vector<uint32_t> indices = {
		// 前
		0, 1, 2, 0, 2, 3,
		// 後
		4, 5, 6, 4, 6, 7,
		// 左
		8, 9,10, 8,10,11,
		// 右
	   12,13,14,12,14,15,
	   // 上
	  16,17,18,16,18,19,
	  // 下
	 20,21,22,20,22,23,
	};

	mesh->Initialize(vertices, indices);
	mesh->TransferData();
	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateRing(float outerRadius, float innerRadius, uint32_t divide) {
	auto mesh = std::make_unique<Mesh>();

	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(divide);

	using V = Mesh::VertexData;
	std::vector<V> vertices;
	std::vector<uint32_t> indices;

	for (uint32_t i = 0; i < divide; ++i) {
		float theta = i * radianPerDivide;
		float thetaNext = (i + 1) * radianPerDivide;

		float sin = std::sin(theta);
		float cos = std::cos(theta);
		float sinNext = std::sin(thetaNext);
		float cosNext = std::cos(thetaNext);

		float u = float(i) / float(divide);
		float uNext = float(i + 1) / float(divide);

		// 頂点4点を定義
		V v1 = { { cos * outerRadius, sin * outerRadius, 0.0f, 1.0f }, { u, 0.0f }, { 0, 0, 1 } };
		V v2 = { { cosNext * outerRadius, sinNext * outerRadius, 0.0f, 1.0f }, { uNext, 0.0f }, { 0, 0, 1 } };
		V v3 = { { cos * innerRadius, sin * innerRadius, 0.0f, 1.0f }, { u, 1.0f }, { 0, 0, 1 } };
		V v4 = { { cosNext * innerRadius, sinNext * innerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, { 0, 0, 1 } };

		uint32_t start = static_cast<uint32_t>(vertices.size());

		// 頂点追加
		vertices.push_back(v1); // 0
		vertices.push_back(v2); // 1
		vertices.push_back(v3); // 2
		vertices.push_back(v4); // 3

		// 三角形① v1, v2, v3
		indices.push_back(start + 0);
		indices.push_back(start + 1);
		indices.push_back(start + 2);

		// 三角形② v2, v4, v3
		indices.push_back(start + 1);
		indices.push_back(start + 3);
		indices.push_back(start + 2);
	}

	mesh->Initialize(vertices, indices);
	mesh->TransferData();

	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateCylinder(float outerRadius, float innerRadius, uint32_t divide, float height)
{
	auto mesh = std::make_unique<Mesh>();
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(divide);
	const float halfHeight = height * 0.5f;

	using V = Mesh::VertexData;
	std::vector<V> vertices;
	std::vector<uint32_t> indices;

	// --- リングの側面（外周・内周）を追加 ---
	for (uint32_t i = 0; i < divide; ++i) {
		float sin = std::sin(i * radianPerDivide);
		float cos = std::cos(i * radianPerDivide);
		float sinNext = std::sin((i + 1) * radianPerDivide);
		float cosNext = std::cos((i + 1) * radianPerDivide);
		float u = float(i) / float(divide);
		float uNext = float(i + 1) / float(divide);

		// 外側の上下
		V v1 = { {-sin * outerRadius, cos * outerRadius, -halfHeight, 1.0f}, {u, 0.0f} }; // 下
		V v2 = { {-sin * outerRadius, cos * outerRadius, +halfHeight, 1.0f}, {u, 1.0f} }; // 上
		V v3 = { {-sinNext * outerRadius, cosNext * outerRadius, -halfHeight, 1.0f}, {uNext, 0.0f} };
		V v4 = { {-sinNext * outerRadius, cosNext * outerRadius, +halfHeight, 1.0f}, {uNext, 1.0f} };

		// 内側の上下（※法線反転するならこっちの面の三角形順序も反転）
		V v5 = { {-sin * innerRadius, cos * innerRadius, -halfHeight, 1.0f}, {u, 0.0f} };
		V v6 = { {-sin * innerRadius, cos * innerRadius, +halfHeight, 1.0f}, {u, 1.0f} };
		V v7 = { {-sinNext * innerRadius, cosNext * innerRadius, -halfHeight, 1.0f}, {uNext, 0.0f} };
		V v8 = { {-sinNext * innerRadius, cosNext * innerRadius, +halfHeight, 1.0f}, {uNext, 1.0f} };

		uint32_t start = static_cast<uint32_t>(vertices.size());

		// --- 頂点登録 ---
		vertices.push_back(v1); // 0
		vertices.push_back(v2); // 1
		vertices.push_back(v3); // 2
		vertices.push_back(v4); // 3
		vertices.push_back(v5); // 4
		vertices.push_back(v6); // 5
		vertices.push_back(v7); // 6
		vertices.push_back(v8); // 7

		// --- 外側の側面 ---
		indices.push_back(start + 0);
		indices.push_back(start + 1);
		indices.push_back(start + 2);
		indices.push_back(start + 2);
		indices.push_back(start + 1);
		indices.push_back(start + 3);

		// --- 内側の側面（※順序逆にして裏面になるように）---
		indices.push_back(start + 6);
		indices.push_back(start + 5);
		indices.push_back(start + 4);
		indices.push_back(start + 6);
		indices.push_back(start + 7);
		indices.push_back(start + 5);
	}

	mesh->Initialize(vertices, indices);
	mesh->TransferData();

	return mesh;
}

