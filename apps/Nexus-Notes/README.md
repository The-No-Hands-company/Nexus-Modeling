# Nexus-Notes

## Purpose

Collaborative note-taking and knowledge base with markdown support

## Quick Start

```bash
bun install
bun run dev
```

## Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Health check |
| GET | /api/v1/status | Service status and contracts |

## Configuration

| Variable | Default | Purpose |
|----------|---------|---------|
| PORT | 3093 | Listen port |
| NEXUS_CLOUD_URL | http://localhost:8787 | Cloud control plane |
| NEXUS_CLOUD_API_KEY | (none) | Cloud API key |
| `NEXUS_NOTES_ENABLE_CLOUD_INTEGRATION` | true | Enable/disable cloud heartbeat |
