#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="${ROOT_DIR}/scripts/scenarios/manifest.tsv"

usage() {
    cat <<EOF
Usage:
  $0 list
  $0 run <scenario_id> [backend]
    $0 run-pack [backend]
  $0 show <scenario_id>
  $0 dashboard
    $0 validate
    $0 monthly-report [YYYY-MM]
EOF
}

require_manifest() {
    if [[ ! -f "${MANIFEST}" ]]; then
        echo "Missing manifest: ${MANIFEST}" >&2
        exit 2
    fi
}

list_scenarios() {
    require_manifest
    echo "Available scenarios:"
    awk -F $'\t' 'NF >= 5 && $1 !~ /^#/ { printf "  - %s (%s): %s\n", $1, $3, $5 }' "${MANIFEST}"
}

show_scenario() {
    local scenario_id="$1"
    local artifact_dir="${ROOT_DIR}/build/scenario_artifacts/${scenario_id}"
    local summary="${artifact_dir}/summary.json"
    local diagnostics="${artifact_dir}/diagnostics.txt"

    if [[ ! -f "${summary}" ]]; then
        echo "No summary found for scenario ${scenario_id}. Run it first." >&2
        exit 2
    fi

    echo "== Summary =="
    cat "${summary}"
    echo
    if [[ -f "${artifact_dir}/deterministic_signature.txt" ]]; then
        echo "== Deterministic Signature =="
        cat "${artifact_dir}/deterministic_signature.txt"
        echo
    fi

    if [[ -f "${artifact_dir}/frame.png" ]]; then
        echo "Frame artifact: ${artifact_dir}/frame.png"
    elif [[ -f "${artifact_dir}/frame_hash.txt" ]]; then
        echo "Frame hash:"
        cat "${artifact_dir}/frame_hash.txt"
        echo
    fi

    if [[ -f "${diagnostics}" ]]; then
        echo "== Diagnostics (last 40 lines) =="
        tail -n 40 "${diagnostics}"
    fi
}

show_dashboard() {
    local dashboard="${ROOT_DIR}/build/capability_dashboard.json"
    "${ROOT_DIR}/tools/capability_dashboard.sh" --output "${dashboard}" >/dev/null
    echo "== Capability Dashboard =="
    cat "${dashboard}"
}

cmd="${1:-}"
case "${cmd}" in
    list)
        list_scenarios
        ;;
    run)
        if [[ $# -lt 2 ]]; then
            usage
            exit 2
        fi
        "${ROOT_DIR}/tools/run_scenario.sh" "${2}" "${3:-}"
        show_scenario "${2}"
        ;;
    run-pack)
        "${ROOT_DIR}/tools/run_scenario_pack.sh" "${2:-}"
        ;;
    show)
        if [[ $# -lt 2 ]]; then
            usage
            exit 2
        fi
        show_scenario "${2}"
        ;;
    dashboard)
        show_dashboard
        ;;
    validate)
        "${ROOT_DIR}/tools/validate_dashboard_artifacts.sh"
        ;;
    monthly-report)
        if [[ $# -ge 2 ]]; then
            "${ROOT_DIR}/tools/monthly_demo_report.sh" --month "${2}"
        else
            "${ROOT_DIR}/tools/monthly_demo_report.sh"
        fi
        ;;
    *)
        usage
        exit 2
        ;;
esac
