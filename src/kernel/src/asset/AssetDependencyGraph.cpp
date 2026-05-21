#include <nexus/asset/AssetDependencyGraph.h>

#include <algorithm>
#include <map>
#include <queue>

namespace nexus::asset {

// static
const std::vector<std::string> AssetDependencyGraph::s_emptyDeps{};

// ─────────────────────────────────────────────────────────────────────────────
//  Node management
// ─────────────────────────────────────────────────────────────────────────────

bool AssetDependencyGraph::addAsset(AssetNodeDesc desc) noexcept
{
    if (desc.path.empty()) { return false; }
    if (m_nodes.contains(desc.path)) { return false; }

    // Avoid relying on function-argument evaluation order here.
    const std::string key = desc.path;
    Node node;
    node.desc = std::move(desc);
    m_nodes.emplace(key, std::move(node));
    return true;
}

bool AssetDependencyGraph::hasAsset(const std::string& path) const noexcept
{
    return m_nodes.contains(path);
}

bool AssetDependencyGraph::removeAsset(const std::string& path) noexcept
{
    return m_nodes.erase(path) > 0u;
}

void AssetDependencyGraph::clear() noexcept
{
    m_nodes.clear();
}

size_t AssetDependencyGraph::assetCount() const noexcept
{
    return m_nodes.size();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Edge management
// ─────────────────────────────────────────────────────────────────────────────

bool AssetDependencyGraph::addDependency(const std::string& path,
                                         const std::string& dependsOnPath) noexcept
{
    if (path == dependsOnPath) { return false; }
    auto it = m_nodes.find(path);
    if (it == m_nodes.end()) { return false; }
    if (!m_nodes.contains(dependsOnPath)) { return false; }

    auto& deps = it->second.dependencies;
    auto depPos = std::lower_bound(deps.begin(), deps.end(), dependsOnPath);
    if (depPos != deps.end() && *depPos == dependsOnPath) {
        return false;  // edge already exists
    }
    deps.insert(depPos, dependsOnPath);
    return true;
}

const std::vector<std::string>& AssetDependencyGraph::dependenciesOf(
    const std::string& path) const noexcept
{
    auto it = m_nodes.find(path);
    if (it == m_nodes.end()) { return s_emptyDeps; }
    return it->second.dependencies;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Topological resolution — Kahn's algorithm
//
//  Edges are stored as "A depends on B" (A → B).
//  For load-order purposes we need B before A, so the sort direction is:
//  in-degree of node X = number of nodes that depend on X (i.e. X appears
//  in their dependency list).
//
//  Nodes with in-degree 0 have nothing depending on them yet and can be
//  placed first only if we were sorting "dependents first".  We want
//  "dependencies first", so we actually process nodes whose out-degree reaches
//  0 in the dependency direction.
//
//  Concretely: re-model as a standard DAG where an edge A→B means "A must
//  come before B in the load order" (i.e. A is a dependency of B).
//  Then Kahn's in-degree is the number of unprocessed dependencies each node
//  still has.
// ─────────────────────────────────────────────────────────────────────────────

DependencyReport AssetDependencyGraph::resolveLoadOrder() const noexcept
{
    DependencyReport report;

    // Detect missing dependency targets first.
    for (const auto& [path, node] : m_nodes) {
        for (const auto& dep : node.dependencies) {
            if (!m_nodes.contains(dep)) {
                report.missing.push_back(dep);
            }
        }
    }
    // Deduplicate missing list.
    std::sort(report.missing.begin(), report.missing.end());
    report.missing.erase(std::unique(report.missing.begin(), report.missing.end()),
                         report.missing.end());

    if (!report.missing.empty()) {
        report.status = DependencyResolutionStatus::MissingDependency;
    }

    // Build in-degree map: how many (registered) dependencies does each node still need?
    // Also build reverse adjacency: dep → list of nodes that depend on it.
    std::map<std::string, int32_t>               inDegree;   // remaining deps count
    std::map<std::string, std::vector<std::string>> dependents; // dep → nodes needing it

    for (const auto& [path, node] : m_nodes) {
        if (!inDegree.contains(path)) {
            inDegree[path] = 0;
        }
        for (const auto& dep : node.dependencies) {
            if (!m_nodes.contains(dep)) { continue; }  // missing — skip in topo pass
            inDegree[path]++;
            dependents[dep].push_back(path);
        }
    }

    // Seed queue with all nodes whose dependencies are already satisfied (in-degree == 0).
    // Use a sorted container for deterministic output.
    std::vector<std::string> ready;
    for (const auto& [path, deg] : inDegree) {
        if (deg == 0) { ready.push_back(path); }
    }
    std::sort(ready.begin(), ready.end());  // lexicographic tier order

    std::queue<std::string> q;
    for (auto& p : ready) { q.push(std::move(p)); }

    while (!q.empty()) {
        std::string current = q.front();
        q.pop();
        report.loadOrder.push_back(current);

        // Collect dependents of 'current' whose in-degree now drops to 0.
        auto it = dependents.find(current);
        if (it != dependents.end()) {
            std::vector<std::string> newReady;
            for (const auto& dependent : it->second) {
                if (--inDegree[dependent] == 0) {
                    newReady.push_back(dependent);
                }
            }
            std::sort(newReady.begin(), newReady.end());
            for (auto& p : newReady) { q.push(std::move(p)); }
        }
    }

    // Any node with remaining in-degree > 0 is part of a cycle.
    for (const auto& [path, deg] : inDegree) {
        if (deg > 0) {
            report.cycles.push_back(path);
        }
    }
    std::sort(report.cycles.begin(), report.cycles.end());

    if (!report.cycles.empty()) {
        report.status = DependencyResolutionStatus::CycleDetected;
    }

    return report;
}

} // namespace nexus::asset
