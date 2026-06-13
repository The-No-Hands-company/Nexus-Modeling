# Nexus Systems — Architecture Guide

> Ecosystem monotree. 80+ apps. One control plane. Polyglot by design.

---

## 1. Ecosystem Overview

Nexus Systems is a polyglot application ecosystem organized as a single git repository with 89 entries under `apps/`:

| Classification | Count |
|---|---|
| Built (TypeScript, Bun) | 24 |
| Built (Rust, C++, Python) | 6 |
| Empty shells (scaffolded) | 57 |
| Meta (docs, test infra) | 2 |
| **Total** | **89** |

Six apps form the **graphics domain** (Graphic, Design, Draw, Photos, Video, GPU-Test), each using a triple-stack architecture: Bun server → React frontend → Python FastAPI backend → Rust WASM engine.

The **Nexus-Cloud** control plane at port 8787 orchestrates service discovery, topology, routing, and tool registration across all running apps.

---

## 2. Directory Layout

```
Nexus-Systems/
├── .gitmodules              # 9 submodules (some uninitialized)
├── docker-compose.yml       # Infrastructure + Cloud orchestration
├── package.json             # Root workspace (pnpm: packages/*, apps/nexus-*, apps/Nexus-*)
├── run-all.sh               # Launch all ecosystem services
├── e2e-test.sh              # End-to-end ecosystem tests
├── test-integration.sh      # Cross-service integration tests
├── test-apps.sh             # Per-app test runner
├── autopilot.py             # Ecosystem automation/orchestration
│
├── apps/                    # All 89 application directories
│   ├── Nexus-Cloud/         # Control plane (Bun/TS, port 8787)
│   ├── Nexus-Systems-API/   # Cross-service contract definitions
│   ├── Nexus-Graphic/       # Graphics app (Bun+React+Python+Rust)
│   │   ├── src/             # Bun/TS server
│   │   ├── frontend/        # React + Vite (npm package)
│   │   ├── backend/         # Python FastAPI
│   │   └── engine/          # Rust WASM crate
│   ├── Nexus-Design/        # (same structure as Graphic)
│   ├── Nexus-Draw/          # (same structure as Graphic)
│   ├── Nexus-Photos/        # (same structure as Graphic)
│   ├── Nexus-Video/         # (same structure as Graphic)
│   ├── Nexus-GPU-Test/      # (same structure, no Bun server)
│   ├── Nexus/               # Rust monorepo (8 crates, ~66k loc, submodule)
│   ├── Phantom/             # Rust monorepo (8 crates, ~21k loc, submodule)
│   ├── Nexusclaw/           # Hybrid TS/C++ agent framework
│   ├── Nexuslang/           # Python/C++ language compiler
│   ├── Nexus-Modeling/      # C++ geometry kernel (CMake)
│   ├── Nexus-Wiki/          # Python wiki engine
│   ├── docs/                # Ecosystem-level documentation
│   └── tests/               # Root-level test infrastructure
│
├── packages/                # Shared npm packages (workspace)
├── nxl-demos/               # Nexuslang demos
├── dhts/                    # Distributed hash table data
├── data/                    # Mounted data volumes
├── Backups/                 # Backup storage
├── scripts/                 # Build/deploy/ops scripts
└── docs/                    # Top-level ecosystem docs
```

### Standard App Layout (Bun/TS)

```
apps/Nexus-<Name>/
├── AGENTS.md               # AI agent instructions
├── README.md               # App README
├── package.json            # Bun/TS app (name: nexus-<name>, v0.1.0)
├── tsconfig.json           # Strict TypeScript config
├── biome.json              # Biome lint/format config
├── src/
│   ├── index.ts            # Entrypoint
│   ├── server.ts           # HTTP server
│   ├── <name>-engine.ts    # Domain logic
│   ├── contracts.ts        # API contracts
│   └── cloud.ts            # Cloud registration + heartbeat
└── tests/
    └── server.test.ts      # Test harness
```

### Graphics App Layout (triple-stack)

```
apps/Nexus-<App>/
├── src/                    # Bun/TS gateway server
├── frontend/               # React + Vite (package.json with react ^19.0)
├── backend/                # Python FastAPI (pyproject.toml, requires-python >=3.12)
└── engine/                 # Rust WASM crate (Cargo.toml, cdylib + rlib)
```

---

## 3. Port Allocation Scheme

| Range | Purpose |
|---|---|
| **8787** | Nexus-Cloud control plane |
| **3000–3099** | Core infrastructure apps (Hosting, Vault, Forge, Engine, Deploy, Monitor) |
| **3100–3199** | Reserved (expansion) |
| **3200–3299** | Network/security apps (Network, Tunnel, Edge, Files, Guardian, Auth) |
| **3300–3399** | API/meta apps (Systems-API, Compliance, Search, AI-Hub, Testing, API, IDE) |
| **3400–3499** | AI apps (Nexus-AI) |
| **3500–3599** | Computing apps (Nexus-Computer) |
| **3600–3699** | Graphics triple-stack apps (Graphic, Design, Draw, Photos, Video, GPU-Test) |
| **3700–3999** | Shell apps (assigned at build time) |
| **5432** | PostgreSQL |
| **6379** | Redis |
| **9000/9001** | MinIO (S3 API / Console) |

