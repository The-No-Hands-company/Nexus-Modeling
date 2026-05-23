#include <nexus/sim/SimulationDriver.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace nexus::sim {

namespace {

// NOTE: the kernel builds with -ffast-math, so std::isfinite / std::isnan are
// unreliable (the compiler may assume operands are finite). Detect non-finite
// values by inspecting the IEEE-754 exponent bits directly, matching the
// convention used in SimulationCore.cpp.

inline bool isFiniteFloatBits(float v) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(v);
    constexpr std::uint32_t kExpMask = 0x7F800000u;
    return (bits & kExpMask) != kExpMask;
}

inline bool isPositiveFinite(double v) noexcept
{
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(v);
    constexpr std::uint64_t kSignMask = 0x8000000000000000ULL;
    constexpr std::uint64_t kExpMask  = 0x7FF0000000000000ULL;
    constexpr std::uint64_t kMagMask  = 0x7FFFFFFFFFFFFFFFULL;
    const bool finite   = (bits & kExpMask) != kExpMask;
    const bool positive = (bits & kSignMask) == 0ULL;
    const bool nonZero  = (bits & kMagMask)  != 0ULL;
    return finite && positive && nonZero;
}

inline SimVec3 lerp(const SimVec3& a, const SimVec3& b, float t) noexcept
{
    return a + (b - a) * t;
}

/// Normalized linear interpolation between unit quaternions, taking the
/// shortest arc. Adequate for blending between adjacent fixed-step orientations.
inline SimQuat nlerp(const SimQuat& a, SimQuat b, float t) noexcept
{
    // Exact endpoints: with -ffast-math the renormalize below uses an approximate
    // reciprocal-sqrt, so even a unit input drifts. Short-circuiting keeps the
    // endpoints bit-exact (and avoids needless work).
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;

    const float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (dot < 0.0f) {                 // shortest path: flip b
        b = {-b.x, -b.y, -b.z, -b.w};
    }
    SimQuat r{
        a.x + t * (b.x - a.x),
        a.y + t * (b.y - a.y),
        a.z + t * (b.z - a.z),
        a.w + t * (b.w - a.w)
    };
    const float lenSq = r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w;
    if (!isFiniteFloatBits(lenSq) || lenSq <= 0.0f) {
        return SimQuat{}; // identity fallback
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    return {r.x*invLen, r.y*invLen, r.z*invLen, r.w*invLen};
}

inline SimBodySnapshot interpolateBody(const SimBodySnapshot& a,
                                       const SimBodySnapshot& b,
                                       float t) noexcept
{
    SimBodySnapshot r;
    r.id              = a.id;
    r.position        = lerp(a.position, b.position, t);
    r.velocity        = lerp(a.velocity, b.velocity, t);
    r.orientation     = nlerp(a.orientation, b.orientation, t);
    r.angularVelocity = lerp(a.angularVelocity, b.angularVelocity, t);
    return r;
}

std::vector<SimBodySnapshot> sortedById(std::vector<SimBodySnapshot> bodies)
{
    std::sort(bodies.begin(), bodies.end(),
              [](const SimBodySnapshot& l, const SimBodySnapshot& r) { return l.id < r.id; });
    return bodies;
}

} // namespace

SimState interpolateSimState(const SimState& from, const SimState& to, float alpha)
{
    const float t = std::clamp(alpha, 0.0f, 1.0f);

    SimState out;
    out.simulationTime = from.simulationTime
                       + (to.simulationTime - from.simulationTime) * static_cast<double>(t);

    const std::vector<SimBodySnapshot> a = sortedById(from.bodies);
    const std::vector<SimBodySnapshot> b = sortedById(to.bodies);
    out.bodies.reserve(std::max(a.size(), b.size()));

    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].id == b[j].id) {
            out.bodies.push_back(interpolateBody(a[i], b[j], t));
            ++i; ++j;
        } else if (a[i].id < b[j].id) {
            out.bodies.push_back(a[i]); // present only in `from`
            ++i;
        } else {
            out.bodies.push_back(b[j]); // present only in `to`
            ++j;
        }
    }
    for (; i < a.size(); ++i) out.bodies.push_back(a[i]);
    for (; j < b.size(); ++j) out.bodies.push_back(b[j]);

    return out;
}

SimulationDriver::SimulationDriver(double fixedTimestep,
                                   std::uint32_t maxSubstepsPerAdvance) noexcept
    : m_fixedDt(fixedTimestep)
    , m_maxSubsteps(maxSubstepsPerAdvance)
{
}

void SimulationDriver::reset(const RigidBodySolver& solver)
{
    m_accumulator = 0.0;
    m_current     = solver.captureState();
    m_previous    = m_current;
    m_initialized = true;
}

std::uint32_t SimulationDriver::advance(RigidBodySolver& solver, double frameDt)
{
    if (!m_initialized) {
        reset(solver);
    }
    if (!isPositiveFinite(frameDt) || !isPositiveFinite(m_fixedDt)) {
        return 0;
    }

    m_accumulator += frameDt;

    std::uint32_t steps = 0;
    while (m_accumulator >= m_fixedDt && steps < m_maxSubsteps) {
        m_previous = m_current;
        const StepReport report = solver.step(m_fixedDt);
        if (!report.ok) {
            break; // solver rejected the step; leave current state untouched
        }
        m_current = solver.captureState();
        m_accumulator -= m_fixedDt;
        ++steps;
    }

    // Spiral-of-death guard: if the substep cap was hit with time still owing,
    // discard the backlog so alpha stays in [0,1] and the loop cannot diverge.
    if (m_accumulator > m_fixedDt) {
        m_accumulator = m_fixedDt;
    }

    return steps;
}

float SimulationDriver::alpha() const noexcept
{
    if (!(m_fixedDt > 0.0)) {
        return 0.0f;
    }
    const double a = m_accumulator / m_fixedDt;
    return static_cast<float>(std::clamp(a, 0.0, 1.0));
}

SimState SimulationDriver::interpolatedState() const
{
    if (!m_initialized) {
        return SimState{};
    }
    return interpolateSimState(m_previous, m_current, alpha());
}

} // namespace nexus::sim
