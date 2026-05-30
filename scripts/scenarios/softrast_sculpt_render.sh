#!/usr/bin/env bash
# softrast_sculpt_render scenario
# Inflates a UV sphere via sculpt.stroke (inflate + smooth), describes the
# sculpted mesh, then renders three Gouraud-lit viewpoints.
# Emits PPM artifacts, sculpt stats, and a frame hash.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-${REPO_ROOT}/artifacts/softrast_sculpt_render}"
BINARY="${REPO_ROOT}/build/tests/nexus_kernel_tests"

if [[ ! -x "${BINARY}" ]]; then
    echo "[softrast_sculpt_render] ERROR: test binary not found at ${BINARY}" >&2
    exit 1
fi

mkdir -p "${ARTIFACT_DIR}"

echo "[softrast_sculpt_render] inflate → smooth → render → ${ARTIFACT_DIR}"

NEXUS_SCENARIO_SCULPT_RENDER=1 \
SOFTRAST_ARTIFACT_DIR="${ARTIFACT_DIR}" \
    "${BINARY}" \
    --gtest_filter="SoftrastScenario.SculptAndRender" \
    --gtest_output="xml:${ARTIFACT_DIR}/results.xml"

if [[ -f "${ARTIFACT_DIR}/sculpt_render/sculpt_stats.txt" ]]; then
    echo "[softrast_sculpt_render] sculpt stats:"
    cat "${ARTIFACT_DIR}/sculpt_render/sculpt_stats.txt"
fi

if [[ -f "${ARTIFACT_DIR}/sculpt_render/frame_hash.txt" ]]; then
    echo "[softrast_sculpt_render] frame_hash=$(cat "${ARTIFACT_DIR}/sculpt_render/frame_hash.txt")"
fi

echo "[softrast_sculpt_render] done."
