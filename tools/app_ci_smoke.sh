#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Nexus Modeling tooling-app CI smoke (PR-19, Week 4 Day 4)
#
#  Headless-safe sequence that exercises the app's no-crash launch path,
#  scenario surface, starter modeling flow, editor seams demo, read-only
#  session panel, and persistence/recovery basics. Intended to participate
#  in CI without manual steps.
#
#  Usage:
#    tools/app_ci_smoke.sh [path/to/nexus_tooling_app] [state-dir]
#
#  Defaults:
#    app:        ./build/src/tooling/nexus_tooling_app
#    state-dir:  $(mktemp -d)/nexus_modeling_state
# ─────────────────────────────────────────────────────────────────────────────
set -u
set -o pipefail

APP_PATH="${1:-./build/src/tooling/nexus_tooling_app}"
STATE_DIR="${2:-$(mktemp -d)/nexus_modeling_state}"

if [[ ! -x "$APP_PATH" ]]; then
    echo "[app-ci-smoke] FAIL: tooling app not executable at $APP_PATH" >&2
    exit 2
fi

mkdir -p "$STATE_DIR"

fail_count=0
step_count=0

run_step() {
    local label="$1"; shift
    local expect_exit="$1"; shift
    local expect_marker="$1"; shift
    step_count=$((step_count + 1))
    local out
    local rc
    out="$("$APP_PATH" "$@" 2>&1)"
    rc=$?
    if [[ "$rc" -ne "$expect_exit" ]]; then
        echo "[app-ci-smoke] FAIL [$label]: exit=$rc expected=$expect_exit" >&2
        echo "$out" | head -20 >&2
        fail_count=$((fail_count + 1))
        return
    fi
    if [[ -n "$expect_marker" ]] && ! grep -qF "$expect_marker" <<<"$out"; then
        echo "[app-ci-smoke] FAIL [$label]: marker not found: $expect_marker" >&2
        echo "$out" | head -20 >&2
        fail_count=$((fail_count + 1))
        return
    fi
    echo "[app-ci-smoke] ok   [$label]"
}

# 1. No-crash launch (--help). Smoke target #1.
run_step "help"               0 "Usage"                              --help

# 2. Scenario surface lists at least the header (scenario-open smoke).
run_step "list-scenarios"     0 "Available scenarios"                --list-scenarios

# 3. Starter workspace surface (read-only orientation pane).
run_step "starter-workspace"  0 ""                                   --starter-workspace

# 4. ModelingShell starter-flow smoke. CG-1 (kernel-contract-gaps.md) is
#    closed in PR-22; the starter cleanup path must now exit 0 and write
#    both the mesh and diagnostics artifacts.
TMP_DIR="$(mktemp -d)"
MESH_OUT="$TMP_DIR/box.obj"
DIAG_OUT="$TMP_DIR/diag.txt"
out="$("$APP_PATH" --starter-model Box --cleanup --session-summary \
                   --export-mesh "$MESH_OUT" --export-diagnostics "$DIAG_OUT" 2>&1)"
rc=$?
step_count=$((step_count + 1))
if [[ $rc -ne 0 ]]; then
    echo "[app-ci-smoke] FAIL [starter-model]: exit=$rc (expected 0)" >&2
    echo "$out" | tail -20 >&2
    fail_count=$((fail_count + 1))
elif ! grep -qF "Starter model session" <<<"$out"; then
    echo "[app-ci-smoke] FAIL [starter-model]: missing header" >&2
    echo "$out" | head -20 >&2
    fail_count=$((fail_count + 1))
elif [[ ! -s "$MESH_OUT" ]] || [[ ! -s "$DIAG_OUT" ]]; then
    echo "[app-ci-smoke] FAIL [starter-model]: artifacts not written" >&2
    fail_count=$((fail_count + 1))
else
    echo "[app-ci-smoke] ok   [starter-model]"
fi

# 5. Editor seams demo (Week 4 Day 1 surface).
run_step "editor-seams-demo"  0 "[after_branch_new_command]"         --editor-seams-demo

# 6. Read-only session panel (Week 4 Day 2 surface) over the artifacts just
#    written by the starter-model step.
run_step "session-panel"      0 "[diagnostics summary]"              \
    --session-panel --panel-mesh "$MESH_OUT" --panel-diagnostics "$DIAG_OUT"

# 7. Persistence: save + restore round trip (Week 4 Day 3 surface).
run_step "save-session-state" 0 "Session state saved"                \
    --save-session-state --session-kind starter-model \
    --session-label ci-smoke --panel-mesh "$MESH_OUT" \
    --panel-diagnostics "$DIAG_OUT" --state-dir "$STATE_DIR"

run_step "restore-session"    0 "session_kind: starter-model"        \
    --restore-session-state --state-dir "$STATE_DIR"

run_step "append-state-log"   0 "State log appended"                 \
    --append-state-log "ci-smoke-pass" --state-dir "$STATE_DIR"

run_step "show-state-log"     0 "ci-smoke-pass"                      \
    --show-state-log --state-dir "$STATE_DIR"

# Cleanup transient outputs (keep STATE_DIR for inspection if explicitly passed).
rm -rf "$TMP_DIR"
if [[ -z "${2:-}" ]]; then
    rm -rf "$STATE_DIR"
fi

echo "[app-ci-smoke] steps=$step_count failures=$fail_count"
if [[ "$fail_count" -ne 0 ]]; then
    exit 1
fi
exit 0
