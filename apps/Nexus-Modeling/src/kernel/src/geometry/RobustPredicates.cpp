#include <nexus/geometry/RobustPredicates.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <limits>
#include <vector>

namespace nexus::geometry {

// --- Expansion arithmetic helpers (Shewchuk, "Adaptive Precision Floating-Point Arithmetic") ---

namespace {
    constexpr double kSplitter = 134217729.0; // 2^27 + 1

    struct FastTwoSumResult { double s, e; };
    struct TwoProdResult { double x, y; };

    inline FastTwoSumResult twoSum(double a, double b) {
        double s = a + b;
        double v = s - a;
        double e = (a - (s - v)) + (b - v);
        return {s, e};
    }

    inline void split(double a, double& hi, double& lo) {
        double c = kSplitter * a;
        double abig = c - a;
        hi = c - abig;
        lo = a - hi;
    }

    inline TwoProdResult twoProduct(double a, double b) {
        double x = a * b;
        double a_hi, a_lo, b_hi, b_lo;
        split(a, a_hi, a_lo);
        split(b, b_hi, b_lo);
        double y = a_hi * b_hi - x + a_hi * b_lo + a_lo * b_hi + a_lo * b_lo;
        return {x, y};
    }

    void growExpansion(double e, double f, std::vector<double>& h) {
        FastTwoSumResult q = twoSum(f, e);
        std::vector<double> hh;
        double qp = q.e;
        for (size_t i = 0; i < h.size(); ++i) {
            FastTwoSumResult qq = twoSum(h[i], qp);
            qp = qq.e;
            if (qq.s != 0.0) hh.push_back(qq.s);
        }
        if (qp != 0.0) hh.push_back(qp);
        if (q.s != 0.0) hh.push_back(q.s);
        h = std::move(hh);
    }

    double expansionValue(const std::vector<double>& e) {
        double sum = 0.0;
        for (double v : e) sum += v;
        return sum;
    }

    double orient2DExact(const Vec2& a, const Vec2& b, const Vec2& c) {
        double acx = a.u - c.u;
        double bcx = b.u - c.u;
        double acy = a.v - c.v;
        double bcy = b.v - c.v;

        auto [det_p1, det_p2] = twoProduct(acx, bcy);
        auto [det_m1, det_m2] = twoProduct(acy, bcx);

        std::vector<double> p{det_p1, det_p2};
        std::vector<double> m{det_m1, det_m2};
        if (p.size() == 2 && p[1] == 0.0) p.resize(1);
        if (m.size() == 2 && m[1] == 0.0) m.resize(1);

        if (p.size() < 2 && m.size() < 2) return p[0] - m[0];

        std::vector<double> e{m[0]};
        for (size_t i = 1; i < m.size(); ++i) {
            if (m[i] != 0.0) e.push_back(m[i]);
        }
        for (double& v : e) v = -v;

        for (size_t i = 0; i < p.size(); ++i) {
            if (p[i] != 0.0) growExpansion(p[i], e[0], e);
        }
        return expansionValue(e);
    }

    double orient3DExact(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                         const nexus::render::Vec3& c, const nexus::render::Vec3& d) {
        double adx = a.x - d.x, ady = a.y - d.y, adz = a.z - d.z;
        double bdx = b.x - d.x, bdy = b.y - d.y, bdz = b.z - d.z;
        double cdx = c.x - d.x, cdy = c.y - d.y, cdz = c.z - d.z;

        double d00 = bdy * cdz - bdz * cdy;
        double d01 = bdz * cdx - bdx * cdz;
        double d02 = bdx * cdy - bdy * cdx;

        auto [p0, pe0] = twoProduct(adx, d00);
        auto [p1, pe1] = twoProduct(ady, d01);
        auto [p2, pe2] = twoProduct(adz, d02);

        auto [s01, se01] = twoSum(p0, p1);
        auto [s, se] = twoSum(s01, p2);

        double det = s + se + se01 + pe0 + pe1 + pe2;

        if (std::abs(det) < 1e-10) {
            std::vector<double> terms;
            if (p0 != 0.0) terms.push_back(p0);
            if (pe0 != 0.0) terms.push_back(pe0);
            if (p1 != 0.0) terms.push_back(p1);
            if (pe1 != 0.0) terms.push_back(pe1);
            if (p2 != 0.0) terms.push_back(p2);
            if (pe2 != 0.0) terms.push_back(pe2);
            if (terms.empty()) return 0.0;
            std::sort(terms.begin(), terms.end(),
                      [](double x, double y) { return std::abs(x) > std::abs(y); });
            double acc = 0.0;
            for (double t : terms) acc += t;
            return acc;
        }

        return det;
    }

