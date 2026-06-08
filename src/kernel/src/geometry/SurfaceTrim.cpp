#include <nexus/geometry/SurfaceTrim.h>

#include <cmath>
#include <utility>

namespace nexus::geometry {

// ─── TrimLoop ─────────────────────────────────────────────────────────────────

double TrimLoop::signedArea() const noexcept
{
    double area = 0.0;
    for (const auto& seg : segments) {
        const auto& pts = seg.points;
        const uint32_t n = static_cast<uint32_t>(pts.size());
        for (uint32_t i = 0; i < n; ++i) {
            const auto& a = pts[i];
            const auto& b = pts[(i + 1) % n];
            area += (a.u * b.v - b.u * a.v);
        }
    }
    return area * 0.5;
}

bool TrimLoop::contains(double u, double v) const noexcept
{
    // 2-D ray-cast algorithm: cast a ray in the +u direction from (u, v)
    // and count crossings across all segments.
    int crossings = 0;
    for (const auto& seg : segments) {
        const auto& pts = seg.points;
        const uint32_t n = static_cast<uint32_t>(pts.size());
        if (n < 2) { continue; }
        const uint32_t last = seg.closed ? n : n - 1;
        for (uint32_t i = 0; i < last; ++i) {
            const auto& a = pts[i];
            const auto& b = pts[(i + 1) % n];
            // Edge from a to b.
            if ((a.v <= v && b.v > v) || (b.v <= v && a.v > v)) {
                const double t = (v - a.v) / (b.v - a.v);
                if (u < a.u + t * (b.u - a.u)) {
                    ++crossings;
                }
            }
        }
    }
    return (crossings % 2) == 1;
}

// ─── TrimRegion ───────────────────────────────────────────────────────────────

bool TrimRegion::contains(double u, double v) const noexcept
{
    if (!outer.contains(u, v)) { return false; }
    for (const auto& hole : holes) {
        if (hole.contains(u, v)) { return false; }
    }
    return true;
}

// ─── SurfaceTrimmer ───────────────────────────────────────────────────────────

Mesh SurfaceTrimmer::tessellate(
    const NurbsSurface& surface,
    const std::vector<TrimRegion>& regions,
    const TrimTessellationOptions& opts)
{
    const uint32_t su = opts.samplesU;
    const uint32_t sv = opts.samplesV;
    if (su < 2 || sv < 2) { return {}; }

    const double uLo = surface.paramMinU(), uHi = surface.paramMaxU();
    const double vLo = surface.paramMinV(), vHi = surface.paramMaxV();

    // Sample the surface on a regular (su × sv) grid.
    std::vector<nexus::render::Vec3> pts;
    pts.reserve(su * sv);
    for (uint32_t i = 0; i < su; ++i) {
        const double u = uLo + (uHi - uLo) * (static_cast<double>(i) / (su - 1));
        for (uint32_t j = 0; j < sv; ++j) {
            const double v = vLo + (vHi - vLo) * (static_cast<double>(j) / (sv - 1));
            pts.push_back(surface.evaluate(u, v));
        }
    }

    // Determine which QUADS (i,j)–(i+1,j+1) are kept.
    // Test the quad centre's parameter coordinates.
    auto inAnyRegion = [&](double u, double v) {
        if (regions.empty()) { return true; }
        for (const auto& r : regions) {
            if (r.contains(u, v)) { return true; }
        }
        return false;
    };

    // Build the output mesh by collecting only kept-quad vertices.
    // Re-index vertices used by at least one kept quad.
    std::vector<uint32_t> remap(su * sv, ~0u);
    std::vector<nexus::render::Vec3> outPts;
    std::vector<std::array<uint32_t, 3>> tris;

    auto getIdx = [&](uint32_t i, uint32_t j) -> uint32_t {
        const uint32_t flat = i * sv + j;
        if (remap[flat] == ~0u) {
            remap[flat] = static_cast<uint32_t>(outPts.size());
            outPts.push_back(pts[flat]);
        }
        return remap[flat];
    };

    for (uint32_t i = 0; i + 1 < su; ++i) {
        for (uint32_t j = 0; j + 1 < sv; ++j) {
            // Parameter-space centre of this quad.
            const double uCentre = uLo + (uHi - uLo) * ((i + 0.5) / (su - 1));
            const double vCentre = vLo + (vHi - vLo) * ((j + 0.5) / (sv - 1));
            const bool inside = inAnyRegion(uCentre, vCentre);
            if (inside != opts.keepInside) { continue; }

            const uint32_t v00 = getIdx(i,     j);
            const uint32_t v10 = getIdx(i + 1, j);
            const uint32_t v11 = getIdx(i + 1, j + 1);
            const uint32_t v01 = getIdx(i,     j + 1);
            tris.push_back({v00, v10, v11});
            tris.push_back({v00, v11, v01});
        }
    }

    Mesh out;
    MeshAttributes attrs;
    attrs.setPositions(std::move(outPts));
    out.attributes() = std::move(attrs);
    for (const auto& tri : tris) {
        Face f; f.indices = {tri[0], tri[1], tri[2]};
        out.topology().addFace(std::move(f));
    }
    return out;
}

TrimRegion SurfaceTrimmer::fullDomain(const NurbsSurface& surface)
{
    TrimRegion r;
    const double uLo = surface.paramMinU(), uHi = surface.paramMaxU();
    const double vLo = surface.paramMinV(), vHi = surface.paramMaxV();
    TrimCurveSegment seg;
    seg.closed = true;
    seg.points = {
        {uLo, vLo}, {uHi, vLo}, {uHi, vHi}, {uLo, vHi}, {uLo, vLo}
    };
    r.outer.segments.push_back(std::move(seg));
    return r;
}

TrimLoop SurfaceTrimmer::circularLoop(double cu, double cv, double r, uint32_t segs)
{
    TrimLoop loop;
    TrimCurveSegment seg;
    seg.closed = true;
    seg.points.reserve(segs + 1);
    for (uint32_t i = 0; i < segs; ++i) {
        const double theta = 2.0 * M_PI * i / segs;
        seg.points.push_back({cu + r * std::cos(theta), cv + r * std::sin(theta)});
    }
    seg.points.push_back(seg.points.front()); // close
    loop.segments.push_back(std::move(seg));
    return loop;
}

} // namespace nexus::geometry
