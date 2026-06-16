#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
# Nexus Systems — Production Deployer for nexussystems.vexr.dev
#
# Usage:
#   ./deploy.sh              # Start everything (foreground, Ctrl+C to stop)
#   ./deploy.sh --bg         # Start in background
#   ./deploy.sh --stop       # Stop all services
#   ./deploy.sh --status     # Check what's running
#
# Prerequisites: bun, git clone of Nexus-Systems
#
# DNS setup:
#   Point nexussystems.vexr.dev     → your server IP
#   Point cloud.nexussystems.vexr.dev  → your server IP
#   Point chat.nexussystems.vexr.dev   → your server IP
#
# For HTTPS (optional):
#   Install Caddy or use Cloudflare Tunnel
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPLOY="$ROOT/deploy/production"
LOG_DIR="/tmp/nexus-production"
PID_DIR="$LOG_DIR/pids"
mkdir -p "$LOG_DIR" "$PID_DIR"

export DOMAIN="${DOMAIN:-nexussystems.vexr.dev}"
export PROXY_PORT="${PROXY_PORT:-8080}"

G="\033[32m" Y="\033[33m" R="\033[0m"
log()  { echo -e "${G}[nexus]${R} $*"; }
warn() { echo -e "${Y}[nexus]${R} $*"; }

# ── Commands ──

cmd_start() {
  log "Starting Nexus Systems on $DOMAIN..."

  # Nexus-Cloud
  log "Nexus-Cloud → :8787"
  cd "$ROOT/apps/Nexus-Cloud"
  NEXUS_CLOUD_API_KEY="${NEXUS_CLOUD_API_KEY:-nexus-deploy-key}" \
  PORT=8787 CORS_ORIGIN="*" \
  nohup bun run src/index.ts > "$LOG_DIR/cloud.log" 2>&1 &
  echo $! > "$PID_DIR/cloud.pid"

  # Nexus-Team-Chat
  log "Nexus-Team-Chat → :3109"
  cd "$ROOT/apps/Nexus-Team-Chat"
  PORT=3109 \
  nohup bun run src/index.ts > "$LOG_DIR/chat.log" 2>&1 &
  echo $! > "$PID_DIR/chat.pid"

  # Reverse Proxy
  log "Proxy → :$PROXY_PORT"
  cd "$DEPLOY"
  PROXY_PORT="$PROXY_PORT" DOMAIN="$DOMAIN" \
  nohup bun run proxy.ts > "$LOG_DIR/proxy.log" 2>&1 &
  echo $! > "$PID_DIR/proxy.pid"

  sleep 3
  cmd_status
}

cmd_stop() {
  log "Stopping all Nexus services..."
  for svc in cloud chat proxy; do
    if [ -f "$PID_DIR/$svc.pid" ]; then
      kill "$(cat "$PID_DIR/$svc.pid")" 2>/dev/null && log "  Stopped $svc" || true
      rm -f "$PID_DIR/$svc.pid"
    fi
  done
  log "All stopped."
}

cmd_status() {
  echo ""
  log "═══════════════════════════════════════════"
  log "  Nexus Systems — nexussystems.vexr.dev"
  log "═══════════════════════════════════════════"
  echo ""

  check() {
    local name=$1 port=$2
    if curl -s -o /dev/null -w "%{http_code}" "http://localhost:$port" 2>/dev/null | grep -q "200\|204\|301\|302"; then
      echo -e "  ${G}✓${R} $name — http://localhost:$port"
    else
      echo -e "  ${Y}✗${R} $name — not responding"
    fi
  }

  check "Nexus-Cloud     " 8787
  check "Nexus-Team-Chat " 3109
  check "Proxy           " "$PROXY_PORT"

  echo ""
  echo "  Domain: http://$DOMAIN:$PROXY_PORT"
  echo "  Cloud:  http://cloud.$DOMAIN:$PROXY_PORT"
  echo "  Chat:   http://chat.$DOMAIN:$PROXY_PORT"
  echo ""
}

# ── Main ──

case "${1:-}" in
  --bg)     cmd_start ;;
  --stop)   cmd_stop ;;
  --status) cmd_status ;;
  *)
    # Foreground mode
    trap 'cmd_stop' EXIT INT TERM
    cmd_start

    # Tail logs
    log "All services running. Press Ctrl+C to stop."
    log "Logs: $LOG_DIR/"
    echo ""
    tail -f "$LOG_DIR/proxy.log" "$LOG_DIR/cloud.log" "$LOG_DIR/chat.log" 2>/dev/null || true
    ;;
esac
