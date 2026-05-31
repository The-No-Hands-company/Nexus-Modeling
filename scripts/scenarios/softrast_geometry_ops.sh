#!/usr/bin/env bash
set -euo pipefail

ARTIFACT_DIR="${1:?artifact dir required}"
BACKEND="${2:-null}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST_BIN="${ROOT_DIR}/build/tests/nexus_kernel_tests"
SCENARIO_ID="softrast_geometry_ops"
FILTER="SoftrastScenario.GeometryOpsPipeline"

mkdir -p "${ARTIFACT_DIR}"
log_tmp="$(mktemp)"
trap 'rm -f "${log_tmp}"' EXIT

status="pass"
if [[ ! -x "${TEST_BIN}" ]]; then
    status="fail"
    echo "Missing test binary: ${TEST_BIN}" >"${log_tmp}"
else
    if ! SOFTRAST_GEOOPS_ARTIFACT_DIR="${ARTIFACT_DIR}" \
         "${TEST_BIN}" --gtest_filter="${FILTER}" >"${log_tmp}" 2>&1; then
        status="fail"
    fi
fi

cp "${log_tmp}" "${ARTIFACT_DIR}/diagnostics.txt"

if [[ ! -f "${ARTIFACT_DIR}/frame_hash.txt" ]]; then
    printf "no_frame_available\n" >"${ARTIFACT_DIR}/frame_hash.txt"
fi

cat >"${ARTIFACT_DIR}/summary.json" <<EOF
{
  "scenario_id": "${SCENARIO_ID}",
  "backend": "${BACKEND}",
  "status": "${status}",
  "test_filter": "${FILTER}",
  "frame_hash": "$(cat "${ARTIFACT_DIR}/frame_hash.txt" | tr -d '\n')",
  "pipeline": "inset+extrude+render",
  "shading": "gouraud",
  "artifact_version": 1
}
EOF

cat "${ARTIFACT_DIR}/summary.json" "${ARTIFACT_DIR}/diagnostics.txt" | sha256sum | awk '{print $1}' >"${ARTIFACT_DIR}/deterministic_signature.txt"
