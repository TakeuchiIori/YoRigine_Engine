#pragma once
#include "Mesh.h"
#include <memory>

// メッシュプリミティブ生成クラス
class MeshPrimitive
{
public:
	static std::shared_ptr<Mesh> CreatePlane(float w, float h);
	static std::shared_ptr<Mesh> CreateBox(float w, float h, float d);
	static std::shared_ptr<Mesh> CreateRing(float outerRadius, float innerRadius, uint32_t divide);
	static std::shared_ptr<Mesh> CreateCylinder(float outerRadius, float innerRadius, uint32_t divide, float height);

};

