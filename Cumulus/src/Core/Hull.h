#pragma once
#include <DirectXMath.h>
#include <vector>
#include "CommonTypes.h"
#include <assimp/vector3.h>
#include <unordered_map>

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
		void ReassignOutsidePoints(
			const std::vector<int>& removedFaces,
			std::unordered_map<int, std::vector<int>>& faceToFurthestPoints,
			std::unordered_map<int, double>& pointToFaceDistance,
			const std::vector<Muon::HullFace>& faces,
			const std::vector<bool>& faceDeleted
		);
	};
}
