#include <nexus/parametric/ParametricSolver.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace nexus::parametric {

namespace {

bool isFiniteDouble(double value) noexcept
{
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t kExpMask = 0x7FF0000000000000ULL;
    return (bits & kExpMask) != kExpMask;
}

double vecLen(double x, double y, double z) noexcept
{
    return std::sqrt(x * x + y * y + z * z);
}

double clampAbs(double v, double limit) noexcept
{
    if (v < -limit) return -limit;
    if (v > limit) return limit;
    return v;
}

} // namespace

ParametricSolverReport ParametricSolver::solve(ConstraintGraph& graph,
                                               const ParametricSolverConfig& config) noexcept
{
    ParametricSolverReport report{};

    if (config.maxIterations == 0) {
        report.converged = false;
        report.errors.push_back("maxIterations must be greater than zero");
        return report;
    }

    if (config.convergenceEpsilon < 0.0) {
        report.converged = false;
        report.errors.push_back("convergenceEpsilon must be non-negative");
        return report;
    }

    if (!isFiniteDouble(config.convergenceEpsilon)) {
        report.converged = false;
        report.errors.push_back("convergenceEpsilon must be finite");
        std::sort(report.errors.begin(), report.errors.end());
        return report;
    }

    const size_t totalConstraints = graph.totalConstraintCount();
    if (totalConstraints == 0) {
        return report;
    }

    // Precompute entity constraint counts for symmetric weighting.
    std::unordered_map<ParametricEntityId, double> entityWeight;
    for (const auto& c : graph.distanceConstraints()) {
        entityWeight[c.entityA] += 1.0;
        entityWeight[c.entityB] += 1.0;
    }
    for (const auto& c : graph.coincidentConstraints()) {
        entityWeight[c.entityA] += 1.0;
        entityWeight[c.entityB] += 1.0;
    }
    for (const auto& c : graph.axisAlignedDistanceConstraints()) {
        entityWeight[c.entityA] += 1.0;
        entityWeight[c.entityB] += 1.0;
    }
    for (const auto& c : graph.angleConstraints()) {
        entityWeight[c.entityA] += 1.0;
        entityWeight[c.entityB] += 1.0;
        entityWeight[c.entityC] += 1.0;
    }
    for (const auto& c : graph.equalDistanceConstraints()) {
        entityWeight[c.entityA] += 1.0;
        entityWeight[c.entityB] += 1.0;
        entityWeight[c.entityC] += 1.0;
        entityWeight[c.entityD] += 1.0;
    }
    for (const auto& c : graph.pointOnLineConstraints()) {
        entityWeight[c.entityP] += 1.0;
        entityWeight[c.entityL0] += 1.0;
        entityWeight[c.entityL1] += 1.0;
    }
    for (const auto& c : graph.sketchPlaneConstraints()) {
        entityWeight[c.entityPlane] += 1.0;
        entityWeight[c.entityPoint] += 1.0;
    }

    // Max correction per iteration to prevent overshoot.
    constexpr double kMaxCorrection = 100.0;
    double previousError = std::numeric_limits<double>::max();
    double adaptiveMaxCorrection = kMaxCorrection;

    report.converged = false;

    for (uint32_t iteration = 0; iteration < config.maxIterations; ++iteration) {
        double iterationMaxError = 0.0;

        // ── Distance constraints (symmetric Gauss-Seidel) ────────────────
        for (const DistanceConstraint& c : graph.distanceConstraints()) {
            const ParametricPoint3* pa = graph.point(c.entityA);
            const ParametricPoint3* pb = graph.point(c.entityB);
            if (!pa || !pb) {
                report.errors.push_back("distance constraint references missing entity");
                continue;
            }

            double dx = pb->x - pa->x;
            double dy = pb->y - pa->y;
            double dz = pb->z - pa->z;
            const double currentDist = vecLen(dx, dy, dz);
            const double error = std::fabs(currentDist - c.targetDistance);
            iterationMaxError = std::max(iterationMaxError, error);

            if (currentDist <= std::numeric_limits<double>::epsilon()) {
                ParametricPoint3 movedA = *pa;
                ParametricPoint3 movedB = *pb;
                movedA.x -= c.targetDistance * 0.5;
                movedB.x += c.targetDistance * 0.5;
                (void)graph.setPoint(c.entityA, movedA);
                (void)graph.setPoint(c.entityB, movedB);
                continue;
            }

            if (config.symmetricRelaxation) {
                const double wA = 1.0 / (1.0 + entityWeight[c.entityA]);
                const double wB = 1.0 / (1.0 + entityWeight[c.entityB]);
                const double totalW = wA + wB;
                const double fA = (totalW > 0.0) ? wB / totalW : 0.5;
                const double fB = (totalW > 0.0) ? wA / totalW : 0.5;

                const double correction = (currentDist - c.targetDistance);
                const double clampedCorr = clampAbs(correction, adaptiveMaxCorrection);

                const double invLen = 1.0 / currentDist;
                const double mx = dx * invLen * clampedCorr;
                const double my = dy * invLen * clampedCorr;
                const double mz = dz * invLen * clampedCorr;

            (void)graph.setPoint(c.entityA, {pa->x + mx * fA, pa->y + my * fA, pa->z + mz * fA});
            (void)graph.setPoint(c.entityB, {pb->x - mx * fB, pb->y - my * fB, pb->z - mz * fB});
            } else {
                const double invLen = 1.0 / currentDist;
                (void)graph.setPoint(c.entityB, {
                    pa->x + dx * invLen * c.targetDistance,
                    pa->y + dy * invLen * c.targetDistance,
                    pa->z + dz * invLen * c.targetDistance
                });
            }
        }

        // ── Coincident constraints (move both toward midpoint) ────────────
        for (const CoincidentConstraint& c : graph.coincidentConstraints()) {
            const ParametricPoint3* pa = graph.point(c.entityA);
            const ParametricPoint3* pb = graph.point(c.entityB);
            if (!pa || !pb) {
                report.errors.push_back("coincident constraint references missing entity");
                continue;
            }

            const double dx = pb->x - pa->x;
            const double dy = pb->y - pa->y;
            const double dz = pb->z - pa->z;
            const double dist = vecLen(dx, dy, dz);
            iterationMaxError = std::max(iterationMaxError, dist);

            if (config.symmetricRelaxation) {
                const ParametricPoint3 mid = {
                    (pa->x + pb->x) * 0.5,
                    (pa->y + pb->y) * 0.5,
                    (pa->z + pb->z) * 0.5
                };
                (void)graph.setPoint(c.entityA, mid);
                (void)graph.setPoint(c.entityB, mid);
            } else {
                (void)graph.setPoint(c.entityB, *pa);
            }
        }

        // ── Axis-aligned distance constraints (symmetric) ─────────────────
        for (const AxisAlignedDistanceConstraint& c : graph.axisAlignedDistanceConstraints()) {
            const ParametricPoint3* pa = graph.point(c.entityA);
            const ParametricPoint3* pb = graph.point(c.entityB);
            if (!pa || !pb) {
                report.errors.push_back("axis-aligned constraint references missing entity");
                continue;
            }

            double axisError = 0.0;
            double currentVal = 0.0;
            if (c.axis == Axis::X) {
                currentVal = pb->x - pa->x;
                axisError = std::fabs(currentVal - c.targetDistance);
            } else if (c.axis == Axis::Y) {
                currentVal = pb->y - pa->y;
                axisError = std::fabs(currentVal - c.targetDistance);
            } else {
                currentVal = pb->z - pa->z;
                axisError = std::fabs(currentVal - c.targetDistance);
            }
            iterationMaxError = std::max(iterationMaxError, axisError);

            if (config.symmetricRelaxation) {
                const double wA = 1.0 / (1.0 + entityWeight[c.entityA]);
                const double wB = 1.0 / (1.0 + entityWeight[c.entityB]);
                const double totalW = wA + wB;
                const double fA = (totalW > 0.0) ? wB / totalW : 0.5;

                const double correction = (currentVal - c.targetDistance);
                const double clampedCorr = clampAbs(correction, adaptiveMaxCorrection);

                ParametricPoint3 movedA = *pa;
                ParametricPoint3 movedB = *pb;
                const double delta = clampedCorr * fA;
                if (c.axis == Axis::X) {
                    movedA.x = pa->x + delta;
                    movedB.x = pb->x - (clampedCorr - delta);
                } else if (c.axis == Axis::Y) {
                    movedA.y = pa->y + delta;
                    movedB.y = pb->y - (clampedCorr - delta);
                } else {
                    movedA.z = pa->z + delta;
                    movedB.z = pb->z - (clampedCorr - delta);
                }
                (void)graph.setPoint(c.entityA, movedA);
                (void)graph.setPoint(c.entityB, movedB);
            } else {
                ParametricPoint3 corrected = *pb;
                if (c.axis == Axis::X) {
                    corrected.x = pa->x + c.targetDistance;
                } else if (c.axis == Axis::Y) {
                    corrected.y = pa->y + c.targetDistance;
                } else {
                    corrected.z = pa->z + c.targetDistance;
                }
                (void)graph.setPoint(c.entityB, corrected);
            }
        }

        // ── Angle constraints ─────────────────────────────────────────────
        for (const AngleConstraint& c : graph.angleConstraints()) {
            const ParametricPoint3* pa = graph.point(c.entityA);
            const ParametricPoint3* pb = graph.point(c.entityB);
            const ParametricPoint3* pc = graph.point(c.entityC);
            if (!pa || !pb || !pc) {
                report.errors.push_back("angle constraint references missing entity");
                continue;
            }

            double abx = pa->x - pb->x;
            double aby = pa->y - pb->y;
            double abz = pa->z - pb->z;
            double cbx = pc->x - pb->x;
            double cby = pc->y - pb->y;
            double cbz = pc->z - pb->z;

            const double abLen = vecLen(abx, aby, abz);
            const double cbLen = vecLen(cbx, cby, cbz);

            if (abLen < 1e-15 || cbLen < 1e-15) {
                report.errors.push_back("angle constraint: degenerate triangle");
                continue;
            }

            const double dot = (abx * cbx + aby * cby + abz * cbz) / (abLen * cbLen);
            const double currentAngle = std::acos(std::clamp(dot, -1.0, 1.0));
            const double targetRad = c.targetAngleDegrees * (std::numbers::pi / 180.0);
            const double error = std::fabs(currentAngle - targetRad);
            iterationMaxError = std::max(iterationMaxError, error);

            const double angleDiff = currentAngle - targetRad;
            if (std::fabs(angleDiff) < 1e-12) continue;

            // Rotate entityA and entityC around entityB to correct the angle.
            // Compute the normal of the plane formed by the three points.
            const double nx = aby * cbz - abz * cby;
            const double ny = abz * cbx - abx * cbz;
            const double nz = abx * cby - aby * cbx;
            const double nLen = vecLen(nx, ny, nz);

            if (nLen < 1e-15) {
                // Points are collinear — cannot determine rotation axis.
                continue;
            }

            const double halfCorrection = angleDiff * 0.5;
            const double clampedCorr = clampAbs(halfCorrection, adaptiveMaxCorrection * 0.25);

            // Apply rotation around normal to both A and C relative to B.
            const double cosA = std::cos(clampedCorr);
            const double sinA = std::sin(clampedCorr);
            const double invN = 1.0 / nLen;
            const double ux = nx * invN, uy = ny * invN, uz = nz * invN;

            // Rotate AB and CB in opposite directions so the angle between them changes.
            const double abDot = abx * ux + aby * uy + abz * uz;
            const double abrx = abx * cosA + (uy * abz - uz * aby) * sinA + ux * abDot * (1.0 - cosA);
            const double abry = aby * cosA + (uz * abx - ux * abz) * sinA + uy * abDot * (1.0 - cosA);
            const double abrz = abz * cosA + (ux * aby - uy * abx) * sinA + uz * abDot * (1.0 - cosA);

            // Rotate CB in the opposite direction to close/open the angle.
            const double cbDot = cbx * ux + cby * uy + cbz * uz;
            const double cbrx = cbx * cosA - (uy * cbz - uz * cby) * sinA + ux * cbDot * (1.0 - cosA);
            const double cbry = cby * cosA - (uz * cbx - ux * cbz) * sinA + uy * cbDot * (1.0 - cosA);
            const double cbrz = cbz * cosA - (ux * cby - uy * cbx) * sinA + uz * cbDot * (1.0 - cosA);

            (void)graph.setPoint(c.entityA, {pb->x + abrx, pb->y + abry, pb->z + abrz});
            (void)graph.setPoint(c.entityC, {pb->x + cbrx, pb->y + cbry, pb->z + cbrz});
        }

        // ── Equal-distance constraints ────────────────────────────────────
        for (const EqualDistanceConstraint& c : graph.equalDistanceConstraints()) {
            const auto* pa = graph.point(c.entityA);
            const auto* pb = graph.point(c.entityB);
            const auto* pc = graph.point(c.entityC);
            const auto* pd = graph.point(c.entityD);
            if (!pa || !pb || !pc || !pd) {
                report.errors.push_back("equal-distance constraint references missing entity");
                continue;
            }

            const double d1 = vecLen(pb->x - pa->x, pb->y - pa->y, pb->z - pa->z);
            const double d2 = vecLen(pd->x - pc->x, pd->y - pc->y, pd->z - pc->z);
            const double error = std::fabs(d1 - d2);
            iterationMaxError = std::max(iterationMaxError, error);

            // Scale the second pair to match the first pair's distance.
            if (d2 > 1e-15) {
                const double ratio = d1 / d2;
                const double cx = (pc->x + pd->x) * 0.5;
                const double cy = (pc->y + pd->y) * 0.5;
                const double cz = (pc->z + pd->z) * 0.5;
                const double hdx = (pd->x - pc->x) * 0.5 * ratio;
                const double hdy = (pd->y - pc->y) * 0.5 * ratio;
                const double hdz = (pd->z - pc->z) * 0.5 * ratio;
                (void)graph.setPoint(c.entityC, {cx - hdx, cy - hdy, cz - hdz});
                (void)graph.setPoint(c.entityD, {cx + hdx, cy + hdy, cz + hdz});
            }
        }

        // ── Point-on-line constraints ──────────────────────────────────
        for (const PointOnLineConstraint& c : graph.pointOnLineConstraints()) {
            const auto* pp = graph.point(c.entityP);
            const auto* pl0 = graph.point(c.entityL0);
            const auto* pl1 = graph.point(c.entityL1);
            if (!pp || !pl0 || !pl1) {
                report.errors.push_back("point-on-line constraint references missing entity");
                continue;
            }

            const double dx = pl1->x - pl0->x;
            const double dy = pl1->y - pl0->y;
            const double dz = pl1->z - pl0->z;
            const double lineLenSq = dx*dx + dy*dy + dz*dz;

            if (lineLenSq < 1e-15) {
                report.errors.push_back("point-on-line constraint: degenerate line");
                continue;
            }

            // Project P onto the line L0-L1.
            const double vx = pp->x - pl0->x;
            const double vy = pp->y - pl0->y;
            const double vz = pp->z - pl0->z;
            const double t = (vx*dx + vy*dy + vz*dz) / lineLenSq;

            // Closest point on line.
            const double cx = pl0->x + t * dx;
            const double cy = pl0->y + t * dy;
            const double cz = pl0->z + t * dz;

            // Perpendicular offset from P to the line.
            const double px = cx - pp->x;
            const double py = cy - pp->y;
            const double pz = cz - pp->z;
            const double error = vecLen(px, py, pz);
            iterationMaxError = std::max(iterationMaxError, error);

            if (error < 1e-15) continue;

            if (config.symmetricRelaxation) {
                const double wP = 1.0 / (1.0 + entityWeight[c.entityP]);
                const double wL0 = 1.0 / (1.0 + entityWeight[c.entityL0]);
                const double wL1 = 1.0 / (1.0 + entityWeight[c.entityL1]);
                const double totalW = wP + wL0 + wL1;
                const double fP = (totalW > 0.0) ? (wL0 + wL1) / (2.0 * totalW) : 1.0 / 3.0;
                const double fL0 = (totalW > 0.0) ? (wP + wL1) / (2.0 * totalW) : 1.0 / 3.0;
                const double fL1 = (totalW > 0.0) ? (wP + wL0) / (2.0 * totalW) : 1.0 / 3.0;

                (void)graph.setPoint(c.entityP, {pp->x + px * fP, pp->y + py * fP, pp->z + pz * fP});
                (void)graph.setPoint(c.entityL0, {pl0->x - px * fL0, pl0->y - py * fL0, pl0->z - pz * fL0});
                (void)graph.setPoint(c.entityL1, {pl1->x - px * fL1, pl1->y - py * fL1, pl1->z - pz * fL1});
            } else {
                (void)graph.setPoint(c.entityP, {cx, cy, cz});
            }
        }

        // ── Sketch-plane constraints ────────────────────────────────────
        for (const SketchPlaneConstraint& c : graph.sketchPlaneConstraints()) {
            const auto* pplane = graph.point(c.entityPlane);
            const auto* ppoint = graph.point(c.entityPoint);
            if (!pplane || !ppoint) {
                report.errors.push_back("sketch-plane constraint references missing entity");
                continue;
            }

            const double error = std::fabs(ppoint->z - pplane->z);
            iterationMaxError = std::max(iterationMaxError, error);

            if (error < 1e-15) continue;

            if (config.symmetricRelaxation) {
                const double wPlane = 1.0 / (1.0 + entityWeight[c.entityPlane]);
                const double wPoint = 1.0 / (1.0 + entityWeight[c.entityPoint]);
                const double totalW = wPlane + wPoint;
                const double fPoint = (totalW > 0.0) ? wPlane / totalW : 0.5;

                const double correction = (ppoint->z - pplane->z);
                const double clampedCorr = clampAbs(correction, adaptiveMaxCorrection);
                const double delta = clampedCorr * fPoint;

                ParametricPoint3 movedPlane = *pplane;
                ParametricPoint3 movedPoint = *ppoint;
                movedPlane.z = pplane->z + (clampedCorr - delta);
                movedPoint.z = ppoint->z - delta;
                (void)graph.setPoint(c.entityPlane, movedPlane);
                (void)graph.setPoint(c.entityPoint, movedPoint);
            } else {
                ParametricPoint3 movedPoint = *ppoint;
                movedPoint.z = pplane->z;
                (void)graph.setPoint(c.entityPoint, movedPoint);
            }
        }

        report.iterationsRan = iteration + 1;
        report.maxConstraintError = iterationMaxError;

        // Adaptive convergence: scale epsilon by average constraint magnitude.
        double scaleEpsilon = config.convergenceEpsilon;
        (void)scaleEpsilon; // available for future use

        // Oscillation detection: if error increased, reduce step size.
        if (iterationMaxError > previousError * 1.05 && adaptiveMaxCorrection > 0.1) {
            adaptiveMaxCorrection *= 0.5;
        } else if (iterationMaxError < previousError * 0.9 && adaptiveMaxCorrection < kMaxCorrection) {
            // Error decreasing well — slowly restore correction if needed.
            adaptiveMaxCorrection = std::min(adaptiveMaxCorrection * 1.1, kMaxCorrection);
        }
        previousError = iterationMaxError;

        if (iterationMaxError <= config.convergenceEpsilon && report.errors.empty()) {
            report.converged = true;
            break;
        }
    }

    return report;
}

} // namespace nexus::parametric
