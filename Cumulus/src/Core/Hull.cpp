#include "Hull.h"
#include <assimp/vector3.h>
#include <unordered_map>
#include <unordered_set>
using namespace DirectX;

namespace Muon
{
    Hull::Hull()
    {
    }
    Hull::Hull(const aiVector3D* points, int pointsCount)
    {
        BuildHull(points, pointsCount);
    }

    static bool FaceVisibleFromPoint(const HullFace& face, XMVECTOR P)
    {
        XMVECTOR N = XMLoadFloat3(&face.normal);
        float side = XMVectorGetX(XMVector3Dot(N, P)) + face.distance;
        return side > 1e-6f;
    }

    //returns (-1,-1) if no face point pairs exist
    //returns face to point
    static std::pair<int, int> FindFurthestFacePointPair(const std::unordered_map<int, std::vector<int>>& faceToPoints, const std::unordered_map<int, double>& pointToFaceDistance) {
        int bestFace = -1;
        int bestPoint = -1;
        double maxDist = -std::numeric_limits<double>::infinity();

        for (const auto& [faceIdx, points] : faceToPoints)
        {
            for (int p : points)
            {
                auto it = pointToFaceDistance.find(p);
                if (it == pointToFaceDistance.end()) continue; // no distance for this point

                double d = it->second;
                if (d > maxDist)
                {
                    maxDist = d;
                    bestFace = faceIdx;
                    bestPoint = p;
                }
            }
        }

        return { bestFace, bestPoint };
    }

    static bool FaceHasDirectedEdge(const HullFace& f, int a, int b)
    {
        return (f.indices[0] == a && f.indices[1] == b) ||
            (f.indices[1] == a && f.indices[2] == b) ||
            (f.indices[2] == a && f.indices[0] == b);
    }

    static void BuildHorizon(
        const std::vector<HullFace>& faces,
        const std::vector<int>& visibleFaces,
        std::vector<Edge>& outHorizon)
    {
        std::unordered_set<int> visibleSet(visibleFaces.begin(), visibleFaces.end());

        std::unordered_set<Edge, EdgeHash> horizonSet;

        auto AddOrRemoveEdge = [&](int a, int b)
            {
                Edge e{ a, b };

                // If the opposite directed edge already exists, remove it.
                // That means edge belongs to INTERNAL region between two visible faces.
                Edge opposite{ b, a };
                if (horizonSet.count(opposite))
                {
                    horizonSet.erase(opposite);
                }
                else
                {
                    horizonSet.insert(e);
                }
            };

        for (int fIdx : visibleFaces)
        {
            const HullFace& f = faces[fIdx];

            int v0 = f.indices[0];
            int v1 = f.indices[1];
            int v2 = f.indices[2];

            // Each face has 3 directed edges
            AddOrRemoveEdge(v0, v1);
            AddOrRemoveEdge(v1, v2);
            AddOrRemoveEdge(v2, v0);
        }

        // Remaining edges form the boundary between visible and non-visible
        outHorizon.assign(horizonSet.begin(), horizonSet.end());
    }

