#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_ROOT="${ROOT_DIR}/build/scenario_artifacts"
MONTH="$(date -u +"%Y-%m")"

if [[ "${1:-}" == "--month" ]]; then
    MONTH="${2:?missing month value}"
    shift 2
fi

if [[ ! -d "${ARTIFACT_ROOT}" ]]; then
    echo "No scenario artifacts found at ${ARTIFACT_ROOT}. Run scenarios first." >&2
    exit 2
fi

PACK_DIR="${ROOT_DIR}/build/monthly_demo/${MONTH}"
mkdir -p "${PACK_DIR}"

dashboard_path="${ROOT_DIR}/build/capability_dashboard.json"
known_gaps_path="${ROOT_DIR}/build/known_gaps.json"
"${ROOT_DIR}/tools/capability_dashboard.sh" --output "${dashboard_path}" --known-gaps-output "${known_gaps_path}"
"${ROOT_DIR}/tools/validate_dashboard_artifacts.sh" --dashboard "${dashboard_path}" --known-gaps "${known_gaps_path}"

scenario_ids_file="${PACK_DIR}/scenario_ids.txt"
signatures_file="${PACK_DIR}/deterministic_signatures.txt"
summary_file="${PACK_DIR}/demo_summary.md"

find "${ARTIFACT_ROOT}" -mindepth 1 -maxdepth 1 -type d -printf "%f\n" | sort >"${scenario_ids_file}"
>"${signatures_file}"

passed=0
failed=0
total=0

while IFS= read -r sid; do
    [[ -z "${sid}" ]] && continue
    total=$((total + 1))
    summary_json="${ARTIFACT_ROOT}/${sid}/summary.json"
    sig_file="${ARTIFACT_ROOT}/${sid}/deterministic_signature.txt"

    if [[ -f "${summary_json}" ]] && grep -q '"status"[[:space:]]*:[[:space:]]*"pass"' "${summary_json}"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi

    if [[ -f "${sig_file}" ]]; then
        printf "%s %s\n" "${sid}" "$(cat "${sig_file}")" >>"${signatures_file}"
    else
        printf "%s missing_signature\n" "${sid}" >>"${signatures_file}"
    fi
done <"${scenario_ids_file}"

cp "${dashboard_path}" "${PACK_DIR}/capability_dashboard.json"
cp "${known_gaps_path}" "${PACK_DIR}/known_gaps_snapshot.json"

cat >"${summary_file}" <<EOF
# Monthly Vertical Demo Pack ${MONTH}

Generated at: $(date -u +"%Y-%m-%dT%H:%M:%SZ")

## Scenario Summary

1. Total scenarios: ${total}
2. Passed scenarios: ${passed}
3. Failed scenarios: ${failed}

## Included Files

1. scenario_ids.txt
2. deterministic_signatures.txt
3. capability_dashboard.json
4. known_gaps_snapshot.json

## Notes

1. This report is generated from existing scenario artifacts.
2. No editor-level UI interaction is required.
EOF

echo "== Monthly Demo Pack Report (${MONTH}) =="
echo "Pack directory: ${PACK_DIR}"
echo "Total scenarios: ${total}"
echo "Passed: ${passed}"
echo "Failed: ${failed}"
echo "Scenario list: ${scenario_ids_file}"
echo "Deterministic signatures: ${signatures_file}"
echo "Dashboard snapshot: ${PACK_DIR}/capability_dashboard.json"
echo "Known gaps snapshot: ${PACK_DIR}/known_gaps_snapshot.json"
