#!/usr/bin/env bash
# softrast_geometry_ops scenario
# Chains mesh.inset → mesh.extrude → softrast.render via the automation
# command system, demonstrating the full hard-surface authoring pipeline.
# Emits PPM renders of the extruded mesh, pipeline stats, and a frame hash.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-${REPO_ROOT}/artifacts/softrast_geometry_ops}"
BINARY="${REPO_ROOT}/build/tests/nexus_kernel_tests"

if [[ ! -x "${BINARY}" ]]; then
    echo "[softrast_geometry_ops] ERROR: test binary not found at ${BINARY}" >&2
    exit 1
fi

mkdir -p "${ARTIFACT_DIR}"

echo "[softrast_geometry_ops] inset → extrude → render pipeline → ${ARTIFACT_DIR}"

SOFTRAST_GEOOPS_ARTIFACT_DIR="${ARTIFACT_DIR}" \
    "${BINARY}" \
    --gtest_filter="SoftrastScenario.GeometryOpsPipeline" \
    --gtest_output="xml:${ARTIFACT_DIR}/results.xml"

if [[ -f "${ARTIFACT_DIR}/pipeline_stats.txt" ]]; then
    echo "[softrast_geometry_ops] pipeline stats:"
    cat "${ARTIFACT_DIR}/pipeline_stats.txt"
fi

if [[ -f "${ARTIFACT_DIR}/frame_hash.txt" ]]; then
    echo "[softrast_geometry_ops] frame_hash=$(cat "${ARTIFACT_DIR}/frame_hash.txt")"
fi

echo "[softrast_geometry_ops] done."
