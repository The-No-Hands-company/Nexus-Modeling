#include <nexus/geometry/NurbsToMesh.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

struct QuadCell {
    float u0, v0, u1, v1;
};

struct EvalKey {
    float u, v;

    bool operator==(const EvalKey& o) const noexcept {
        return u == o.u && v == o.v;
    }
};

struct EvalKeyHash {
    size_t operator()(EvalKey k) const noexcept {
        auto h = [](float f) { return std::bit_cast<uint32_t>(f); };
        return std::hash<uint64_t>{}((static_cast<uint64_t>(h(k.u)) << 32) | h(k.v));
    }
};

struct EvalCache {
    std::unordered_map<EvalKey, Vec3, EvalKeyHash> pos;
    std::unordered_map<EvalKey, Vec3, EvalKeyHash> du;
    std::unordered_map<EvalKey, Vec3, EvalKeyHash> dv;
    const NurbsSurface* surface = nullptr;

    const Vec3& position(float u, float v) {
        EvalKey k{u, v};
        auto it = pos.find(k);
        if (it != pos.end()) return it->second;
        Vec3 p = surface->evaluate(u, v);
        pos[k] = p;
        du[k] = surface->derivativeU(u, v);
        dv[k] = surface->derivativeV(u, v);
        return pos[k];
    }

    Vec3 normal(float u, float v) {
        EvalKey k{u, v};
        auto itU = du.find(k);
        if (itU == du.end()) {
            position(u, v);
            itU = du.find(k);
        }
        Vec3 n = itU->second.cross(dv[k]);
        float len = n.length();
        return (len > 1e-10f) ? (n * (1.f / len)) : Vec3{0.f, 0.f, 1.f};
    }
};

struct TessState {
    std::vector<nexus::render::Vec3>  positions;
    std::vector<nexus::render::Vec3>  normals;
    std::vector<Vec2>                 uvs;
    std::vector<uint32_t>             indices;
    int32_t                           maxDepthSeen = 0;

    uint32_t addVertex(EvalCache& cache, float u, float v) {
        uint32_t idx = static_cast<uint32_t>(positions.size());
        positions.push_back(cache.position(u, v));
        normals.push_back(cache.normal(u, v));
        uvs.push_back({u, v});
        return idx;
    }
};

float chordError(const QuadCell& cell, EvalCache& cache) {
    Vec3 v00 = cache.position(cell.u0, cell.v0);
    Vec3 v10 = cache.position(cell.u1, cell.v0);
    Vec3 v11 = cache.position(cell.u1, cell.v1);
    Vec3 v01 = cache.position(cell.u0, cell.v1);

    Vec3 midBottom = cache.position((cell.u0 + cell.u1) * 0.5f, cell.v0);
    Vec3 midRight  = cache.position(cell.u1, (cell.v0 + cell.v1) * 0.5f);
    Vec3 midTop    = cache.position((cell.u0 + cell.u1) * 0.5f, cell.v1);
    Vec3 midLeft   = cache.position(cell.u0, (cell.v0 + cell.v1) * 0.5f);

    float errBottom = ((v00 + v10) * 0.5f - midBottom).length();
    float errRight  = ((v10 + v11) * 0.5f - midRight).length();
    float errTop    = ((v01 + v11) * 0.5f - midTop).length();
    float errLeft   = ((v00 + v01) * 0.5f - midLeft).length();

    return std::max({errBottom, errRight, errTop, errLeft});
}

void emitQuad(const QuadCell& cell, TessState& state, EvalCache& cache) {
    uint32_t v00 = state.addVertex(cache, cell.u0, cell.v0);
    uint32_t v10 = state.addVertex(cache, cell.u1, cell.v0);
    uint32_t v11 = state.addVertex(cache, cell.u1, cell.v1);
    uint32_t v01 = state.addVertex(cache, cell.u0, cell.v1);

    state.indices.push_back(v00); state.indices.push_back(v10); state.indices.push_back(v11);
    state.indices.push_back(v00); state.indices.push_back(v11); state.indices.push_back(v01);
}

void subdivideCell(const QuadCell& cell, TessState& state, EvalCache& cache,
                   int32_t depth, float tolerance) {
    if (depth > state.maxDepthSeen) state.maxDepthSeen = depth;

    if (depth == 0 || chordError(cell, cache) <= tolerance) {
        emitQuad(cell, state, cache);
        return;
    }

    float um = (cell.u0 + cell.u1) * 0.5f;
    float vm = (cell.v0 + cell.v1) * 0.5f;

    subdivideCell({cell.u0, cell.v0, um, vm}, state, cache, depth - 1, tolerance);
    subdivideCell({um, cell.v0, cell.u1, vm}, state, cache, depth - 1, tolerance);
    subdivideCell({cell.u0, vm, um, cell.v1}, state, cache, depth - 1, tolerance);
    subdivideCell({um, vm, cell.u1, cell.v1}, state, cache, depth - 1, tolerance);
}

} // namespace

TessellationResult tessellateNurbs(const NurbsSurface& surface,
                                  const TessellationOptions& opts) {
    TessellationResult result;
    if (!surface.isValid()) return result;

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();

    float du = (uMax - uMin) / static_cast<float>(opts.initialResU);
    float dv = (vMax - vMin) / static_cast<float>(opts.initialResV);

    EvalCache cache;
    cache.surface = &surface;

    TessState state;
    int32_t effectiveDepth = opts.maxDepth;

    for (int32_t i = 0; i < opts.initialResU; ++i) {
        float u0 = uMin + static_cast<float>(i) * du;
        float u1 = u0 + du;
        for (int32_t j = 0; j < opts.initialResV; ++j) {
            float v0 = vMin + static_cast<float>(j) * dv;
            float v1 = v0 + dv;
            subdivideCell({u0, v0, u1, v1}, state, cache, effectiveDepth, opts.tolerance);
        }
    }

    Mesh& mesh = result.mesh;
    mesh.attributes().setPositions(std::move(state.positions));
    mesh.attributes().setNormals(std::move(state.normals));
    mesh.attributes().setUVs(std::move(state.uvs));

    MeshTopology& topo = mesh.topology();
    for (size_t ti = 0; ti + 2 < state.indices.size(); ti += 3) {
        Face f;
        f.indices = {state.indices[ti], state.indices[ti + 1], state.indices[ti + 2]};
        topo.addFace(f);
    }

    result.totalTriangles  = static_cast<int32_t>(state.indices.size() / 3);
    result.maxDepthReached = state.maxDepthSeen;
    return result;
}

} // namespace nexus::geometry
