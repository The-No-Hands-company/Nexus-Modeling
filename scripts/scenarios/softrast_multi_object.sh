#!/usr/bin/env bash
# softrast_multi_object scenario
# Renders a three-mesh scene (sphere + box + sphere) via softrast.render_scene,
# producing Gouraud-lit PPM artifacts from three viewpoints with a combined frame hash.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-${REPO_ROOT}/artifacts/softrast_multi_object}"
BINARY="${REPO_ROOT}/build/tests/nexus_kernel_tests"

if [[ ! -x "${BINARY}" ]]; then
    echo "[softrast_multi_object] ERROR: test binary not found at ${BINARY}" >&2
    exit 1
fi

mkdir -p "${ARTIFACT_DIR}"

echo "[softrast_multi_object] rendering multi-object scene → ${ARTIFACT_DIR}"

SOFTRAST_MULTI_ARTIFACT_DIR="${ARTIFACT_DIR}" \
    "${BINARY}" \
    --gtest_filter="SoftrastScenario.RenderMultiObjectScene" \
    --gtest_output="xml:${ARTIFACT_DIR}/results.xml"

if [[ -f "${ARTIFACT_DIR}/frame_hash.txt" ]]; then
    echo "[softrast_multi_object] frame_hash=$(cat "${ARTIFACT_DIR}/frame_hash.txt")"
fi

echo "[softrast_multi_object] done."
