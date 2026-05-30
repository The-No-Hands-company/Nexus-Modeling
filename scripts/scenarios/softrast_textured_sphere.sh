#!/usr/bin/env bash
# softrast_textured_sphere scenario
# Renders a UV sphere with a procedural checkerboard texture and a UV-gradient
# texture from two viewpoints, emitting PPM artifacts and a combined frame hash.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-${REPO_ROOT}/artifacts/softrast_textured_sphere}"
BINARY="${REPO_ROOT}/build/tests/nexus_kernel_tests"

if [[ ! -x "${BINARY}" ]]; then
    echo "[softrast_textured_sphere] ERROR: test binary not found at ${BINARY}" >&2
    exit 1
fi

mkdir -p "${ARTIFACT_DIR}"

echo "[softrast_textured_sphere] rendering textured sphere → ${ARTIFACT_DIR}"

SOFTRAST_TEXTURED_ARTIFACT_DIR="${ARTIFACT_DIR}" \
    "${BINARY}" \
    --gtest_filter="SoftrastScenario.RenderTexturedSphere" \
    --gtest_output="xml:${ARTIFACT_DIR}/results.xml"

if [[ -f "${ARTIFACT_DIR}/frame_hash.txt" ]]; then
    echo "[softrast_textured_sphere] frame_hash=$(cat "${ARTIFACT_DIR}/frame_hash.txt")"
fi

echo "[softrast_textured_sphere] done."
