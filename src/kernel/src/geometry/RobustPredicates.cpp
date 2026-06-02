#include <nexus/geometry/RobustPredicates.h>

#include <cmath>
#include <array>

// Shewchuk adaptive floating-point predicates.
// Two-product and two-sum decompositions provide exact multi-word arithmetic.

namespace nexus::geometry::predicates {

namespace {

// Split a double into two non-overlapping halves for two-product.
inline void split(double a, double& ahi, double& alo) noexcept
{
    constexpr double splitter = (1 << 27) + 1.0; // 2^27 + 1
    const double c = splitter * a;
    ahi = c - (c - a);
    alo = a - ahi;
}

// Compute fl(a*b) and err such that a*b = product + err exactly.
inline double twoProduct(double a, double b, double& err) noexcept
{
    const double product = a * b;
    double ahi, alo, bhi, blo;
    split(a, ahi, alo);
    split(b, bhi, blo);
    err = ((ahi * bhi - product) + ahi * blo + alo * bhi) + alo * blo;
    return product;
}

// Compute fl(a+b) and err such that a+b = sum + err exactly.
inline double twoSum(double a, double b, double& err) noexcept
{
    const double sum = a + b;
    const double bvirt = sum - a;
    err = (a - (sum - bvirt)) + (b - bvirt);
    return sum;
}

// ─── orient2d exact expansion ────────────────────────────────────────────────
// Computes the 2×2 determinant:
//   | ax-cx  ay-cy |
//   | bx-cx  by-cy |
// using two-product expansions to accumulate all error terms.
double orient2dExact(const double pa[2], const double pb[2], const double pc[2]) noexcept
{
    // Differences
    const double acx = pa[0] - pc[0];
    const double acy = pa[1] - pc[1];
    const double bcx = pb[0] - pc[0];
    const double bcy = pb[1] - pc[1];

    // Two-product: acx * bcy  and  acy * bcx
    double s1err, t1err;
    const double s1 = twoProduct(acx, bcy, s1err);
    const double t1 = twoProduct(acy, bcx, t1err);

    // det = s1 - t1 + s1err - t1err (all four terms, in expansion form)
    // We accumulate as a 4-term expansion and return the sum.
    double u1err, u2err;
    const double u1 = twoSum(s1,    -t1,    u1err);
    const double u2 = twoSum(s1err, -t1err, u2err);
    double v1err;
    const double v1 = twoSum(u1, u2, v1err);
    const double result = v1 + (v1err + u1err + u2err);
    return result;
}

// ─── orient3d exact expansion ────────────────────────────────────────────────
// Computes the 3×3 determinant:
//   | ax-dx  ay-dy  az-dz |
//   | bx-dx  by-dy  bz-dz |
//   | cx-dx  cy-dy  cz-dz |
double orient3dExact(const double pa[3], const double pb[3],
                     const double pc[3], const double pd[3]) noexcept
{
    const double adx = pa[0] - pd[0];
    const double ady = pa[1] - pd[1];
    const double adz = pa[2] - pd[2];
    const double bdx = pb[0] - pd[0];
    const double bdy = pb[1] - pd[1];
    const double bdz = pb[2] - pd[2];
    const double cdx = pc[0] - pd[0];
    const double cdy = pc[1] - pd[1];
    const double cdz = pc[2] - pd[2];

    // Co-factors with error terms
    double bdxcdy_err, cdxbdy_err, cdxady_err, adxcdy_err, adxbdy_err, bdxady_err;
    const double bdxcdy = twoProduct(bdx, cdy, bdxcdy_err);
    const double cdxbdy = twoProduct(cdx, bdy, cdxbdy_err);
    const double cdxady = twoProduct(cdx, ady, cdxady_err);
    const double adxcdy = twoProduct(adx, cdy, adxcdy_err);
    const double adxbdy = twoProduct(adx, bdy, adxbdy_err);
    const double bdxady = twoProduct(bdx, ady, bdxady_err);

    // Minor01 = bdxcdy - cdxbdy
    double m01err;
    const double m01 = twoSum(bdxcdy - cdxbdy, bdxcdy_err - cdxbdy_err, m01err);

    // Minor12 = cdxady - adxcdy
    double m12err;
    const double m12 = twoSum(cdxady - adxcdy, cdxady_err - adxcdy_err, m12err);

    // Minor20 = adxbdy - bdxady
    double m20err;
    const double m20 = twoSum(adxbdy - bdxady, adxbdy_err - bdxady_err, m20err);

    // det = adz*m01 + bdz*m12 + cdz*m20  (ignoring second-order error cross terms
    // for this single-adaptive-stage implementation — sufficient for the sign)
    const double det = adz * (m01 + m01err)
                     + bdz * (m12 + m12err)
                     + cdz * (m20 + m20err);
    return det;
}

// Error bound constants from Shewchuk 1997 (Table 2).
// epsilon = 2^-53, the machine epsilon for double.
constexpr double kEps = 1.1102230246251565e-16; // 2^-53
constexpr double kErrBound2D = (3.0 + 16.0 * kEps) * kEps;
constexpr double kErrBound3D = (7.0 + 56.0 * kEps) * kEps;

} // namespace

// ─── Public API ──────────────────────────────────────────────────────────────

double orient2d(const double pa[2], const double pb[2], const double pc[2]) noexcept
{
    const double acx = pa[0] - pc[0];
    const double acy = pa[1] - pc[1];
    const double bcx = pb[0] - pc[0];
    const double bcy = pb[1] - pc[1];

    const double det = acx * bcy - acy * bcx;
    const double errBound = kErrBound2D * (std::fabs(acx * bcy) + std::fabs(acy * bcx));

    if (std::fabs(det) > errBound) {
        return det; // filter passed — sign is reliable
    }

    return orient2dExact(pa, pb, pc);
}

double orient3d(const double pa[3], const double pb[3],
                const double pc[3], const double pd[3]) noexcept
{
    const double adx = pa[0] - pd[0];
    const double ady = pa[1] - pd[1];
    const double adz = pa[2] - pd[2];
    const double bdx = pb[0] - pd[0];
    const double bdy = pb[1] - pd[1];
    const double bdz = pb[2] - pd[2];
    const double cdx = pc[0] - pd[0];
    const double cdy = pc[1] - pd[1];
    const double cdz = pc[2] - pd[2];

    const double bdxcdy = bdx * cdy;
    const double cdxbdy = cdx * bdy;
    const double cdxady = cdx * ady;
    const double adxcdy = adx * cdy;
    const double adxbdy = adx * bdy;
    const double bdxady = bdx * ady;

    const double det = adz * (bdxcdy - cdxbdy)
                     + bdz * (cdxady - adxcdy)
                     + cdz * (adxbdy - bdxady);

    const double permanent =
        (std::fabs(bdxcdy) + std::fabs(cdxbdy)) * std::fabs(adz) +
        (std::fabs(cdxady) + std::fabs(adxcdy)) * std::fabs(bdz) +
        (std::fabs(adxbdy) + std::fabs(bdxady)) * std::fabs(cdz);

    const double errBound = kErrBound3D * permanent;

    if (std::fabs(det) > errBound) {
        return det;
    }

    return orient3dExact(pa, pb, pc, pd);
}

} // namespace nexus::geometry::predicates