Port assignments are registered in `src/server.ts` per app and discoverable via `GET /api/v1/topology` from Nexus-Cloud.

---

## 4. Stack Per Domain

| Domain | Stack | Rationale |
|---|---|---|
| **Standard apps** (Office, Spend, CRM, etc.) | Bun + SQLite + TypeScript (strict) | Fast startup, zero-dependency local DB, unified language |
| **Graphics** (Graphic, Design, Draw, Photos, Video, GPU-Test) | Bun gateway + React 19 + Python/FastAPI + Rust WASM | React for UI responsiveness, Python for async task queues (Celery), Rust for compute-intensive rendering |
| **Security / Crypto** (Vault, Guardian) | Rust / C++ | Memory safety for secrets, zero-cost abstractions for encryption |
| **AI / ML** (Nexus-AI, AI-Hub, Inference) | Python / FastAPI | Ecosystem maturity for ML libraries, FastAPI for high-throughput inference APIs |
| **Networking** (Network, Tunnel, Edge) | Bun + TypeScript | Efficient I/O via Bun's native HTTP, low-latency event loop |
| **Modeling / Simulation** | C++ (CMake) + Vulkan | Geometry kernels, GPU compute, performance-critical simulation |
| **Language Tooling** (Nexuslang) | Python + C++ | Python for compiler frontend, C++ for runtime performance |
| **Wiki** | Python | Python ecosystem for document processing, templates, search |

---

## 5. Service Discovery

### Nexus-Cloud — The Nerve System

Nexus-Cloud (port 8787) is the single control plane for all service discovery:

```
App Startup Flow:
  1. App boots → reads NEXUS_CLOUD_URL from env
  2. App calls POST /api/v1/tools → registers with capability tags
  3. App begins heartbeat loop → POST /api/v1/tools/:toolId/heartbeat (interval: 30s)
  4. Cloud maintains live topology in memory + PostgreSQL
```

### Discovery APIs (all at Nexus-Cloud)

| Endpoint | Method | Returns |
|---|---|---|
| `/api/v1/tools` | GET | All registered tools with health state |
| `/api/v1/tools` | POST | Register new tool |
| `/api/v1/tools/:toolId/heartbeat` | POST | Heartbeat ping |
| `/api/v1/topology` | GET | Full ecosystem graph (apps + connections) |
| `/api/v1/routes` | GET | Active domain-to-upstream routing table |
| `/api/v1/addresses` | POST | Request domain/exposure for an app |
| `/api/v1/status` | GET | Healthy tool count, live state |

### Systems API Contracts

The `apps/Nexus-Systems-API/` package defines cross-service contract types and client utilities. Every app imports and consumes these:

- `Client` class — typed HTTP client for Nexus-Cloud Systems API
- `Contracts` types — Tool, Topology, Route, Address, Heartbeat schemas
- `Examples` — reference usage patterns

---

## 6. Data Flow

```
                         ┌──────────────────────────┐
                         │     Nexus-Cloud :8787     │
                         │  ┌────────────────────┐   │
                         │  │  Service Registry   │   │
                         │  │  Topology Graph     │   │
                         │  │  Route Table        │   │
                         │  │  Audit Trail        │   │
                         │  └────────┬───────────┘   │
                         └───────────┼───────────────┘
                                     │
                    ┌────────────────┼────────────────┐
                    │ register       │ heartbeat      │ query
                    ▼                ▼                ▼
        ┌──────────────────┐  ┌──────────┐  ┌──────────────────┐
        │   App (any)      │  │  App N   │  │   Dashboard /     │
        │  ┌────────────┐  │  │  liveness│  │   CLI tool        │
        │  │ cloud.ts   │──┼──│  pulse   │  │                   │
        │  │ contracts  │  │  └──────────┘  │ GET /api/v1/      │
        │  │ engine     │  │                │ topology          │
        │  └────────────┘  │                │ routes            │
        └──────┬───────────┘                │ status            │
               │                            └──────────────────┘
               │ fetch contracts (Systems API)
               ▼
        ┌──────────────────┐
        │   Other App      │
        │  ┌────────────┐  │
        │  │ contracts  │  │
        │  └────────────┘  │
        └──────────────────┘

   App → Cloud:  POST /api/v1/tools         (register)
                 POST /api/v1/tools/:id/beat (heartbeat)
   App → Cloud:  POST /api/v1/addresses     (exposure)

   Any → Cloud:  GET  /api/v1/topology      (map)
                 GET  /api/v1/routes        (routing)
                 GET  /api/v1/status        (health)

   App → App:    fetch contracts via Nexus-Systems-API package
                 (typed, versioned, no ad-hoc JSON)
```

