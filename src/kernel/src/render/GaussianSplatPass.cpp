// ─────────────────────────────────────────────────────────────────────────────
//  GaussianSplatPass — deterministic per-frame splat accounting
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/GaussianSplatPass.h>

#include <cmath>

namespace nexus::render {

void GaussianSplatPass::addCloud(const nexus::gfx::GaussianSplatSceneNode* node)
{
    if (node == nullptr) {
        return;
    }
    m_clouds.push_back(node);
}

void GaussianSplatPass::clearClouds() noexcept
{
    m_clouds.clear();
}

std::size_t GaussianSplatPass::attachedCloudCount() const noexcept
{
    return m_clouds.size();
}

void GaussianSplatPass::setConfig(const GaussianSplatPassConfig& cfg) noexcept
{
    m_config = cfg;
}

void GaussianSplatPass::setCameraMatrices(const Mat4& view, const Mat4& projection) noexcept
{
    m_config.view       = view;
    m_config.projection = projection;
}

const GaussianSplatPassConfig& GaussianSplatPass::config() const noexcept
{
    return m_config;
}

GaussianSplatPassStats GaussianSplatPass::computeStats() const
{
    GaussianSplatPassStats stats{};
    if (m_clouds.empty()) {
        return stats;
    }

    const Mat4 viewProj = m_config.projection * m_config.view;
    const float opacityCutoff = m_config.opacityCutoffLogit;

    for (const auto* node : m_clouds) {
        if (node == nullptr || !node->visible || node->cloud.splats.empty()) {
            continue;
        }

        const Mat4 mvp = viewProj * node->transform;
        const std::uint32_t cloudSubmitted = static_cast<std::uint32_t>(node->cloud.splats.size());
        std::uint32_t cloudProjected = 0;

        for (const auto& splat : node->cloud.splats) {
            if (splat.opacity < opacityCutoff) {
                continue;
            }

            const Vec4 worldH{ splat.position.x, splat.position.y, splat.position.z, 1.f };
            const Vec4 clip = mvp * worldH;

            // Behind camera or numerically degenerate w.
            if (!(clip.w > 0.f)) {
                continue;
            }
            // Frustum clip in clip space (Vulkan NDC z range [0, w]).
            const float ax = std::fabs(clip.x);
            const float ay = std::fabs(clip.y);
            if (ax > clip.w || ay > clip.w) {
                continue;
            }
            if (clip.z < 0.f || clip.z > clip.w) {
                continue;
            }
            ++cloudProjected;
        }

        stats.submittedSplats += cloudSubmitted;
        stats.projectedSplats += cloudProjected;
        if (cloudProjected > 0) {
            ++stats.splatDrawCalls;
        }
    }

    stats.discardedSplats = stats.submittedSplats - stats.projectedSplats;
    return stats;
}

} // namespace nexus::render
