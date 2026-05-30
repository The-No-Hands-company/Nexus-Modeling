#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="${ROOT_DIR}/scripts/scenarios/manifest.tsv"
BACKEND_OVERRIDE="${1:-}"

if [[ ! -f "${MANIFEST}" ]]; then
    echo "Missing manifest: ${MANIFEST}" >&2
    exit 2
fi

mapfile -t scenario_ids < <(awk -F $'\t' 'NF >= 5 && $1 !~ /^#/ { print $1 }' "${MANIFEST}")
if [[ ${#scenario_ids[@]} -eq 0 ]]; then
    echo "No scenarios found in manifest." >&2
    exit 2
fi

pass1_file="$(mktemp)"
pass2_file="$(mktemp)"
trap 'rm -f "${pass1_file}" "${pass2_file}"' EXIT

echo "[scenario-pack] pass 1"
for id in "${scenario_ids[@]}"; do
    if [[ -n "${BACKEND_OVERRIDE}" ]]; then
        "${ROOT_DIR}/tools/run_scenario.sh" "${id}" "${BACKEND_OVERRIDE}"
    else
        "${ROOT_DIR}/tools/run_scenario.sh" "${id}"
    fi
    sig="$(cat "${ROOT_DIR}/build/scenario_artifacts/${id}/deterministic_signature.txt")"
    printf "%s\t%s\n" "${id}" "${sig}" >>"${pass1_file}"
done

echo "[scenario-pack] pass 2"
for id in "${scenario_ids[@]}"; do
    if [[ -n "${BACKEND_OVERRIDE}" ]]; then
        "${ROOT_DIR}/tools/run_scenario.sh" "${id}" "${BACKEND_OVERRIDE}"
    else
        "${ROOT_DIR}/tools/run_scenario.sh" "${id}"
    fi
    sig="$(cat "${ROOT_DIR}/build/scenario_artifacts/${id}/deterministic_signature.txt")"
    printf "%s\t%s\n" "${id}" "${sig}" >>"${pass2_file}"
done

drift=0
echo "[scenario-pack] deterministic replay comparison"
for id in "${scenario_ids[@]}"; do
    sig1="$(awk -F $'\t' -v sid="${id}" '$1 == sid { print $2; exit }' "${pass1_file}")"
    sig2="$(awk -F $'\t' -v sid="${id}" '$1 == sid { print $2; exit }' "${pass2_file}")"
    if [[ "${sig1}" != "${sig2}" ]]; then
        echo "DRIFT: ${id}"
        echo "  pass1=${sig1}"
        echo "  pass2=${sig2}"
        drift=1
    else
        echo "OK: ${id} ${sig1}"
    fi
done

if [[ ${drift} -ne 0 ]]; then
    echo "Deterministic signature drift detected." >&2
    exit 4
fi

echo "Scenario pack complete with stable deterministic signatures."
