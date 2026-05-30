#!/usr/bin/env bash
# softrast_parametric_render scenario
# Builds a unit-square parametric sketch, solves it, then renders a derived
# box mesh at two Gouraud-lit viewpoints.
# Emits PPM artifacts, solve log, sketch stats, and a frame hash.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-${REPO_ROOT}/artifacts/softrast_parametric_render}"
BINARY="${REPO_ROOT}/build/tests/nexus_kernel_tests"

if [[ ! -x "${BINARY}" ]]; then
    echo "[softrast_parametric_render] ERROR: test binary not found at ${BINARY}" >&2
    exit 1
fi

mkdir -p "${ARTIFACT_DIR}"

echo "[softrast_parametric_render] sketch → solve → render → ${ARTIFACT_DIR}"

NEXUS_SCENARIO_PARAMETRIC_RENDER=1 \
SOFTRAST_ARTIFACT_DIR="${ARTIFACT_DIR}" \
    "${BINARY}" \
    --gtest_filter="SoftrastScenario.ParametricSketchRender" \
    --gtest_output="xml:${ARTIFACT_DIR}/results.xml"

if [[ -f "${ARTIFACT_DIR}/parametric_render/sketch_stats.txt" ]]; then
    echo "[softrast_parametric_render] sketch stats:"
    cat "${ARTIFACT_DIR}/parametric_render/sketch_stats.txt"
fi

if [[ -f "${ARTIFACT_DIR}/parametric_render/solve_log.txt" ]]; then
    echo "[softrast_parametric_render] solve log:"
    cat "${ARTIFACT_DIR}/parametric_render/solve_log.txt"
fi

if [[ -f "${ARTIFACT_DIR}/parametric_render/frame_hash.txt" ]]; then
    echo "[softrast_parametric_render] frame_hash=$(cat "${ARTIFACT_DIR}/parametric_render/frame_hash.txt")"
fi

echo "[softrast_parametric_render] done."
