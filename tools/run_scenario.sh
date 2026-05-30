#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <scenario_id> [backend]" >&2
    exit 2
fi

SCENARIO_ID="$1"
BACKEND_OVERRIDE="${2:-}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="${ROOT_DIR}/scripts/scenarios/manifest.tsv"

if [[ ! -f "${MANIFEST}" ]]; then
    echo "Missing manifest: ${MANIFEST}" >&2
    exit 2
fi

row="$(awk -F $'\t' -v id="${SCENARIO_ID}" 'NF >= 5 && $1 !~ /^#/ && $1 == id { print; exit }' "${MANIFEST}")"
if [[ -z "${row}" ]]; then
    echo "Unknown scenario id: ${SCENARIO_ID}" >&2
    exit 2
fi

IFS=$'\t' read -r _id _title _backend _script _description <<<"${row}"
backend="${_backend}"
if [[ -n "${BACKEND_OVERRIDE}" ]]; then
    backend="${BACKEND_OVERRIDE}"
fi

script_path="${ROOT_DIR}/${_script}"
if [[ ! -x "${script_path}" ]]; then
    echo "Scenario script not executable: ${script_path}" >&2
    exit 2
fi

artifact_dir="${ROOT_DIR}/build/scenario_artifacts/${SCENARIO_ID}"
mkdir -p "${artifact_dir}"

"${script_path}" "${artifact_dir}" "${backend}"

for required in summary.json diagnostics.txt deterministic_signature.txt; do
    if [[ ! -f "${artifact_dir}/${required}" ]]; then
        echo "Missing required artifact: ${artifact_dir}/${required}" >&2
        exit 3
    fi
done

echo "Scenario ${SCENARIO_ID} complete"
echo "Artifacts: ${artifact_dir}"
