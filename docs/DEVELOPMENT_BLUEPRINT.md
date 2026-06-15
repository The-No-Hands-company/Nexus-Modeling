# Nexus Systems — Development Blueprint

> Every app with its tech stack, current status, and build path to completion.

## Stack Decision Matrix

| App Category | Backend | Frontend | Database | AI/ML | Real-time |
|---|---|---|---|---|---|
| **Communication** | Rust (axum) | React/Tauri | PostgreSQL + ScyllaDB | — | WebSocket + WebRTC |
| **AI/ML** | Python (FastAPI) | React | PostgreSQL | Hugging Face, Anthropic | WebSocket |
| **Creative/Graphics** | Rust (WASM engine) + Python (FastAPI) | React + WebGL2 | PostgreSQL + S3 | Celery + ComfyUI | WebSocket (CRDT) |
| **Developer Tools** | Rust (axum) or Bun (Elysia) | React | PostgreSQL + SQLite | — | WebSocket |
| **Productivity (web)** | Bun (Bun.serve) | React + Tailwind | SQLite (local) + PostgreSQL (cloud) | Optional | — |
| **Platform/Infra** | Bun (Bun.serve) | — | SQLite + PostgreSQL | — | WebSocket |
| **Security/Privacy** | Rust | — | SQLite (encrypted) | — | P2P (libp2p) |

---

## Tier 1 — Completed (Production-Ready)

| App | Stack | Port | Status |
|-----|-------|------|--------|
| **Nexus** (chat) | Rust + React/Tauri | 8080-82 | Production. E2EE (Signal). Federation. Desktop + mobile clients. Phantom identity layer added |
| **Nexus-Forge** | Bun/Elysia + React | 8090 | Production. Git/SVN/Mercurial/Pijul. PR/issues, CI/CD, 90+ pages |
| **Nexus-Cloud** | Bun + strict TS | 8787 | Control plane. 72 tools registered. Federation gossip. DNS. Caddy routing |
| **Nexus-Guardian** | Bun | 4320 | Security policy. Threat detection. Rule engine. 8 tests |
| **Nexus-Edge** | Bun | 4340 | API gateway. Rate limiting. WAF. Anomaly detection. 3 tests |
| **Nexus-Auth** | Bun | 4310 | Identity provider. OAuth2. RBAC. Sessions. API keys |
| **Nexus-Tunnel** | Bun | 4330 | Network overlay. Proxy. Cloud reconciliation. 6 tests |
| **Nexus-Modeling** | C++23/Vulkan | — | Rendering kernel. 529/534 tests. Vulkan/Metal/DX12 |
| **Phantom** | Rust | P2P | PQ crypto. Oblivious routing. FHE. ZK proofs. Node binary |
| **Nexuslang** | Python | 8765 | Programming language. 1080 tests. Compiler, LSP, runtime |

## Tier 2 — Substantial (Needs Frontend)

| App | Backend | Needs | Port |
|-----|---------|-------|------|
| **Nexus-AI** | Python/FastAPI | React dashboard for model management, RAG, SSO | 8000 |
| **Nexus-Computer** | Python/FastAPI | React terminal UI, agent dashboard | — |
| **Nexusclaw** | Node/Fastify | React agent builder UI | 18800 |
| **Nexus-Deploy** | Express | React deployment dashboard | 5173 |
| **Nexus-Hosting** | pnpm + Rust | Admin panel for workload management | 5173 |
| **Nexus-Network** | Express + WS | Federation health dashboard | — |
| **Nexus-Vault** | Express + SQLite | Secrets management UI | — |
| **Nexus-Engine** | Vite + Babylon.js | Game level editor, asset pipeline | 3000 |

## Tier 3 — Graphics Stack (Engine + Backend + Frontend)

Each of these has the full stack: Rust WASM engine + Python/FastAPI + React/WebGL2.

| App | Engine | Backend | Frontend | Port |
|-----|--------|---------|----------|------|
| **Nexus-Graphic** | Vec2/Matrix3/BBox/Color/bezier | Projects, layers, assets, AI generation | WebGL2 canvas, toolbar, layers, properties | 3080 |
| **Nexus-Design** | Same engine | Projects, frames, layers, components | Design frames, prototype tools | 3074 |
| **Nexus-Draw** | Same engine | Boards, elements, templates | Whiteboard, pen, shapes, arrows | 3075 |
| **Nexus-Photos** | Same engine | Albums, photos, tags, favorites | Photo grid, viewer, filter editor | 3096 |
| **Nexus-Video** | Same engine | Videos, channels, playlists | Video grid, HTML5 player | 3113 |
| **Nexus-GPU-Test** | FLIP diff + Vulkan render | Jobs, workers, results | Dashboard, diff viewer | 3082 |

**Shared engine maintenance:** One Rust WASM crate (`apps/Nexus-Graphic/engine/`) duplicated across all 6. Consolidate into `packages/nexus-graphics-engine/`.

## Tier 4 — Python Backend Apps (Need Frontend)

