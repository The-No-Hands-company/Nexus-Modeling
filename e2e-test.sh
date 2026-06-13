#!/usr/bin/env bash
set -euo pipefail

CLOUD_LOG=/tmp/nexus-cloud.log
rm -f $CLOUD_LOG
ROOT="$(cd "$(dirname "$0")" && pwd)"

cleanup() {
  jobs -p | xargs -r kill 2>/dev/null
  wait 2>/dev/null
}
trap cleanup EXIT

# ── Start cloud ─────────────────────────────────────────────────────

echo "━━━ Nexus-Cloud ━━━"
cd "$ROOT/apps/Nexus-Cloud"
NEXUS_CLOUD_API_KEY=change-me NEXUS_CLOUD_URL=http://localhost:8787 CORS_ORIGIN=* PORT=8787 \
  bun run src/index.ts > $CLOUD_LOG 2>&1 &
CLOUD_PID=$!

for i in $(seq 1 20); do
  nc -z localhost 8787 2>/dev/null && break
  [ $i -eq 20 ] && { echo "FAIL: cloud did not start"; kill $CLOUD_PID 2>/dev/null; exit 1; }
  sleep 1
done

curl -s http://localhost:8787/health | grep -q '"ok":true' && echo "✓ Cloud healthy" || echo "✗ Cloud health failed"

# ── Start apps ──────────────────────────────────────────────────────

declare -A PORTS=( [Nexus-Graphic]=3080 [Nexus-Design]=3074 [Nexus-Draw]=3075 [Nexus-Photos]=3096 [Nexus-Video]=3113 )
COUNT=0

for app in Nexus-Graphic Nexus-Design Nexus-Draw Nexus-Photos Nexus-Video; do
  port=${PORTS[$app]}
  env_key="NEXUS_${app//-/_}_ENABLE_CLOUD_INTEGRATION"

  export PORT=$port
  export NEXUS_CLOUD_URL=http://localhost:8787
  export NEXUS_CLOUD_API_KEY=change-me
  export "$env_key=true"

  cd "$ROOT/apps/$app"
  bun run src/index.ts > "/tmp/${app}.log" 2>&1 &
  
  for i in $(seq 1 10); do
    nc -z localhost $port 2>/dev/null && break
    sleep 1
  done
  curl -s "http://localhost:$port/health" | grep -q '"status":"ok"' && echo "✓ $app:$port healthy" || echo "✗ $app:$port FAILED"
  COUNT=$((COUNT+1))
done

# ── Topology ────────────────────────────────────────────────────────

sleep 2
echo ""
echo "━━━ Topology ━━━"
curl -s http://localhost:8787/api/v1/tools | python3 -c "
import json,sys
data=json.load(sys.stdin)
tools=data.get('tools',[])
print(f'{len(tools)} tools registered:')
for t in sorted(tools,key=lambda x:x['id']):
    print(f'  {t[\"id\"]:25s} {t[\"health\"]:10s} {t.get(\"upstreamUrl\",\"-\"):40s}')
" 2>/dev/null || true

echo ""
echo "✓ Ecosystem running ($COUNT apps + cloud)"
