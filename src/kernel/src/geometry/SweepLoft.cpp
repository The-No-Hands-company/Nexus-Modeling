#include <nexus/geometry/SweepLoft.h>

#include <cmath>
#include <utility>

namespace nexus::geometry {

// ─── Rotation-minimizing frame (double reflection method) ─────────────────────
// Given consecutive tangents T0, T1 and frame (N0, B0), compute (N1, B1).

static void rmfStep(
    const nexus::render::Vec3& p0, const nexus::render::Vec3& p1,
    const nexus::render::Vec3& t0, const nexus::render::Vec3& t1,
    nexus::render::Vec3& n, nexus::render::Vec3& b)
{
    // Reflect N across the plane bisecting p0-p1.
    const float dx = p1.x - p0.x, dy = p1.y - p0.y, dz = p1.z - p0.z;
    const float len2 = dx*dx + dy*dy + dz*dz;
    if (len2 < 1e-20f) { return; }
    const float dot = n.x*dx + n.y*dy + n.z*dz;
    const float c   = 2.f * dot / len2;
    nexus::render::Vec3 n1{n.x - c*dx, n.y - c*dy, n.z - c*dz};

    // Reflect n1 across the plane with normal (t1 - reflected_t0).
    const float dt0  = t0.x*dx + t0.y*dy + t0.z*dz;
    const float ct   = 2.f * dt0 / len2;
    const nexus::render::Vec3 rt0{t0.x - ct*dx, t0.y - ct*dy, t0.z - ct*dz};
    const float dx2 = t1.x - rt0.x, dy2 = t1.y - rt0.y, dz2 = t1.z - rt0.z;
    const float len22 = dx2*dx2 + dy2*dy2 + dz2*dz2;
    if (len22 > 1e-20f) {
        const float dn1 = n1.x*dx2 + n1.y*dy2 + n1.z*dz2;
        const float c2  = 2.f * dn1 / len22;
        n1.x -= c2*dx2; n1.y -= c2*dy2; n1.z -= c2*dz2;
    }
    // Binormal B = T × N.
    n = n1;
    b = {t1.y*n1.z - t1.z*n1.y,
         t1.z*n1.x - t1.x*n1.z,
         t1.x*n1.y - t1.y*n1.x};
}

static nexus::render::Vec3 normalize3(const nexus::render::Vec3& v)
{
    const float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-12f) { return {0, 0, 1}; }
    return {v.x/len, v.y/len, v.z/len};
}

// ─── addCapFan ────────────────────────────────────────────────────────────────