    double inCircleExact(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) {
        double adx = a.u - d.u, ady = a.v - d.v;
        double bdx = b.u - d.u, bdy = b.v - d.v;
        double cdx = c.u - d.u, cdy = c.v - d.v;

        double abdet = adx * bdy - ady * bdx;
        double bcdet = bdx * cdy - bdy * cdx;
        double cadet = cdx * ady - cdy * adx;

        double alift = adx * adx + ady * ady;
        double blift = bdx * bdx + bdy * bdy;
        double clift = cdx * cdx + cdy * cdy;

        auto [t0, te0] = twoProduct(alift, bcdet);
        auto [t1, te1] = twoProduct(blift, cadet);
        auto [t2, te2] = twoProduct(clift, abdet);

        double sum01 = t0 + t1;
        double det = sum01 + t2 + te0 + te1 + te2;

        if (std::abs(det) < 1e-10) {
            std::vector<double> terms;
            if (t0 != 0.0) terms.push_back(t0);
            if (te0 != 0.0) terms.push_back(te0);
            if (t1 != 0.0) terms.push_back(t1);
            if (te1 != 0.0) terms.push_back(te1);
            if (t2 != 0.0) terms.push_back(t2);
            if (te2 != 0.0) terms.push_back(te2);
            if (terms.empty()) return 0.0;
            std::sort(terms.begin(), terms.end(),
                      [](double x, double y) { return std::abs(x) > std::abs(y); });
            double acc = 0.0;
            for (double t : terms) acc += t;
            return acc;
        }

        return det;
    }
} // anonymous namespace

// --- orient2D ---------------------------------------------------------------

double RobustPredicates::orient2D(const Vec2& a, const Vec2& b, const Vec2& c) noexcept {
    double acx = a.u - c.u;
    double bcx = b.u - c.u;
    double acy = a.v - c.v;
    double bcy = b.v - c.v;

    double det = acx * bcy - acy * bcx;

    double err_bound = (3.0 + 16.0 * DBL_EPSILON) * DBL_EPSILON *
                       (std::abs(acx * bcy) + std::abs(acy * bcx));

    if (det > err_bound || -det > err_bound) return det;

    return orient2DExact(a, b, c);
}

// --- orient3D ---------------------------------------------------------------

double RobustPredicates::orient3D(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                                  const nexus::render::Vec3& c, const nexus::render::Vec3& d) noexcept {
    double adx = a.x - d.x, ady = a.y - d.y, adz = a.z - d.z;
    double bdx = b.x - d.x, bdy = b.y - d.y, bdz = b.z - d.z;
    double cdx = c.x - d.x, cdy = c.y - d.y, cdz = c.z - d.z;

    double bcdy = bdy * cdz - bdz * cdy;
    double bcdx = bdz * cdx - bdx * cdz;
    double bcdz = bdx * cdy - bdy * cdx;

    double det = adx * bcdy + ady * bcdx + adz * bcdz;

    double ab = std::abs(adx) * (std::abs(bdy * cdz) + std::abs(bdz * cdy)) +
                std::abs(ady) * (std::abs(bdz * cdx) + std::abs(bdx * cdz)) +
                std::abs(adz) * (std::abs(bdx * cdy) + std::abs(bdy * cdx));
    double err_bound = 2.0 * DBL_EPSILON * ab;

    if (det > err_bound || -det > err_bound) return det;

    return orient3DExact(a, b, c, d);
}

// --- inCircle ---------------------------------------------------------------

double RobustPredicates::inCircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) noexcept {
    double adx = a.u - d.u, ady = a.v - d.v;
    double bdx = b.u - d.u, bdy = b.v - d.v;
    double cdx = c.u - d.u, cdy = c.v - d.v;

    double ab = adx * bdy - ady * bdx;
    double bc = bdx * cdy - bdy * cdx;
    double ca = cdx * ady - cdy * adx;

    double alift = adx * adx + ady * ady;
    double blift = bdx * bdx + bdy * bdy;
    double clift = cdx * cdx + cdy * cdy;

    double det = alift * bc + blift * ca + clift * ab;

    double permanent = (std::abs(bc) + std::abs(ca) + std::abs(ab)) * 3.0 * DBL_EPSILON;
    double err_bound = permanent * std::max({alift, blift, clift});

    if (det > err_bound || -det > err_bound) return det;

    return inCircleExact(a, b, c, d);
}

} // namespace nexus::geometry
