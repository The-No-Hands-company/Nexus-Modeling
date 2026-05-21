#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Asset — Asset Dependency Graph
//
//  Tracks named assets and directed dependency edges ("A depends on B") so
//  that a deterministic, topologically-sorted load order can be derived.
//
//  Design constraints (Month 6 v0 scope):
//  - Pure data contract — no GPU objects, no file I/O.
//  - Deterministic output: paths ordered lexicographically within each tier.
//  - Cycle and missing-dependency detection reported without throwing.
//  - Headless-safe; all paths are noexcept.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nexus::asset {

// ─────────────────────────────────────────────────────────────────────────────
//  Asset node descriptor
// ─────────────────────────────────────────────────────────────────────────────
struct AssetNodeDesc {
    // Unique stable key used in dependency edges (typically a canonical path).
    std::string path;
    // Optional human-readable label; does not affect resolution.
    std::string name;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Resolution result
// ─────────────────────────────────────────────────────────────────────────────
enum class DependencyResolutionStatus : uint8_t {
    Success,          // loadOrder is complete and valid
    CycleDetected,    // one or more dependency cycles exist; loadOrder is partial
    MissingDependency // one or more dependency targets are not registered
};

struct DependencyReport {
    DependencyResolutionStatus status = DependencyResolutionStatus::Success;

    // Paths in dependency-first load order.  Populated even when status != Success
    // for the resolvable portion of the graph.
    std::vector<std::string> loadOrder;

    // Paths that form or are involved in cycles (only populated when cycles exist).
    std::vector<std::string> cycles;

    // Dependency target paths that were referenced but never registered.
    std::vector<std::string> missing;

    [[nodiscard]] bool valid() const noexcept
    {
        return status == DependencyResolutionStatus::Success;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  AssetDependencyGraph
// ─────────────────────────────────────────────────────────────────────────────
class AssetDependencyGraph {
public:
    AssetDependencyGraph()  = default;
    ~AssetDependencyGraph() = default;

    // Registers an asset node. Returns false if path is empty or already registered.
    [[nodiscard]] bool addAsset(AssetNodeDesc desc) noexcept;

    // Returns true if the asset path is registered.
    [[nodiscard]] bool hasAsset(const std::string& path) const noexcept;

    // Removes an asset node and all outgoing edges from it.
    // Incoming edges that reference this path are left intact (reported as missing
    // on the next resolveLoadOrder call). Returns false if not found.
    [[nodiscard]] bool removeAsset(const std::string& path) noexcept;

    // Removes all nodes and edges.
    void clear() noexcept;

    // Number of registered asset nodes.
    [[nodiscard]] size_t assetCount() const noexcept;

    // Adds a directed dependency: 'path' depends on 'dependsOnPath'.
    // Both paths must be registered. Returns false if either is unknown,
    // they are the same, or the edge already exists.
    [[nodiscard]] bool addDependency(const std::string& path,
                                     const std::string& dependsOnPath) noexcept;

    // Returns the direct dependencies of the given asset (paths it depends on).
    // Dependency paths are kept in lexicographic order for deterministic reads.
    // Returns an empty vector if the path is not registered.
    [[nodiscard]] const std::vector<std::string>& dependenciesOf(
        const std::string& path) const noexcept;

    // Resolves a deterministic load order via Kahn's topological sort.
    // - Dependencies are placed before their dependents.
    // - Nodes at the same tier are ordered lexicographically for determinism.
    // - Cycles are detected and reported in DependencyReport::cycles.
    // - Missing dependency targets (registered as edges but not as nodes) are
    //   reported in DependencyReport::missing.
    [[nodiscard]] DependencyReport resolveLoadOrder() const noexcept;

private:
    struct Node {
        AssetNodeDesc          desc;
        std::vector<std::string> dependencies; // edges: this asset depends on these
    };

    // std::map gives deterministic iteration order (lexicographic).
    std::map<std::string, Node> m_nodes;

    // Returned by reference for out-of-range dependenciesOf queries.
    static const std::vector<std::string> s_emptyDeps;
};

} // namespace nexus::asset
