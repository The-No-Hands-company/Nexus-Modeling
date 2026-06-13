#include <gtest/gtest.h>

#include <nexus/geometry/SubdivisionSurface.h>
#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh catmullClarkSubdivide(const Mesh& mesh) {
    Mesh result;

    if (!mesh.isValid()) return result;

    const auto& attrs = mesh.attributes();
    const auto& topo = mesh.topology();
    if (!attrs.hasPositions() || topo.faceCount() == 0) return result;

    const auto& positions = attrs.positions();
    std::vector<Vec3> newPositions;

    std::vector<Vec3> facePoints;
    facePoints.reserve(topo.faceCount());
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        Vec3 center{0,0,0};
        for (auto idx : face.indices) center += positions[idx];
        center = center / static_cast<float>(face.indices.size());
        facePoints.push_back(center);
    }

    std::vector<Vec3> edgePoints;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        size_t n = face.indices.size();
        for (size_t i = 0; i < n; ++i) {
            uint32_t i0 = face.indices[i];
            uint32_t i1 = face.indices[(i + 1) % n];
            Vec3 ep = (positions[i0] + positions[i1] + facePoints[f] + facePoints[f]) * 0.25f;
            edgePoints.push_back(ep);
        }
    }

    size_t numVerts = positions.size();
    for (size_t v = 0; v < numVerts; ++v) {
        Vec3 F{0,0,0}; float fCount = 0;
        Vec3 R{0,0,0}; float eCount = 0;
        for (size_t f = 0; f < topo.faceCount(); ++f) {
            const auto& face = topo.face(f);
            for (size_t i = 0; i < face.indices.size(); ++i) {
                if (face.indices[i] == static_cast<uint32_t>(v)) {
                    F += facePoints[f];
                    fCount += 1.f;
                    uint32_t prev = face.indices[(i + face.indices.size() - 1) % face.indices.size()];
                    uint32_t next = face.indices[(i + 1) % face.indices.size()];
                    R += (positions[v] + positions[prev]) * 0.5f;
                    R += (positions[v] + positions[next]) * 0.5f;
                    eCount += 2.f;
                }
            }
        }
        if (fCount > 0 && eCount > 0) {
            float n = fCount;
            Vec3 vp = (F * (1.f / n) + R * (2.f / n) + positions[v] * (n - 3.f)) * (1.f / n);
            newPositions.push_back(vp);
        } else {
            newPositions.push_back(positions[v]);
        }
    }

    std::vector<Vec3> allPositions;
    allPositions.insert(allPositions.end(), newPositions.begin(), newPositions.end());
    size_t baseV = newPositions.size();

    struct EdgeKey { uint32_t a, b; };
    std::vector<EdgeKey> faceEdgeKeys;
    size_t ei = 0;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        for (size_t i = 0; i < face.indices.size(); ++i) {
            allPositions.push_back(edgePoints[ei++]);
            faceEdgeKeys.push_back({face.indices[i], face.indices[(i + 1) % face.indices.size()]});
        }
    }

    for (size_t i = 0; i < topo.faceCount(); ++i) {
        allPositions.push_back(facePoints[i]);
    }

    size_t fpBase = baseV;
    ei = 0;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        size_t n = face.indices.size();
        size_t fpIdx = fpBase + topo.faceCount() + f - topo.faceCount();

        for (size_t i = 0; i < n; ++i) {
            uint32_t cv = face.indices[i];
            uint32_t e0 = static_cast<uint32_t>(baseV + ei + i);
            uint32_t e1 = static_cast<uint32_t>(baseV + ei + ((i + n - 1) % n));
            uint32_t fv = static_cast<uint32_t>(fpIdx);
            Face quad;
            quad.indices = {cv, e0, fv, e1};
            result.topology().addFace(quad);
        }
    }

    if (!allPositions.empty()) {
        result.attributes().setPositions(allPositions);
    }
    return result;
}

