# Month 6 Asset and Pipeline Core Checklist

This checklist executes the Month 6 roadmap commitment in
[vision-and-roadmap.md](vision-and-roadmap.md).

Primary slice: Scene/Package Format Versioning, Migration Policy Metadata,
Baseline Scene Adapter, and Deterministic Mesh Import Baseline.

## Scope Lock

In scope:

1. Versioned package-manifest format for deterministic multi-scene load plans.
2. Public migration hook API for package format upgrades.
3. Deterministic migration policy metadata:
   - target version
   - strict/lenient mode flags
   - migration audit trail
4. Importer support for manifest-driven package loads.
5. Baseline scene import/export text adapter for one scene format v0.
6. Deterministic mesh import/export baseline for OBJ.
7. Deterministic tests for package manifest round-trip and migration behavior.

Out of scope:

1. Full scene/package fidelity adapters for external DCC ecosystems.
2. Advanced migration orchestration tooling beyond per-version step functions.
3. Content diff/merge tooling for package manifests.

## Work Packages

## R6.1 Versioned Package Manifest Contract

Status: Complete

Delivered:

1. Added package format constants and descriptor contracts in
   `nexus/asset/SceneAsset.h`:
   - `kSceneAssetPackageMagic`
   - `kSceneAssetPackageVersionCurrent`
   - `SceneAssetPackageDescriptor`
   - `SceneAssetPackageIOReport`
2. Added package diagnostics and helper predicate:
   - `SceneAssetPackageDiagnostic`
   - `hasPackageDiagnostic(...)`
3. Added package manifest binary I/O APIs:
   - `SceneAsset::savePackageManifest(...)`
   - `SceneAsset::loadPackageManifest(...)`

Done when:

1. Package manifests can be saved/loaded deterministically with explicit magic
   and version headers.

## R6.2 Package Migration Hooks

Status: Complete

Delivered:

1. Added package migration registration API in `nexus/asset/SceneAsset.h`:
   - `registerPackageMigration(...)`
   - `resetBuiltinPackageMigrations()`
2. Implemented migration registry and built-in v0->v1 package migration in
   `src/kernel/src/asset/SceneAsset.cpp`.
3. Ensured migration chain execution and deterministic diagnostics on failure.
4. Added migration policy metadata and execution reporting:
   - target version
   - strict/lenient mode flags
   - per-step migration audit trail

Done when:

1. Legacy package bytes can be upgraded to current package format via migration
   hooks, and migration failures are reported deterministically.

## R6.3 Migration Policy Metadata and Audit Trail

Status: Complete

Delivered:

1. Added policy metadata in `nexus/asset/SceneAsset.h`:
   - `SceneAssetPackageMigrationPolicy`
   - `SceneAssetPackageMigrationModeFlags`
2. Added migration audit contract:
   - `SceneAssetPackageMigrationAuditEntry`
   - `SceneAssetPackageIOReport::migrationAuditTrail`
3. Upgraded package format to v2 with header metadata fields and built-in
   migration v1->v2 in `src/kernel/src/asset/SceneAsset.cpp`.
4. Threaded migration policy through importer options for manifest loads.

Done when:

1. Callers can deterministically control migration target/mode and inspect a
   deterministic migration audit trail.

## R6.4 Importer Integration

Status: Complete

Delivered:

1. Added importer API:
   - `SceneAssetImporter::importPackageManifest(...)`
2. Implemented manifest load + scene import wiring in
   `src/kernel/src/asset/SceneAssetImporter.cpp`.

Done when:

1. A manifest file can drive package scene imports through the existing importer
   contract.

## R6.5 Baseline Scene Format Adapter

Status: Complete

Delivered:

1. Added `SceneAssetTextAdapter` public API in
   `nexus/asset/SceneAssetTextAdapter.h`.
2. Implemented deterministic text scene export/import in
   `src/kernel/src/asset/SceneAssetTextAdapter.cpp`.
3. Registered adapter in umbrella header and API freeze manifest.

Done when:

1. One deterministic scene format adapter can round-trip scene payloads through
   a stable public contract.

## R6.6 Deterministic Mesh Import Baseline

Status: Complete

Delivered:

1. Added deterministic OBJ import support to `nexus/geometry/MeshIO.h`:
   - `MeshImportFormat`
   - `MeshImportDiagnostic`
   - `MeshImportOptions`
   - `MeshImportReport`
   - `MeshIO::importMesh(...)`
2. Implemented deterministic OBJ import parsing in `src/kernel/src/geometry/MeshIO.cpp`.
3. Added regression tests for export/import round-trip, normal preservation,
   and malformed OBJ rejection in `tests/kernel/test_MeshIO.cpp`.

Done when:

1. The core mesh export path has a deterministic import baseline for OBJ and
   the resulting behavior is covered by tests.

## Test Coverage

Implemented in:

1. `tests/kernel/test_SceneAsset.cpp`
   - package manifest round-trip
   - legacy v0 manifest migration path
   - migration failure diagnostics
   - strict/lenient migration policy behavior
   - migration policy metadata persistence
   - migration audit trail checks
2. `tests/kernel/test_SceneAssetImporter.cpp`
   - importer path for manifest-driven dependency-ordered loads
   - importer failure path for missing manifest file
3. `tests/kernel/test_SceneAssetTextAdapter.cpp`
   - text scene round-trip behavior
   - deterministic export behavior
   - invalid format rejection behavior
4. `tests/kernel/test_MeshIO.cpp`
   - OBJ export/import round-trip behavior
   - imported normal preservation from exported OBJ payloads
   - malformed OBJ rejection behavior

## Month 6 Exit Gate (This Slice)

This Month 6 slice is complete only when all are true:

1. Package format is explicitly versioned and migration-capable.
2. Migration policy metadata and audit trail are exposed in public APIs.
3. Importer exposes manifest-driven package import as public API.
4. At least one scene format adapter is available and deterministic.
5. Deterministic tests cover round-trip, migration, policy, and importer integration.
6. Build and test quality gates pass in the local workspace.

Status: COMPLETE

Test baseline: 756 discovered / 756 passed / 0 failed (10 skipped by capability gating).
