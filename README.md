# Nexus Modeling

Nexus Modeling is the geometry and rendering application in Nexus Systems, targeting production-scale DCC workflows (Blender/Rhino/Maya class) with a Vulkan-first C++23 kernel.

## Current state

- Core graphics kernel is live and buildable.
- Vulkan and Null backends are available.
- Frame scheduler, GPU resources, shader compilation, scene graph, and camera systems are implemented.
- Renderer has active GBuffer, shadow atlas, and composite pass scaffolding with real command recording and draw submission hooks.
- Mesh-shader submission is wired for geometry and shadow paths, including raster fallback behavior when mesh pipelines are unavailable.
- Inline production mesh-shader GLSL source headers are now part of the public API surface:
	- `src/kernel/include/nexus/render/GBufferMeshShaders.h`
	- `src/kernel/include/nexus/render/ShadowMeshShaders.h`
- Test suite currently runs 709 tests with expected capability-based Vulkan skips on unsupported hardware.

## Repository layout

```
Nexus-Modeling/
├── AGENTS.md
├── README.md
├── docs/
├── src/
│   └── kernel/
│       ├── include/nexus/   # public API surface
│       └── src/             # internal implementation
└── tests/
	 └── kernel/
```

## Build and test

From repository root:

1. Configure:
	- cmake -S . -B build
2. Build:
	- cmake --build build -j$(nproc)
3. Test:
	- ctest --test-dir build --output-on-failure
4. Alpha release gate and signoff report:
	- ./tools/release_gate_alpha.sh
5. Alpha tag helper (requires overall_signoff=PASS report):
	- ./tools/create_alpha_tag.sh --report build/release_signoff_1.0-alpha.txt --tag v1.0.0-alpha

## Design goals

- Production-grade GPU backend correctness (lifetime, synchronization, transitions).
- API-first architecture that can scale to a multi-million-line codebase.
- Headless/CI compatibility via Null backend.
- Extensible render pipeline for deferred, hybrid RT, and neural workflows.

## Near-term execution order

1. Build default Vulkan pipeline bootstrap helpers for inline mesh-shader sources to reduce external pipeline setup burden.
2. Expand material/descriptor binding to support full lighting/composite sampling contracts.
3. Keep scheduler path as the production render path and de-scope non-scheduler parity.
4. Extend mesh-shader production pass coverage (hardware-gated) and continue RT parity hardening.
5. Continue test and documentation expansion as features land.

## Render path policy

- Production rendering is scheduler-driven.
- The non-scheduler path remains a minimal compatibility fallback and is intentionally not feature-parity with scheduler execution.
- New rendering features must land on the scheduler path first with explicit transitions and tests.

## Documentation index

See docs/README.md for architecture, build, shader, design rationale, and testing strategy documents.

Project process and collaboration docs:

- ROADMAP.md
- CONTRIBUTING.md
- docs/PRD.md
- docs/SDD.md
- docs/FRD.md
- docs/kernel-capability-map.md
