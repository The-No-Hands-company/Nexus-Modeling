#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Eval Extension — SubgraphRegistry (Slice 5)
//
//  A named catalog of SubgraphTemplate instances.  Templates are indexed by a
//  non-empty token name (same character set as port/node names: alphanumeric,
//  '_', '-', '.') and can be looked up, enumerated, or removed.
//
//  The companion SubgraphRegistrySerializer serializes an entire registry to a
//  single-file text archive ("NEXUS_SUBGRAPH_REGISTRY_V1") using the existing
//  single-template format with template-delimiter records prepended:
//
//    NEXUS_SUBGRAPH_REGISTRY_V1
//    T <template_name>          -- begins a new template section
//    N 1 Constant src           -- node record (same as single-template format)
//    ...
//
//  Forward-compatibility: unknown top-level tags are skipped.
//  An empty registry serializes to a header-only file.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/eval_ext/Subgraph.h>
#include <nexus/eval_ext/SubgraphSerialization.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::eval_ext {

class SubgraphRegistry {
public:
    SubgraphRegistry()  = default;
    ~SubgraphRegistry() = default;

    // ── Mutation ─────────────────────────────────────────────────────────────
    /// Add a template under `name`. Returns false when `name` is already taken
    /// or is not a valid token (empty or contains disallowed characters).
    [[nodiscard]] bool add(std::string name, SubgraphTemplate tmpl);

    /// Remove the template registered under `name`. Returns false if not found.
    bool remove(std::string_view name);

    /// Remove all registered templates.
    void clear() noexcept;

    // ── Query ─────────────────────────────────────────────────────────────────
    /// Look up by name. Returns nullptr when the name is not registered.
    [[nodiscard]] const SubgraphTemplate* find(std::string_view name) const noexcept;

    /// Non-const overload for in-place modification.
    [[nodiscard]] SubgraphTemplate* find(std::string_view name) noexcept;

    /// Returns true when a template is registered under `name`.
    [[nodiscard]] bool contains(std::string_view name) const noexcept;

    /// Returns the number of registered templates.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Returns true when no templates are registered.
    [[nodiscard]] bool empty() const noexcept;

    /// Returns all registered names in ascending lexicographic order.
    [[nodiscard]] std::vector<std::string> names() const;

private:
    std::map<std::string, SubgraphTemplate> m_templates; // ordered by key
};

// ── Registry serialization ────────────────────────────────────────────────────

struct SubgraphRegistrySerializationReport {
    bool ok = true;
    std::vector<std::string> errors;   ///< Sorted lexicographically.
    std::size_t templateCount = 0;     ///< Number of templates written/read.
};

class SubgraphRegistrySerializer {
public:
    /// Serialize the entire registry to a text archive.
    /// Returns an empty string + non-ok report when any template fails validation
    /// or contains invalid token characters.
    [[nodiscard]] static std::string serialize(
        const SubgraphRegistry& registry,
        SubgraphRegistrySerializationReport& report);

    /// Deserialize a string produced by `serialize` into `outRegistry`.
    /// `outRegistry` is cleared before population.
    /// Returns non-ok when the header is missing, any template section fails
    /// to parse, or a template name conflicts (first parse of a name wins; the
    /// duplicate is reported as an error but parsing continues).
    [[nodiscard]] static SubgraphRegistrySerializationReport deserialize(
        const std::string& data, SubgraphRegistry& outRegistry);
};

} // namespace nexus::eval_ext
