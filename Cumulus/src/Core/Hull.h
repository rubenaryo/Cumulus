#pragma once
#include <DirectXMath.h>
#include <vector>
#include "CommonTypes.h"
#include <assimp/vector3.h>

namespace Muon
{
	class Hull
	{
	public:
		Hull();
		Hull(const aiVector3D* points, int pointsCount);
		std::vector<DirectX::XMVECTOR> hullPoints;
		std::vector<Muon::HullFace> faces;
	protected:
		void BuildHull(const aiVector3D* points, int pointsCount);
	};
}