| App | Backend | Frontend Needed | Port |
|-----|---------|-----------------|------|
| **Nexus-Wiki** | Python/FastAPI + PostgreSQL | React wiki editor + reader | — |
| **Nexus-Email** | Python/FastAPI | React email client | — |
| **Nexus-PDF** | Python/FastAPI | React PDF viewer + editor | — |

## Tier 5 — Bun/SQLite Web Apps (70+ apps, identical pattern)

Every app follows the same architecture. To complete each one:

```
1. Scaffold via Ghost       → bun run ghost/src/index.ts scaffold AppName --port XXXX
2. Symlink node_modules     → ln -sfn ../Nexus-Graphic/node_modules apps/Nexus-AppName/
3. Symlink data             → ln -sfn ../../data apps/Nexus-AppName/data
4. Verify typecheck + test  → bunx tsc --noEmit && bun test
5. Validate contract        → curl -X POST Cloud/api/v1/tools with app payload
6. Add React frontend       → npm create vite@latest frontend -- --template react-ts
```

**Phase 1 — Already scaffolded with Phantom + Discovery + contracts:**

| App | Port | App | Port | App | Port |
|-----|------|-----|------|-----|------|
| Commerce | 3134 | Community | 3135 | Docs | 3136 |
| Editor | 3137 | Inventory | 3138 | Invoice | 3139 |
| Jobs | 3140 | Journal | 3141 | Knowledge | 3142 |
| Logistics | 3143 | Maps | 3144 | News | 3145 |
| Publishing | 3147 | Recipes | 3148 | Remote | 3149 |
| Reporter | 3150 | Reservations | 3151 | Schedule | 3152 |
| Support | — | Portal | — | — | — |

**Phase 2 — Existing scaffolds needing Phantom + Discovery mass-bind:**

| App | Port | App | Port | App | Port |
|-----|------|-----|------|-----|------|
| Academy | 3060 | Accounting | 3061 | Agenda | 3116 |
| Agents | 3117 | AI-Hub | 3114 | Analytics | 3118 |
| API | 3036 | Arcade | 3064 | Automate | 3065 |
| Billing | 3066 | Book | 3067 | Broadcast | 3120 |
| Browsing | 3121 | CRM | 3073 | Calendar | 3068 |
| Code | 3126 | Code-Review | 3069 | Compliance | 3031 |
| Confidential | 3070 | Content | 3071 | Contracts | 3072 |
| Converter | 3130 | Dashboard | 3132 | Data | 3133 |
| Files | 3033 | Finance | 3076 | Fitness | 3077 |
| Forms | 3078 | Game | 3079 | Health | 3081 |
| Home | 3082 | HR | 3083 | IDE | 3084 |
| Inference | 3085 | Insights | 3086 | Learn | 3087 |
| Market | 3088 | Media | 3089 | Meet | 3090 |
| Mind | 3091 | Music | 3092 | Notes | 3093 |
| Nutrition | 3094 | Office | 3095 | Planner | 3097 |
| Play | 3098 | Presence | 3099 | Provenance | 3100 |
| Radio-Live | 3101 | Sales | 3102 | Search | 3034 |
| SEO | 3103 | Social | 3104 | Spend | 3105 |
| Store | 3106 | Survey | 3107 | Tasks | 3108 |
| Team-Chat | 3109 | Terminal | 3110 | Tutor | 3111 |
| Vertical | 3112 | Warehouse | — | — | — |

## Tier 6 — Special Cases

| App | Stack | Plan |
|-----|-------|------|
| **Nexus-Database** | Rust (axum + sqlx) | Database-as-a-service. Needs API routes, connection pooling, backup mgmt |
| **Nexus-Security** | Rust (axum) | Comprehensive security suite. Threat detection, confidential computing, provenance |
| **Nexus-Monitor** | Bun | Ecosystem health monitoring. Needs dashboard frontend |
| **Nexus-Systems-API** | Bun | API contract registry. Needs schema validation engine |
| **Nexus-API** | Bun | API gateway. Needs route aggregation + auth passthrough |
| **Nexus-Compliance** | Bun | Compliance engine. Needs policy templates + audit reports |
| **Nexus-Testing** | Bun | Test runner. Needs CI integration + report generation |
| **Nexus-Warehouse** | Bun | Data warehouse. Needs ETL pipelines + query engine |

---

## Development Priority by Tier

```
Tier 1 → Done (17 apps, production-ready or near-production)
Tier 2 → Frontend for 8 substantial backends
Tier 3 → Consolidate graphics engine, complete 6 graphics apps
Tier 4 → React frontends for 3 Python apps
Tier 5 → Web frontends for 70+ Bun/SQLite apps (mechanical, Ghost-assisted)
Tier 6 → Custom development for 8 special-purpose apps
```

**Next session target:**
- Tier 2: Create React scaffold for Nexus-AI (highest value, existing backend)
- Tier 3: Consolidate `nexus-graphics-engine` into a shared package
- Tier 5: Batch Ghost-scaffold 20 more apps to close the 80+ gap