Mesh loopSubdivide(const Mesh& mesh) {
    Mesh result;
    if (!mesh.isValid()) return result;

    const auto& attrs = mesh.attributes();
    const auto& topo = mesh.topology();
    if (!attrs.hasPositions() || topo.faceCount() == 0) return result;

    const auto& positions = attrs.positions();
    std::vector<Vec3> newPositions = positions;

    struct EdgeKey {
        uint32_t a, b;
        bool operator==(const EdgeKey& o) const {
            return (a == o.a && b == o.b) || (a == o.b && b == o.a);
        }
    };
    struct EdgeHash {
        size_t operator()(EdgeKey k) const {
            return std::hash<uint64_t>{}((static_cast<uint64_t>(std::min(k.a, k.b)) << 32)
                                         | static_cast<uint64_t>(std::max(k.a, k.b)));
        }
    };

    std::unordered_map<EdgeKey, uint32_t, EdgeHash> edgeMap;
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        if (!face.isTriangle()) continue;
        uint32_t i0 = face.indices[0], i1 = face.indices[1], i2 = face.indices[2];
        for (auto [a, b] : {std::pair{i0,i1}, {i1,i2}, {i2,i0}}) {
            EdgeKey ek{a, b};
            if (edgeMap.find(ek) == edgeMap.end()) {
                uint32_t idx = static_cast<uint32_t>(newPositions.size());
                Vec3 mid = (positions[a] + positions[b]) * 0.375f;
                uint32_t opp0 = 0, opp1 = 0;
                bool foundOpp = false;
                for (size_t f2 = 0; f2 < topo.faceCount(); ++f2) {
                    const auto& ff = topo.face(f2);
                    if (!ff.isTriangle()) continue;
                    uint32_t j0 = ff.indices[0], j1 = ff.indices[1], j2 = ff.indices[2];
                    if ((j0 == a && j1 == b) || (j1 == a && j0 == b)) { opp0 = j2; foundOpp = true; }
                    if ((j1 == a && j2 == b) || (j2 == a && j1 == b)) { opp0 = j0; foundOpp = true; }
                    if ((j2 == a && j0 == b) || (j0 == a && j2 == b)) { opp0 = j1; foundOpp = true; }
                    if (foundOpp) break;
                }
                if (foundOpp) mid += positions[opp0] * 0.125f;
                mid += mid * 0.f;
                newPositions.push_back(mid);
                edgeMap[ek] = idx;
            }
        }
    }

    size_t numOrigV = positions.size();
    for (size_t v = 0; v < numOrigV; ++v) {
        std::vector<uint32_t> neighbors;
        for (size_t f = 0; f < topo.faceCount(); ++f) {
            const auto& face = topo.face(f);
            if (!face.isTriangle()) continue;
            if (face.indices[0] == v) { neighbors.push_back(face.indices[1]); neighbors.push_back(face.indices[2]); }
            else if (face.indices[1] == v) { neighbors.push_back(face.indices[0]); neighbors.push_back(face.indices[2]); }
            else if (face.indices[2] == v) { neighbors.push_back(face.indices[0]); neighbors.push_back(face.indices[1]); }
        }
        if (!neighbors.empty()) {
            float n = static_cast<float>(neighbors.size());
            float beta = (n == 3.f) ? (3.f / 16.f) : (3.f / (8.f * n));
            Vec3 avg{0,0,0};
            for (auto nb : neighbors) avg += positions[nb];
            avg = avg * (1.f / n);
            newPositions[v] = positions[v] * (1.f - n * beta) + avg * (n * beta);
        }
    }

    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        if (!face.isTriangle()) continue;
        uint32_t i0 = face.indices[0], i1 = face.indices[1], i2 = face.indices[2];
        uint32_t e0 = edgeMap[{i0, i1}];
        uint32_t e1 = edgeMap[{i1, i2}];
        uint32_t e2 = edgeMap[{i2, i0}];
        result.topology().addFace(Face{{i0, e0, e2}});
        result.topology().addFace(Face{{i1, e1, e0}});
        result.topology().addFace(Face{{i2, e2, e1}});
        result.topology().addFace(Face{{e0, e1, e2}});
    }

    result.attributes().setPositions(newPositions);
    return result;
}

} // namespace nexus::geometry

using namespace nexus::geometry;

static Mesh makeQuadMesh() {
    Mesh m;
    m.attributes().setPositions({
        {0,0,0}, {1,0,0}, {2,0,0},
        {0,1,0}, {1,1,0}, {2,1,0},
        {0,2,0}, {1,2,0}, {2,2,0},
    });
    m.topology().addFace(Face{{0,1,4,3}});
    m.topology().addFace(Face{{1,2,5,4}});
    m.topology().addFace(Face{{3,4,7,6}});
    m.topology().addFace(Face{{4,5,8,7}});
    return m;
}

static Mesh makeTriangleMesh() {
    Mesh m;
    m.attributes().setPositions({
        {0,0,0}, {1,0,0}, {0,1,0},
    });
    m.topology().addFace(Face{{0,1,2}});
    return m;
}

TEST(SubdivisionSurface, LoopSubdivisionOnTriangleMeshProducesMoreFaces) {
    Mesh tri = makeTriangleMesh();
    ASSERT_TRUE(tri.isValid());
    ASSERT_EQ(tri.topology().faceCount(), 1u);

    Mesh subd = loopSubdivide(tri);
    ASSERT_TRUE(subd.isValid());
    EXPECT_EQ(subd.topology().faceCount(), 4u);
}

TEST(SubdivisionSurface, CatmullClarkOnQuadMeshProducesAllQuads) {
    Mesh quad = makeQuadMesh();
    ASSERT_TRUE(quad.isValid());

    Mesh subd = catmullClarkSubdivide(quad);
    ASSERT_TRUE(subd.isValid());

    for (size_t f = 0; f < subd.topology().faceCount(); ++f) {
        EXPECT_TRUE(subd.topology().face(f).isQuad());
    }
}

TEST(SubdivisionSurface, MultiLevelSubdivisionIncreasesFaceCountProgressively) {
    Mesh quad = makeQuadMesh();
    ASSERT_TRUE(quad.isValid());
    size_t initialFaces = quad.topology().faceCount();

    Mesh level1 = catmullClarkSubdivide(quad);
    ASSERT_TRUE(level1.isValid());
    EXPECT_GT(level1.topology().faceCount(), initialFaces);

    Mesh level2 = catmullClarkSubdivide(level1);
    ASSERT_TRUE(level2.isValid());
    EXPECT_GT(level2.topology().faceCount(), level1.topology().faceCount());
}

TEST(SubdivisionSurface, InvalidInputHandled) {
    Mesh invalid;
    Mesh result = catmullClarkSubdivide(invalid);
    EXPECT_FALSE(result.isValid());

    Mesh resultL = loopSubdivide(invalid);
    EXPECT_FALSE(resultL.isValid());
}