static void addCapFan(Mesh& mesh,
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

// ─── Sweeper::sweep ───────────────────────────────────────────────────────────

SweepResult Sweeper::sweep(const NurbsCurve& spine,
                            const std::vector<nexus::render::Vec3>& profile2d,
                            const SweepOptions& opts)
{
    if (profile2d.size() < 2) {
        return {{}, false, "profile needs >= 2 points"};
    }
    const uint32_t nSteps   = std::max(opts.spineSteps, 2u);
    const uint32_t nProfile = static_cast<uint32_t>(profile2d.size());

    // Sample spine positions and tangents.
    const double uLo = spine.paramMin(), uHi = spine.paramMax();
    std::vector<nexus::render::Vec3> spinePos(nSteps);
    std::vector<nexus::render::Vec3> spineTan(nSteps);
    for (uint32_t k = 0; k < nSteps; ++k) {
        const double u = uLo + (uHi - uLo) * (static_cast<double>(k) / (nSteps - 1));
        spinePos[k] = spine.evaluate(u);
        spineTan[k] = normalize3(spine.derivative1(u));
    }

    // Build initial frame (N, B) from the first tangent.
    nexus::render::Vec3 T0 = spineTan[0];
    // Choose an initial N perpendicular to T0.
    nexus::render::Vec3 N0, B0;
    if (std::abs(T0.x) < 0.9f) {
        const nexus::render::Vec3 up{1, 0, 0};
        const float d = up.x*T0.x + up.y*T0.y + up.z*T0.z;
        N0 = normalize3({up.x - d*T0.x, up.y - d*T0.y, up.z - d*T0.z});
    } else {
        const nexus::render::Vec3 up{0, 1, 0};
        const float d = up.x*T0.x + up.y*T0.y + up.z*T0.z;
        N0 = normalize3({up.x - d*T0.x, up.y - d*T0.y, up.z - d*T0.z});
    }
    B0 = {T0.y*N0.z - T0.z*N0.y, T0.z*N0.x - T0.x*N0.z, T0.x*N0.y - T0.y*N0.x};

    // Build all rings.
    std::vector<std::vector<nexus::render::Vec3>> rings(nSteps);
    nexus::render::Vec3 N = N0, B = B0;

    for (uint32_t k = 0; k < nSteps; ++k) {
        const float t = static_cast<float>(k) / (nSteps - 1);
        const float scale = opts.scaleStart + (opts.scaleEnd - opts.scaleStart) * t;
        const float twist = opts.twistRadians * t;
        const float ct = std::cos(twist), st = std::sin(twist);

        // Rotate N, B by twist angle.
        const nexus::render::Vec3 Nt{ ct*N.x + st*B.x, ct*N.y + st*B.y, ct*N.z + st*B.z };
        const nexus::render::Vec3 Bt{-st*N.x + ct*B.x,-st*N.y + ct*B.y,-st*N.z + ct*B.z };

        rings[k].resize(nProfile);
        for (uint32_t pi = 0; pi < nProfile; ++pi) {
            const auto& p = profile2d[pi];
            rings[k][pi] = {
                spinePos[k].x + scale * (p.x * Nt.x + p.y * Bt.x),
                spinePos[k].y + scale * (p.x * Nt.y + p.y * Bt.y),
                spinePos[k].z + scale * (p.x * Nt.z + p.y * Bt.z)
            };
        }

        if (k + 1 < nSteps) {
            rmfStep(spinePos[k], spinePos[k + 1], spineTan[k], spineTan[k + 1], N, B);
        }
    }

    // Assemble mesh.
    Mesh mesh;
    MeshAttributes attrs;
    std::vector<nexus::render::Vec3> allPts;
    allPts.reserve(nSteps * nProfile + 2);
    for (const auto& ring : rings) {
        for (const auto& pt : ring) { allPts.push_back(pt); }
    }
    // Optional cap centres.
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

    auto ringIdx = [&](uint32_t k, uint32_t pi) {
        return k * nProfile + pi;
    };

    const uint32_t lastStep = opts.closedSpine ? nSteps : nSteps - 1;
    for (uint32_t k = 0; k < lastStep; ++k) {
        const uint32_t kNext = (k + 1) % nSteps;
        const uint32_t lastP = opts.closedProfile ? nProfile : nProfile - 1;
        for (uint32_t pi = 0; pi < lastP; ++pi) {
            const uint32_t piNext = (pi + 1) % nProfile;
            Face f0; f0.indices = {ringIdx(k,    pi),
                                   ringIdx(kNext, pi),
                                   ringIdx(kNext, piNext)};
            Face f1; f1.indices = {ringIdx(k,    pi),
                                   ringIdx(kNext, piNext),
                                   ringIdx(k,    piNext)};
            mesh.topology().addFace(std::move(f0));
            mesh.topology().addFace(std::move(f1));
        }
    }

    // Caps.
    if (opts.capStart) {
        std::vector<uint32_t> ring;
        for (uint32_t pi = 0; pi < nProfile; ++pi) { ring.push_back(ringIdx(0, pi)); }
        addCapFan(mesh, ring, startCapIdx, true);
    }
    if (opts.capEnd) {
        std::vector<uint32_t> ring;
        for (uint32_t pi = 0; pi < nProfile; ++pi) { ring.push_back(ringIdx(nSteps - 1, pi)); }
        addCapFan(mesh, ring, endCapIdx, false);
    }

    return {std::move(mesh), true, {}};
}

SweepResult Sweeper::sweepCircle(const NurbsCurve& spine, float radius,
                                  const SweepOptions& opts)
{
    const uint32_t n = (opts.profileSteps > 0) ? opts.profileSteps : 16;
    std::vector<nexus::render::Vec3> profile;
    profile.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        const float theta = 2.f * static_cast<float>(M_PI) * i / n;
        profile.push_back({radius * std::cos(theta), radius * std::sin(theta), 0.f});
    }
    return sweep(spine, profile, opts);
}

