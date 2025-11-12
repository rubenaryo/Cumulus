#include "Hull.h"
#include <assimp/vector3.h>

namespace Muon
{
    Hull::Hull()
    {
    }
    Hull::Hull(const aiVector3D* points, int pointsCount)
    {
        BuildHull(points, pointsCount);
    }

    void Hull::BuildHull(const aiVector3D* points, int pointsCount)
    {
        if (pointsCount < 4)
        {
            perror("ERROR: Not enough points for hull");
            return;
        }

        float maxX = -FLT_MAX;
        float minX = FLT_MAX;

        int minXIndex = -1;
        int maxXIndex = -1;

        // ---------------------------------------------------------
        // Step 1: find extreme X points
        // ---------------------------------------------------------
        for (int i = 0; i < pointsCount; ++i)
        {
            float x = points[i].x;
            if (x < minX)
            {
                minX = x;
                minXIndex = i;
            }

            if (x > maxX)
            {
                maxX = x;
                maxXIndex = i;
            }
        }

        if (minXIndex == maxXIndex)
        {
            perror("ERROR: all points are identical in Hull");
            return;
        }

        DirectX::XMVECTOR A = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&points[minXIndex]));
        DirectX::XMVECTOR B = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&points[maxXIndex]));


        // ---------------------------------------------------------
        // Step 2: find point C farthest from AB line
        // ---------------------------------------------------------
        DirectX::XMVECTOR AB = DirectX::XMVectorSubtract(B, A);
        DirectX::XMVECTOR ABn = DirectX::XMVector3Normalize(AB);

        int topIndex = -1;
        float maxDistFromLine = 0.0f;

        for (int i = 0; i < pointsCount; ++i)
        {
            if (i == minXIndex || i == maxXIndex)
                continue;

            DirectX::XMVECTOR P = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&points[i]));
            DirectX::XMVECTOR AP = DirectX::XMVectorSubtract(P, A);
            // perpendicular distance to line AB = |AP × ABn|
            DirectX::XMVECTOR crossProd = DirectX::XMVector3Cross(AP, ABn);
            float dist = DirectX::XMVectorGetX(DirectX::XMVector3Length(crossProd));

            if (dist > maxDistFromLine)
            {
                maxDistFromLine = dist;
                topIndex = i;
            }
        }

        if (topIndex == -1 || maxDistFromLine < 1e-5f)
        {
            perror("ERROR: Points are collinear in Hull");
            return;
        }

        DirectX::XMVECTOR C = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&points[topIndex]));

        // ---------------------------------------------------------
        // Step 3: find point D farthest from plane ABC
        // ---------------------------------------------------------
        DirectX::XMVECTOR ABv = DirectX::XMVectorSubtract(B, A);
        DirectX::XMVECTOR ACv = DirectX::XMVectorSubtract(C, A);
        DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(ABv, ACv));

        float maxPlaneDist = 0.0f;
        int dIndex = -1;

        float planeD = DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, A)); // plane eq: n·x = d

        for (int i = 0; i < pointsCount; ++i)
        {
            if (i == minXIndex || i == maxXIndex || i == topIndex)
                continue;

            DirectX::XMVECTOR P = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&points[i]));
            float dist = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, P)) - planeD);

            if (dist > maxPlaneDist)
            {
                maxPlaneDist = dist;
                dIndex = i;
            }
        }

        if (dIndex == -1 || maxPlaneDist < 1e-5f)
        {
            perror("ERROR: Points are coplanar in Hull");
            return;
        }

        DirectX::XMVECTOR D = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&points[dIndex]));


        // ---------------------------------------------------------
        // Step 4: Build tetrahedron faces
        // ---------------------------------------------------------
        hullPoints.clear();
        faces.clear();

        hullPoints.push_back(A); // 0
        hullPoints.push_back(B); // 1
        hullPoints.push_back(C); // 2
        hullPoints.push_back(D); // 3

        auto AddFace = [&](int i0, int i1, int i2)
            {
                Muon::HullFace face;
                face.indices[0] = i0;
                face.indices[1] = i1;
                face.indices[2] = i2;

                DirectX::XMVECTOR p0 = hullPoints[i0];
                DirectX::XMVECTOR p1 = hullPoints[i1];
                DirectX::XMVECTOR p2 = hullPoints[i2];
                DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(p1, p0), DirectX::XMVectorSubtract(p2, p0)));
                float d = DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, p0));

                DirectX::XMFLOAT3A out;
                DirectX::XMStoreFloat3A(&out, n);
                face.normal = out;
                face.dot = d;

                faces.push_back(face);
            };

        // Create 4 tetrahedron faces
        AddFace(0, 1, 2);
        AddFace(0, 2, 3);
        AddFace(0, 3, 1);
        AddFace(1, 3, 2);

        // ---------------------------------------------------------
        // Step 5: Ensure outward normals (optional)
        // ---------------------------------------------------------
        // For each face, check if the remaining vertex lies on positive side.
        // If so, flip the face winding.
        for (int fi = 0; fi < (int)faces.size(); ++fi)
        {
            Muon::HullFace face = faces[fi];
            // find the opposite vertex
            int opp = -1;
            for (int i = 0; i < 4; ++i)
            {
                if (i != face.indices[0] && i != face.indices[1] && i != face.indices[2])
                {
                    opp = i;
                    break;
                }
            }
            if (opp == -1) continue;

            DirectX::XMVECTOR N = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&face.normal));

            float side = DirectX::XMVectorGetX(DirectX::XMVector3Dot(N, hullPoints[opp])) - face.dot;
            if (side > 0)
            {
                // inward normal — flip winding
                std::swap(face.indices[1], face.indices[2]);
                DirectX::XMVECTOR p0 = hullPoints[face.indices[0]];
                DirectX::XMVECTOR p1 = hullPoints[face.indices[1]];
                DirectX::XMVECTOR p2 = hullPoints[face.indices[2]];

                DirectX::XMFLOAT3A outNorm;
                auto & norm = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(p1, p0), DirectX::XMVectorSubtract(p2, p0)));
                DirectX::XMStoreFloat3A(&outNorm, norm);
                face.normal = outNorm;
                face.dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(N, p0));
            }
        }
    }
}