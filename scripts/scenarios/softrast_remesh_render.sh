#!/usr/bin/env bash
# softrast_remesh_render scenario
# Remeshes a coarse UV sphere isotropically then renders the original and
# remeshed versions with a checkerboard texture, emitting PPM artifacts,
# remesh stats, and a combined frame hash.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-${REPO_ROOT}/artifacts/softrast_remesh_render}"
BINARY="${REPO_ROOT}/build/tests/nexus_kernel_tests"

if [[ ! -x "${BINARY}" ]]; then
    echo "[softrast_remesh_render] ERROR: test binary not found at ${BINARY}" >&2
    exit 1
fi

mkdir -p "${ARTIFACT_DIR}"

echo "[softrast_remesh_render] remesh + render pipeline → ${ARTIFACT_DIR}"

SOFTRAST_REMESH_ARTIFACT_DIR="${ARTIFACT_DIR}" \
    "${BINARY}" \
    --gtest_filter="SoftrastScenario.RemeshAndRender" \
    --gtest_output="xml:${ARTIFACT_DIR}/results.xml"

if [[ -f "${ARTIFACT_DIR}/remesh_stats.txt" ]]; then
    echo "[softrast_remesh_render] stats:"
    cat "${ARTIFACT_DIR}/remesh_stats.txt"
fi

if [[ -f "${ARTIFACT_DIR}/frame_hash.txt" ]]; then
    echo "[softrast_remesh_render] frame_hash=$(cat "${ARTIFACT_DIR}/frame_hash.txt")"
fi

echo "[softrast_remesh_render] done."
