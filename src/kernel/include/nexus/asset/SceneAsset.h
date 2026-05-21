#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Asset — Scene Asset v0
//
//  SceneAsset is a versioned, serialisable container for a scene description:
//  a flat list of named mesh entries, each with a transform and an optional
//  source path reference for the round-trip import/export pipeline.
//
//  Design constraints (Month 6 v0 scope):
//  - No runtime GPU or render objects — this is a pure data contract.
//  - Format version field + migration hook for forward compatibility.
//  - Deterministic load/save for CI round-trip testing.
//  - Headless-safe; all paths are noexcept.
//
//  Format on disk: custom binary with magic + version header.
//  Human-readable text export is available via SceneAsset::exportText().
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/asset/AssetDependencyGraph.h>
#include <nexus/render/Camera.h>  // Vec3, Vec4

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace nexus::asset {

using Vec3 = nexus::render::Vec3;
using Vec4 = nexus::render::Vec4;

// ─────────────────────────────────────────────────────────────────────────────
//  Version constants
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr uint32_t kSceneAssetMagic        = 0x4E584153u;  // "NXAS"
inline constexpr uint32_t kSceneAssetVersionCurrent = 1u;
inline constexpr uint32_t kSceneAssetPackageMagic = 0x4E58504Bu;  // "NXPK"
inline constexpr uint32_t kSceneAssetPackageVersionCurrent = 2u;

// ─────────────────────────────────────────────────────────────────────────────
//  Decomposed transform (matches render::Transform without the runtime dep)
// ─────────────────────────────────────────────────────────────────────────────
struct AssetTransform {
    Vec3 translation = {0.f, 0.f, 0.f};
    Vec4 rotation    = {0.f, 0.f, 0.f, 1.f};  // quaternion xyzw
    Vec3 scale       = {1.f, 1.f, 1.f};
};

// ─────────────────────────────────────────────────────────────────────────────
//  Scene mesh entry
// ─────────────────────────────────────────────────────────────────────────────
struct SceneMeshEntry {
    // Stable human-readable identifier for this entry.
    std::string name;
    // Optional originating source path (e.g., OBJ file path). May be empty.
    std::string sourcePath;
    // Embedded mesh geometry.
    nexus::geometry::Mesh mesh;
    // Entry transform in scene space.
    AssetTransform transform;
    // Whether this entry is visible in a viewport.
    bool visible = true;
};

// ─────────────────────────────────────────────────────────────────────────────
//  I/O diagnostic
// ─────────────────────────────────────────────────────────────────────────────
enum class SceneAssetDiagnostic : uint32_t {
    Success           = 0,
    FileOpenFailed    = 1u << 0,
    InvalidMagic      = 1u << 1,
    UnsupportedVersion = 1u << 2,
    WriteError        = 1u << 3,
    ReadError         = 1u << 4,
    InvalidData       = 1u << 5,
    MigrationFailed   = 1u << 6,
};

inline bool hasDiagnostic(SceneAssetDiagnostic val, SceneAssetDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct SceneAssetIOReport {
    SceneAssetDiagnostic diagnostic = SceneAssetDiagnostic::Success;
    bool                 valid      = false;
    uint32_t             version    = 0;
    uint32_t             entryCount = 0;
    /// @note messages is lexicographically sorted on every return path so
    ///       that multi-diagnostic output is deterministic regardless of
    ///       the order in which individual conditions are detected.
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == SceneAssetDiagnostic::Success;
    }
};

struct SceneAssetPackageEntry {
    // Unique package key, typically the SceneAsset file path used during load.
    std::string path;
    // Optional label for diagnostics only.
    std::string name;
    // Dependency paths that must be loaded before this path.
    std::vector<std::string> dependsOn;
};

enum class SceneAssetPackageMigrationModeFlags : uint32_t {
    None    = 0,
    Strict  = 1u << 0,
    Lenient = 1u << 1,
};

inline SceneAssetPackageMigrationModeFlags operator|(
    SceneAssetPackageMigrationModeFlags a,
    SceneAssetPackageMigrationModeFlags b) noexcept
{
    return static_cast<SceneAssetPackageMigrationModeFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool hasMigrationModeFlag(SceneAssetPackageMigrationModeFlags val,
                                 SceneAssetPackageMigrationModeFlags flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct SceneAssetPackageMigrationPolicy {
    uint32_t targetVersion = kSceneAssetPackageVersionCurrent;
    SceneAssetPackageMigrationModeFlags modeFlags =
        SceneAssetPackageMigrationModeFlags::Strict;
};

struct SceneAssetPackageMigrationAuditEntry {
    uint32_t fromVersion = 0;
    uint32_t toVersion = 0;
    bool success = false;
    std::string message;
};

struct SceneAssetPackageDescriptor {
    uint32_t version = kSceneAssetPackageVersionCurrent;
    SceneAssetPackageMigrationPolicy migrationPolicy;
    std::vector<SceneAssetPackageEntry> entries;
};

enum class SceneAssetPackageDiagnostic : uint32_t {
    Success            = 0,
    FileOpenFailed     = 1u << 0,
    InvalidMagic       = 1u << 1,
    UnsupportedVersion = 1u << 2,
    WriteError         = 1u << 3,
    ReadError          = 1u << 4,
    InvalidData        = 1u << 5,
    MigrationFailed    = 1u << 6,
};

inline bool hasPackageDiagnostic(SceneAssetPackageDiagnostic val,
                                 SceneAssetPackageDiagnostic flag) noexcept
{
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct SceneAssetPackageIOReport {
    SceneAssetPackageDiagnostic diagnostic = SceneAssetPackageDiagnostic::Success;
    bool valid = false;
    uint32_t sourceVersion = 0;
    uint32_t version = 0;
    uint32_t targetVersion = 0;
    SceneAssetPackageMigrationModeFlags modeFlags =
        SceneAssetPackageMigrationModeFlags::Strict;
    uint32_t entryCount = 0;
    std::vector<SceneAssetPackageMigrationAuditEntry> migrationAuditTrail;
    std::vector<std::string> messages;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return diagnostic == SceneAssetPackageDiagnostic::Success;
    }
};

struct SceneAssetPackageReport {
    bool valid = false;
    DependencyReport dependencyReport;
    std::vector<std::string> loadOrder;
    std::vector<std::string> loadedPaths;
    std::vector<std::string> failedPaths;
    std::map<std::string, SceneAssetIOReport> loadReports;
    /// @note messages is lexicographically sorted on every return path so
    ///       that multi-diagnostic output is deterministic regardless of
    ///       the order in which individual conditions are detected.
    std::vector<std::string> messages;
};

// ─────────────────────────────────────────────────────────────────────────────
//  SceneAsset — versioned scene container
// ─────────────────────────────────────────────────────────────────────────────
class SceneAsset {
public:
    using MigrationFn = std::function<bool(uint32_t fromVersion, std::vector<uint8_t>& rawBytes)>;
    using PackageMigrationFn = std::function<bool(uint32_t fromVersion,
                                                  std::vector<uint8_t>& rawBytes)>;

    SceneAsset()  = default;
    ~SceneAsset() = default;

    // SceneAsset is movable but not copyable (owns mesh data).
    SceneAsset(const SceneAsset&)            = delete;
    SceneAsset& operator=(const SceneAsset&) = delete;
    SceneAsset(SceneAsset&&)                 = default;
    SceneAsset& operator=(SceneAsset&&)      = default;

    // Returns a deterministic, canonical empty SceneAsset that callers
    // (tests, tooling, CI fixtures) can use as a stable round-trip baseline
    // without relying on the on-disk format being byte-stable across kernel
    // versions. Contract: sceneName() == "empty", entryCount() == 0,
    // version() == kSceneAssetVersionCurrent. The exact identity of these
    // fields is part of the public API and is covered by a kernel
    // regression test; treat it as stable.
    [[nodiscard]] static SceneAsset canonicalEmpty() noexcept;

    // ── Entry management ──────────────────────────────────────────────────
    void addEntry(SceneMeshEntry entry);
    void removeEntry(size_t index) noexcept;
    void clear() noexcept;

    [[nodiscard]] size_t               entryCount() const noexcept { return m_entries.size(); }
    [[nodiscard]] const SceneMeshEntry& entry(size_t i) const noexcept { return m_entries[i]; }
    [[nodiscard]] SceneMeshEntry&       entry(size_t i)       noexcept { return m_entries[i]; }

    // Returns nullptr if no entry has the given name.
    [[nodiscard]] const SceneMeshEntry* findByName(const std::string& name) const noexcept;
    [[nodiscard]] SceneMeshEntry*       findByName(const std::string& name)       noexcept;

    // ── Metadata ─────────────────────────────────────────────────────────
    [[nodiscard]] const std::string& sceneName()  const noexcept { return m_sceneName; }
    void                              setSceneName(std::string name) { m_sceneName = std::move(name); }
    [[nodiscard]] uint32_t            version()    const noexcept { return kSceneAssetVersionCurrent; }

    // ── Binary I/O ────────────────────────────────────────────────────────
    // Serialises this asset to a binary file at 'path'.
    [[nodiscard]] SceneAssetIOReport save(const std::string& path) const noexcept;

    // Registers migration handler for fromVersion -> fromVersion + 1.
    // A newer registration for the same fromVersion replaces the previous one.
    static bool registerMigration(uint32_t fromVersion, MigrationFn fn) noexcept;

    // Restores the built-in migration set (e.g. v0->v1). Primarily useful in
    // tests that overwrite migrations and need to restore production defaults.
    static void resetBuiltinMigrations() noexcept;

    // ── Package manifest I/O (versioned binary) ─────────────────────────
    // Binary layout v2:
    // [magic][version][targetVersion][modeFlags][entryCount]
    // entry: [path][name][dependsCount][dependsOn...]
    [[nodiscard]] static SceneAssetPackageIOReport savePackageManifest(
        const SceneAssetPackageDescriptor& package,
        const std::string& path) noexcept;

    [[nodiscard]] static SceneAssetPackageIOReport loadPackageManifest(
        const std::string& path,
        SceneAssetPackageDescriptor& outPackage,
        const SceneAssetPackageMigrationPolicy& policy = {}) noexcept;

    // Registers migration for package version step fromVersion -> fromVersion + 1.
    static bool registerPackageMigration(uint32_t fromVersion,
                                         PackageMigrationFn fn) noexcept;

    // Restores built-in package migrations (e.g. v0->v1).
    static void resetBuiltinPackageMigrations() noexcept;

    // Deserialises a binary file into this asset (replaces current contents).
    [[nodiscard]] SceneAssetIOReport load(const std::string& path) noexcept;

    // Loads a deterministic package of scene assets using explicit dependencies.
    // The package graph is resolved via AssetDependencyGraph.
    // - On dependency errors (missing/cycle), no files are loaded.
    // - On file-load errors, independent assets continue to load and failures are reported.
    // outAssets is cleared before population and keyed by entry.path.
    [[nodiscard]] static SceneAssetPackageReport loadPackage(
        const std::vector<SceneAssetPackageEntry>& packageEntries,
        std::map<std::string, SceneAsset>& outAssets) noexcept;

    // ── Text export (human-readable, not re-importable) ───────────────────
    // Writes a text summary of the scene to 'path'. Always overwrites.
    [[nodiscard]] SceneAssetIOReport exportText(const std::string& path) const noexcept;

private:
    std::string                  m_sceneName;
    std::vector<SceneMeshEntry>  m_entries;
};

} // namespace nexus::asset
