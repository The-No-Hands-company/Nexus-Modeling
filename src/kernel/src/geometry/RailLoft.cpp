#include <nexus/geometry/RailLoft.h>

#include <cmath>
#include <utility>

namespace nexus::geometry {

// ─── helpers ─────────────────────────────────────────────────────────────────

static nexus::render::Vec3 rl_normalize(const nexus::render::Vec3& v)
{
    const float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-12f) { return {0.f, 1.f, 0.f}; }
    return {v.x/len, v.y/len, v.z/len};
}

static nexus::render::Vec3 rl_sub(const nexus::render::Vec3& a,
                                   const nexus::render::Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

static nexus::render::Vec3 rl_add(const nexus::render::Vec3& a,
                                   const nexus::render::Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static nexus::render::Vec3 rl_scale(const nexus::render::Vec3& a, float s)
{
    return {a.x*s, a.y*s, a.z*s};
}

static float rl_dot(const nexus::render::Vec3& a, const nexus::render::Vec3& b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static void addCapFanRL(Mesh& mesh,
                        const std::vector<uint32_t>& ring,
                        uint32_t centreIdx,
                        bool flipWinding)
{
    const uint32_t n = static_cast<uint32_t>(ring.size());
    for (uint32_t i = 0; i < n; ++i) {
        Face f;
        if (!flipWinding) {
            f.indices = {centreIdx, ring[i], ring[(i + 1) % n]};
        } else {
            f.indices = {centreIdx, ring[(i + 1) % n], ring[i]};
        }
        mesh.topology().addFace(std::move(f));
    }
}

// ─── RailLofter::loft ────────────────────────────────────────────────────────

RailLoftResult RailLofter::loft(
    const NurbsCurve& rail0,
    const NurbsCurve& rail1,
    const std::vector<nexus::render::Vec3>& profile2d,
    const RailLoftOptions& opts)
{
    if (profile2d.size() < 2) {
        return {{}, false, "profile needs >= 2 points"};
    }
    const uint32_t nSteps   = std::max(opts.railSteps, 2u);
    const uint32_t nProfile = static_cast<uint32_t>(profile2d.size());

    // Sample both rails at the same normalised parameter fractions.
    const double r0Lo = rail0.paramMin(), r0Hi = rail0.paramMax();
    const double r1Lo = rail1.paramMin(), r1Hi = rail1.paramMax();

    std::vector<nexus::render::Vec3> pts0(nSteps), pts1(nSteps);
    std::vector<nexus::render::Vec3> tan0(nSteps), tan1(nSteps);
    for (uint32_t k = 0; k < nSteps; ++k) {
        const double frac = static_cast<double>(k) / (nSteps - 1);
        pts0[k] = rail0.evaluate(r0Lo + (r0Hi - r0Lo) * frac);
        pts1[k] = rail1.evaluate(r1Lo + (r1Hi - r1Lo) * frac);
        tan0[k] = rl_normalize(rail0.derivative1(r0Lo + (r0Hi - r0Lo) * frac));
        tan1[k] = rl_normalize(rail1.derivative1(r1Lo + (r1Hi - r1Lo) * frac));
    }

    // Build rings.  At each step:
    //   W  = normalize(pts1[k] - pts0[k])     (span / width direction)
    //   T  = normalize(tan0[k] + tan1[k])     (mean rail tangent)
    //   U  = normalize(T - (T·W)*W)           (height axis, T projected out of W)
    //   If T is nearly parallel to W, fall back to a world-up axis.
    //
    // Profile point placement:
    //   pos = lerp(pts0[k], pts1[k], p.x)  +  U * p.y * span_len
    // where span_len = |pts1[k] - pts0[k]|.

    std::vector<std::vector<nexus::render::Vec3>> rings(nSteps);
    for (uint32_t k = 0; k < nSteps; ++k) {
        const nexus::render::Vec3 span = rl_sub(pts1[k], pts0[k]);
        const float spanLen = std::sqrt(rl_dot(span, span));
        const nexus::render::Vec3 W = (spanLen > 1e-10f)
            ? rl_scale(span, 1.f / spanLen)
            : nexus::render::Vec3{1.f, 0.f, 0.f};

        // Mean tangent.
        nexus::render::Vec3 T = rl_normalize(rl_add(tan0[k], tan1[k]));
        // Remove component along W so U is perpendicular to the span.
        const float tDotW = rl_dot(T, W);
        nexus::render::Vec3 U = rl_normalize({T.x - tDotW*W.x,
                                               T.y - tDotW*W.y,
                                               T.z - tDotW*W.z});
        // Fallback if T ≈ W.
        if (std::abs(rl_dot(T, W)) > 0.99f) {
            // pick an axis not aligned with W.
            nexus::render::Vec3 alt = (std::abs(W.y) < 0.9f)
                ? nexus::render::Vec3{0.f, 1.f, 0.f}
                : nexus::render::Vec3{0.f, 0.f, 1.f};
            const float d = rl_dot(alt, W);
            U = rl_normalize({alt.x - d*W.x, alt.y - d*W.y, alt.z - d*W.z});
        }

        rings[k].resize(nProfile);
        for (uint32_t pi = 0; pi < nProfile; ++pi) {
            const float sx = profile2d[pi].x; // span fraction [0,1]
            const float sy = profile2d[pi].y; // height
            // Base point: lerp between rail points.
            const nexus::render::Vec3 base = {
                pts0[k].x + sx * span.x,
                pts0[k].y + sx * span.y,
                pts0[k].z + sx * span.z,
            };
            rings[k][pi] = rl_add(base, rl_scale(U, sy * spanLen));
        }
    }

    // Assemble mesh.
    Mesh mesh;
    MeshAttributes attrs;
    std::vector<nexus::render::Vec3> allPts;
    allPts.reserve(nSteps * nProfile + 2);
    for (const auto& ring : rings) {
        for (const auto& p : ring) { allPts.push_back(p); }
    }

    uint32_t startCapIdx = 0, endCapIdx = 0;
    if (opts.capStart) {
        startCapIdx = static_cast<uint32_t>(allPts.size());
        nexus::render::Vec3 c{0, 0, 0};
        for (const auto& p : rings.front()) { c.x += p.x; c.y += p.y; c.z += p.z; }
        const float inv = 1.f / nProfile;
        allPts.push_back({c.x*inv, c.y*inv, c.z*inv});
    }
    if (opts.capEnd) {
        endCapIdx = static_cast<uint32_t>(allPts.size());
        nexus::render::Vec3 c{0, 0, 0};
        for (const auto& p : rings.back()) { c.x += p.x; c.y += p.y; c.z += p.z; }
        const float inv = 1.f / nProfile;
        allPts.push_back({c.x*inv, c.y*inv, c.z*inv});
    }

    attrs.setPositions(std::move(allPts));
    mesh.attributes() = std::move(attrs);

    auto ringIdx = [&](uint32_t k, uint32_t pi) { return k * nProfile + pi; };

    for (uint32_t k = 0; k + 1 < nSteps; ++k) {
        const uint32_t lastP = opts.closedProfile ? nProfile : nProfile - 1;
        for (uint32_t pi = 0; pi < lastP; ++pi) {
            const uint32_t piNext = (pi + 1) % nProfile;
            Face f0; f0.indices = {ringIdx(k,     pi),
                                   ringIdx(k + 1, pi),
                                   ringIdx(k + 1, piNext)};
            Face f1; f1.indices = {ringIdx(k,     pi),
                                   ringIdx(k + 1, piNext),
                                   ringIdx(k,     piNext)};
            mesh.topology().addFace(std::move(f0));
            mesh.topology().addFace(std::move(f1));
        }
    }

    if (opts.capStart) {
        std::vector<uint32_t> ring;
        for (uint32_t pi = 0; pi < nProfile; ++pi) { ring.push_back(ringIdx(0, pi)); }
        addCapFanRL(mesh, ring, startCapIdx, true);
    }
    if (opts.capEnd) {
        std::vector<uint32_t> ring;
        for (uint32_t pi = 0; pi < nProfile; ++pi) { ring.push_back(ringIdx(nSteps - 1, pi)); }
        addCapFanRL(mesh, ring, endCapIdx, false);
    }

    return {std::move(mesh), true, {}};
}

// ─── RailLofter::loftFlat ────────────────────────────────────────────────────

RailLoftResult RailLofter::loftFlat(
    const NurbsCurve& rail0,
    const NurbsCurve& rail1,
    uint32_t profileSteps,
    const RailLoftOptions& opts)
{
    if (profileSteps < 2) { profileSteps = 2; }
    std::vector<nexus::render::Vec3> profile;
    profile.reserve(profileSteps);
    for (uint32_t i = 0; i < profileSteps; ++i) {
        const float x = static_cast<float>(i) / (profileSteps - 1);
        profile.push_back({x, 0.f, 0.f});
    }
    return loft(rail0, rail1, profile, opts);
}

} // namespace nexus::geometry