    void Hull::ReassignOutsidePoints(
        const std::vector<int>& removedFaces,
        std::unordered_map<int, std::vector<int>>& faceToFurthestPoints,
        std::unordered_map<int, double>& pointToFaceDistance,
        const std::vector<Muon::HullFace>& faces,
        const std::vector<bool>& faceDeleted
    )
    {
        std::vector<int> toReassign;

        // Collect points from removed faces
        for (int f : removedFaces)
        {
            auto& pts = faceToFurthestPoints[f];
            toReassign.insert(toReassign.end(), pts.begin(), pts.end());
            pts.clear();
        }

        // Now reassign them to new or existing faces
        for (auto& op : toReassign)
        {
            int idx = op;
            XMVECTOR P = XMLoadFloat3((XMFLOAT3*)&hullPoints[idx]);

            float maxDist = 0;
            int bestFace = -1;

            for (int f = 0; f < faces.size(); ++f)
            {
                if (faceDeleted[f]) continue;

                XMVECTOR N = XMLoadFloat3(&faces[f].normal);
                float dist = XMVectorGetX(XMVector3Dot(N, P)) + faces[f].distance;

                if (dist > maxDist)
                {
                    maxDist = dist;
                    bestFace = f;
                }
            }

            if (bestFace != -1) {
                faceToFurthestPoints[bestFace].push_back(idx);
                pointToFaceDistance[idx] = maxDist;
            }
        }
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

        // Step 1: find extreme X points
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

        XMVECTOR A = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&points[minXIndex]));
        XMVECTOR B = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&points[maxXIndex]));


        // Step 2: find point C farthest from AB line
        XMVECTOR AB = XMVectorSubtract(B, A);
        XMVECTOR ABn = XMVector3Normalize(AB);

        int topIndex = -1;
        float maxDistFromLine = 0.0f;

        for (int i = 0; i < pointsCount; ++i)
        {
            if (i == minXIndex || i == maxXIndex)
                continue;

            XMVECTOR P = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&points[i]));
            XMVECTOR AP = XMVectorSubtract(P, A);
            // perpendicular distance to line AB = |AP × ABn|
            XMVECTOR crossProd = XMVector3Cross(AP, ABn);
            float dist = XMVectorGetX(XMVector3Length(crossProd));

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

        XMVECTOR C = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&points[topIndex]));

        // Step 3: find point D farthest from plane ABC
        XMVECTOR ABv = XMVectorSubtract(B, A);
        XMVECTOR ACv = XMVectorSubtract(C, A);
        XMVECTOR n = XMVector3Normalize(XMVector3Cross(ABv, ACv));

        float maxPlaneDist = 0.0f;
        int dIndex = -1;

        float planeD = XMVectorGetX(XMVector3Dot(n, A)); // plane eq: n·x = d

        for (int i = 0; i < pointsCount; ++i)
        {
            if (i == minXIndex || i == maxXIndex || i == topIndex)
                continue;

            XMVECTOR P = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&points[i]));
            float dist = fabsf(XMVectorGetX(XMVector3Dot(n, P)) - planeD);

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

        XMVECTOR D = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&points[dIndex]));


        // Step 4: Build tetrahedron faces
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

                XMVECTOR p0 = hullPoints[i0];
                XMVECTOR p1 = hullPoints[i1];
                XMVECTOR p2 = hullPoints[i2];
                XMVECTOR n = XMVector3Normalize(XMVector3Cross(XMVectorSubtract(p1, p0), XMVectorSubtract(p2, p0)));
                float d = -XMVectorGetX(XMVector3Dot(n, p0));

                XMFLOAT3A out;
                XMStoreFloat3A(&out, n);
                face.normal = out;
                face.distance = d;

                faces.push_back(face);
            };

        // Create 4 tetrahedron faces
        AddFace(0, 1, 2);
        AddFace(0, 2, 3);
        AddFace(0, 3, 1);
        AddFace(1, 3, 2);

        // Step 5: Ensure outward normals
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

            XMVECTOR N = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&face.normal));

            float side = XMVectorGetX(XMVector3Dot(N, hullPoints[opp])) - face.distance;
            if (side > 0)
            {
                // inward normal — flip winding
                std::swap(face.indices[1], face.indices[2]);
                XMVECTOR p0 = hullPoints[face.indices[0]];
                XMVECTOR p1 = hullPoints[face.indices[1]];
                XMVECTOR p2 = hullPoints[face.indices[2]];

                XMFLOAT3A outNorm;
                auto & norm = XMVector3Normalize(XMVector3Cross(XMVectorSubtract(p1, p0), XMVectorSubtract(p2, p0)));
                XMStoreFloat3A(&outNorm, norm);
                face.normal = outNorm;
                face.distance = XMVectorGetX(XMVector3Dot(N, p0));
            }
        }

        std::unordered_map<int, std::vector<int>> faceToFurthestPoints;
        std::unordered_map<int, double> pointToFaceDistance;

        //Step 6: find points outside faces
        for (int i = 0; i < pointsCount; ++i)
        {
            //tetra verts
            if (i == minXIndex || i == maxXIndex || i == topIndex || i == dIndex)
                continue;

            XMVECTOR P = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&points[i]));

            float maxDist = 0;
            int bestFace = -1;

            for (int f = 0; f < faces.size(); ++f)
            {
                XMVECTOR N = XMLoadFloat3(&faces[f].normal);
                float dist = XMVectorGetX(XMVector3Dot(N, P)) + faces[f].distance;

                if (dist > maxDist)
                {
                    maxDist = dist;
                    bestFace = f;
                }
            }


            if (bestFace != -1) {
                faceToFurthestPoints[bestFace].push_back(i);
                pointToFaceDistance[i] = maxDist;
            }
        }

        //Step 7: Expand Hull

        while (true)
        {
            int faceWithFurthest = 0;
            std::pair<int, int> furthestPoint = FindFurthestFacePointPair(faceToFurthestPoints, pointToFaceDistance);
            
            //done!
            if (furthestPoint.first == -1) break;

            XMVECTOR P = XMLoadFloat3((XMFLOAT3*)&points[furthestPoint.second]);

            std::vector<int> visibleFaces;
            for (int f = 0; f < faces.size(); ++f) {
                if (FaceVisibleFromPoint(faces[f], P)) {
                    visibleFaces.push_back(f);
                }
            }

            std::vector<Edge> horizon;
            BuildHorizon(faces, visibleFaces, horizon);

            std::vector<bool> faceDeleted(faces.size(), false);
            std::vector<int> removedFaces;

            for (int idx : visibleFaces) {
                faceDeleted[idx] = true;
                removedFaces.push_back(idx);
            }

            for (auto& e : horizon) {
                AddFace(e.v0, e.v1, furthestPoint.second);
            }

            ReassignOutsidePoints(removedFaces, faceToFurthestPoints, pointToFaceDistance, faces, faceDeleted);
        }
    }
}