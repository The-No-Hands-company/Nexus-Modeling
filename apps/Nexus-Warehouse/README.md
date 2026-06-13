# Nexus Warehouse

Nexus Warehouse — inventory storage service

## Quick Start

```bash
bun install
bun dev        # start with hot reload
bun test       # run tests
bun run check  # typecheck + lint
```

## Endpoints

- `GET /health` — health check

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3050` | HTTP listen port |
| `SERVICE_NAME` | `nexus-warehouse` | Service name in logs |