### Graphics App Internal Flow

```
  Browser (React 19)
       │ WebSocket (Yjs for collab) + REST
       ▼
  Bun Gateway (port 36xx)
       │ HTTP
       ├──▶ Python FastAPI backend (uvicorn)
       │       ├──▶ PostgreSQL (SQLAlchemy async)
       │       ├──▶ Redis (Celery task queue)
       │       └──▶ MinIO/S3 (asset storage)
       │
       └──▶ Rust WASM Engine (loaded in browser)
               (compute-heavy ops: filters, transforms, rendering)
```

---

## 7. Git Submodule Status

| Submodule | Path | Remote | Status |
|---|---|---|---|
| **Nexus** | apps/Nexus | github.com:The-No-Hands-company/Nexus.git | Initialized |
| **Nexus-AI** | apps/Nexus-AI | github.com:The-No-Hands-company/Nexus-AI | Initialized |
| **Nexus-Cloud** | apps/Nexus-Cloud | github.com:The-No-Hands-company/Nexus-Cloud.git | Initialized |
| **Nexus-Computer** | apps/Nexus-Computer | github.com:The-No-Hands-company/Nexus-Computer.git | Initialized |
| **Nexus-Deploy** | apps/Nexus-Deploy | github.com:The-No-Hands-company/Nexus-Deploy.git | Initialized |
| **Nexus-Hosting** | apps/Nexus-Hosting | github.com:The-No-Hands-company/Nexus-Hosting.git | Initialized |
| **Nexus-Network** | apps/Nexus-Network | github.com:The-No-Hands-company/Nexus-Network.git | **Not initialized** — must clone separately |
| **Nexus-Vault** | apps/Nexus-Vault | github.com:The-No-Hands-company/Nexus-Vault.git | **Not initialized** — must clone separately |
| **Phantom** | apps/Phantom | github.com:Zajfan/Phantom.git | **Not initialized** — must clone separately |

### To initialize uninitialized submodules:

```bash
git submodule update --init apps/Nexus-Network
git submodule update --init apps/Nexus-Vault
git submodule update --init apps/Phantom
```

Or all at once:

```bash
git submodule update --init --recursive
```

---

## 8. Production Deployment Options

### Option A: Docker Compose (recommended)

```bash
# Start infrastructure + Cloud
docker compose up -d postgres redis minio nexus-cloud

# Start all app services (generated from app definitions)
docker compose up -d

# Check health
docker compose ps
curl http://localhost:8787/api/v1/status
```

### Option B: systemd (single-machine)

Each app runs as a systemd service unit:

```ini
# /etc/systemd/system/nexus-cloud.service
[Unit]
Description=Nexus Cloud Control Plane
After=network.target postgresql.service redis.service

[Service]
Type=simple
User=nexus
WorkingDirectory=/opt/nexus-systems/apps/Nexus-Cloud
Environment=NODE_ENV=production
Environment=PORT=8787
Environment=POSTGRES_URL=postgresql://nexus:nexus@localhost:5432/nexus
Environment=REDIS_URL=redis://localhost:6379
Environment=S3_ENDPOINT=http://localhost:9000
ExecStart=/usr/bin/bun run src/index.ts
Restart=always

[Install]
WantedBy=multi-user.target
```

App services follow the same pattern, each on its assigned port.

### Option C: Single-Machine (direct)

```bash
# Start infrastructure daemons
systemctl start postgresql redis minio

# Launch Cloud
cd apps/Nexus-Cloud && bun run dev &

# Launch apps (each in its own terminal or tmux pane)
cd apps/Nexus-Office && bun run dev &
cd apps/Nexus-Spend && bun run dev &
# ...

# Monitor
curl http://localhost:8787/api/v1/status
```

### Infrastructure Requirements

| Component | Min Version | Notes |
|---|---|---|
| PostgreSQL | 16 | Required by Cloud + Python backends |
| Redis | 7 | Required by Cloud + Celery workers |
| MinIO | latest | S3-compatible object storage |
| Bun | ^1.3 | Runtime for all TypeScript services |
| Python | >=3.12 | Required by graphics backends + AI |
| Rust | stable (edition 2021) | Required for engine crates |

### Environment Variables (global)

```bash
NEXUS_CLOUD_URL=http://localhost:8787
NEXUS_CLOUD_API_KEY=change-me
CORS_ORIGIN=*
POSTGRES_URL=postgresql://nexus:nexus@localhost:5432/nexus
REDIS_URL=redis://localhost:6379
S3_ENDPOINT=http://localhost:9000
S3_ACCESS_KEY=minioadmin
S3_SECRET_KEY=minioadmin
S3_BUCKET=nexus
```