// ─── Lofter::loft ─────────────────────────────────────────────────────────────

LoftResult Lofter::loft(const std::vector<std::vector<nexus::render::Vec3>>& profiles,
                         const LoftOptions& opts)
{
    if (profiles.size() < 2) {
        return {{}, false, "need at least 2 profiles"};
    }
    const uint32_t nPts = static_cast<uint32_t>(profiles[0].size());
    for (const auto& p : profiles) {
        if (p.size() != nPts) {
            return {{}, false, "all profiles must have the same vertex count"};
        }
    }
    const uint32_t nSections = static_cast<uint32_t>(profiles.size());

    Mesh mesh;
    MeshAttributes attrs;
    std::vector<nexus::render::Vec3> allPts;
    allPts.reserve(nSections * nPts + 2);
    for (const auto& prof : profiles) {
        for (const auto& p : prof) { allPts.push_back(p); }
    }
    // Cap centres.
    uint32_t startCapIdx = 0, endCapIdx = 0;
    if (opts.capStart) {
        startCapIdx = static_cast<uint32_t>(allPts.size());
        nexus::render::Vec3 c{0, 0, 0};
        for (const auto& p : profiles.front()) { c.x+=p.x; c.y+=p.y; c.z+=p.z; }
        const float inv = 1.f / nPts;
        allPts.push_back({c.x*inv, c.y*inv, c.z*inv});
    }
    if (opts.capEnd) {
        endCapIdx = static_cast<uint32_t>(allPts.size());
        nexus::render::Vec3 c{0, 0, 0};
        for (const auto& p : profiles.back()) { c.x+=p.x; c.y+=p.y; c.z+=p.z; }
        const float inv = 1.f / nPts;
        allPts.push_back({c.x*inv, c.y*inv, c.z*inv});
    }

    attrs.setPositions(std::move(allPts));
    mesh.attributes() = std::move(attrs);

    auto idx = [&](uint32_t sec, uint32_t pi) { return sec * nPts + pi; };

    const uint32_t lastSec = opts.closedLoft ? nSections : nSections - 1;
    for (uint32_t s = 0; s < lastSec; ++s) {
        const uint32_t sNext = (s + 1) % nSections;
        const uint32_t lastP = opts.closedProfiles ? nPts : nPts - 1;
        for (uint32_t pi = 0; pi < lastP; ++pi) {
            const uint32_t piNext = (pi + 1) % nPts;
            Face f0; f0.indices = {idx(s, pi), idx(sNext, pi), idx(sNext, piNext)};
            Face f1; f1.indices = {idx(s, pi), idx(sNext, piNext), idx(s, piNext)};
            mesh.topology().addFace(std::move(f0));
            mesh.topology().addFace(std::move(f1));
        }
    }

    if (opts.capStart) {
        std::vector<uint32_t> ring;
        for (uint32_t pi = 0; pi < nPts; ++pi) { ring.push_back(idx(0, pi)); }
        addCapFan(mesh, ring, startCapIdx, true);
    }
    if (opts.capEnd) {
        std::vector<uint32_t> ring;
        for (uint32_t pi = 0; pi < nPts; ++pi) { ring.push_back(idx(nSections - 1, pi)); }
        addCapFan(mesh, ring, endCapIdx, false);
    }

    return {std::move(mesh), true, {}};
}

LoftResult Lofter::loftCurves(const std::vector<NurbsCurve>& curves,
                                uint32_t samplesPerProfile,
                                const LoftOptions& opts)
{
    if (samplesPerProfile < 2) { samplesPerProfile = 2; }
    std::vector<std::vector<nexus::render::Vec3>> profiles;
    profiles.reserve(curves.size());
    for (const auto& c : curves) {
        profiles.push_back(c.tessellate(samplesPerProfile));
    }
    return loft(profiles, opts);
}

} // namespace nexus::geometry
