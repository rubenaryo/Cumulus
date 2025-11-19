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
		static const int MAX_HULL_LOOPS = 25;
		void BuildHull(const aiVector3D* points, int pointsCount);
		void ReassignOutsidePoints(
			std::unordered_map<int, std::vector<int>>& faceToFurthestPoints,
			std::unordered_map<int, double>& pointToFaceDistance,
			const aiVector3D* points,
			const std::vector<Muon::HullFace>& faces,
			const std::vector<bool>& faceDeleted,
			const std::vector<int>& removedFaces
		);
	};
}
