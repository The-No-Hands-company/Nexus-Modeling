#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_PATH="${BUILD_DIR}/capability_dashboard.json"
KNOWN_GAPS_PATH="${BUILD_DIR}/known_gaps.json"
ARTIFACT_ROOT="${NEXUS_SCENARIO_ARTIFACT_ROOT:-${ROOT_DIR}/build/scenario_artifacts}"
MANIFEST_PATH="${NEXUS_SCENARIO_MANIFEST:-${ROOT_DIR}/scripts/scenarios/manifest.tsv}"
CANONICAL_SCENARIO_TARGET=8
RUN_GATES=1

usage() {
  cat <<EOF
Usage:
  $0 [--output <path>] [--known-gaps-output <path>] [--skip-gates]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --output)
      OUTPUT_PATH="${2:?missing value for --output}"
      shift 2
      ;;
    --known-gaps-output)
      KNOWN_GAPS_PATH="${2:?missing value for --known-gaps-output}"
      shift 2
      ;;
    --skip-gates)
      RUN_GATES=0
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 2
      ;;
  esac
done

mkdir -p "$(dirname "${OUTPUT_PATH}")"
mkdir -p "$(dirname "${KNOWN_GAPS_PATH}")"

timestamp="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
commit="$(git -C "${ROOT_DIR}" rev-parse --short HEAD 2>/dev/null || echo "unknown")"

scenario_total=0
scenario_present=0
scenario_passed=0
scenario_failed=0
scenario_missing=0
scenario_unexpected=0
canonical_scenarios=0
declare -A scenario_state
declare -A artifact_state
declare -A canonical_set
declare -a canonical_ids

build_status="skipped"
build_exit_code=-1
build_tail="Gate skipped"

test_status="skipped"
test_exit_code=-1
tests_total=0
tests_failed=0
tests_passed=0
test_summary_line="Gate skipped"

build_log="$(mktemp)"
test_log="$(mktemp)"
trap 'rm -f "${build_log}" "${test_log}"' EXIT

if [[ ${RUN_GATES} -eq 1 ]]; then
  set +e
  cmake --build "${BUILD_DIR}" -j"$(nproc)" >"${build_log}" 2>&1
  build_exit_code=$?
  set -e
  if [[ ${build_exit_code} -eq 0 ]]; then
    build_status="pass"
  else
    build_status="fail"
  fi
  build_tail="$(tail -n 6 "${build_log}" | sed 's/"/\\"/g' | tr '\n' ';' || true)"

  set +e
  ctest --test-dir "${BUILD_DIR}" --output-on-failure >"${test_log}" 2>&1
  test_exit_code=$?
  set -e
  if [[ ${test_exit_code} -eq 0 ]]; then
    test_status="pass"
  else
    test_status="fail"
  fi

  summary_line_raw="$(grep -E "tests passed, [0-9]+ tests failed out of" "${test_log}" | tail -n 1 || true)"
  if [[ -n "${summary_line_raw}" ]]; then
    test_summary_line="${summary_line_raw}"
    tests_failed="$(echo "${summary_line_raw}" | sed -E 's/.*passed, ([0-9]+) tests failed out of ([0-9]+).*/\1/')"
    tests_total="$(echo "${summary_line_raw}" | sed -E 's/.*passed, ([0-9]+) tests failed out of ([0-9]+).*/\2/')"
    tests_passed=$((tests_total - tests_failed))
  else
    test_summary_line="Could not parse ctest summary"
  fi
fi

if [[ -f "${MANIFEST_PATH}" ]]; then
  mapfile -t canonical_ids < <(awk -F $'\t' 'NF >= 5 && $1 !~ /^#/ { print $1 }' "${MANIFEST_PATH}")
  canonical_scenarios="${#canonical_ids[@]}"
  for scenario_id in "${canonical_ids[@]}"; do
    canonical_set["${scenario_id}"]=1
  done
fi

if [[ -d "${ARTIFACT_ROOT}" ]]; then
    while IFS= read -r -d '' summary; do
        scenario_id="$(basename "$(dirname "${summary}")")"
        if grep -q '"status"[[:space:]]*:[[:space:]]*"pass"' "${summary}"; then
            artifact_state["${scenario_id}"]="pass"
        else
            artifact_state["${scenario_id}"]="fail"
        fi
    done < <(find "${ARTIFACT_ROOT}" -mindepth 2 -maxdepth 2 -name summary.json -print0 | sort -z)
fi

if [[ ${canonical_scenarios} -gt 0 ]]; then
  scenario_total=${canonical_scenarios}
  for scenario_id in "${canonical_ids[@]}"; do
    state="${artifact_state[${scenario_id}]:-missing}"
    scenario_state["${scenario_id}"]="${state}"
    if [[ "${state}" == "pass" ]]; then
      scenario_present=$((scenario_present + 1))
      scenario_passed=$((scenario_passed + 1))
    elif [[ "${state}" == "fail" ]]; then
      scenario_present=$((scenario_present + 1))
      scenario_failed=$((scenario_failed + 1))
    else
      scenario_missing=$((scenario_missing + 1))
    fi
  done

  for scenario_id in "${!artifact_state[@]}"; do
    if [[ -z "${canonical_set[${scenario_id}]+x}" ]]; then
      scenario_unexpected=$((scenario_unexpected + 1))
    fi
  done
else
  for scenario_id in "${!artifact_state[@]}"; do
    scenario_state["${scenario_id}"]="${artifact_state[${scenario_id}]}"
    scenario_total=$((scenario_total + 1))
    scenario_present=$((scenario_present + 1))
    if [[ "${artifact_state[${scenario_id}]}" == "pass" ]]; then
      scenario_passed=$((scenario_passed + 1))
    else
      scenario_failed=$((scenario_failed + 1))
    fi
  done
fi

scenario_signature="none"
scenario_sig_payload=""
if [[ ${canonical_scenarios} -gt 0 ]]; then
  for scenario_id in "${canonical_ids[@]}"; do
    sig="none"
    if [[ -f "${ARTIFACT_ROOT}/${scenario_id}/deterministic_signature.txt" ]]; then
      sig="$(tr -d '\n\r' <"${ARTIFACT_ROOT}/${scenario_id}/deterministic_signature.txt")"
      if [[ -z "${sig}" ]]; then
        sig="none"
      fi
    fi
    scenario_sig_payload+="${scenario_id}:${scenario_state[${scenario_id}]:-missing}:${sig}"$'\n'
  done
else
  while IFS= read -r scenario_id; do
    sig="none"
    if [[ -f "${ARTIFACT_ROOT}/${scenario_id}/deterministic_signature.txt" ]]; then
      sig="$(tr -d '\n\r' <"${ARTIFACT_ROOT}/${scenario_id}/deterministic_signature.txt")"
      if [[ -z "${sig}" ]]; then
        sig="none"
      fi
    fi
    scenario_sig_payload+="${scenario_id}:${scenario_state[${scenario_id}]:-missing}:${sig}"$'\n'
  done < <(printf '%s\n' "${!scenario_state[@]}" | sort)
fi

if [[ -n "${scenario_sig_payload}" ]]; then
  scenario_signature="$(printf '%s' "${scenario_sig_payload}" | sha256sum | awk '{print $1}')"
fi

deterministic_status="fail"
if [[ ${scenario_total} -gt 0 && ${scenario_failed} -eq 0 && ${scenario_missing} -eq 0 ]]; then
  deterministic_status="pass"
fi

declare -a gaps
gaps=()
if [[ ${build_status} != "pass" ]]; then
  gaps+=("P0-GATE-BUILD")
fi
if [[ ${test_status} != "pass" ]]; then
  gaps+=("P0-GATE-TEST")
fi
if [[ ${scenario_total} -eq 0 ]]; then
  gaps+=("P0-SCENARIO-NONE")
fi
if [[ ${scenario_failed} -gt 0 ]]; then
  gaps+=("P0-SCENARIO-FAIL")
fi
if [[ ${scenario_missing} -gt 0 ]]; then
  gaps+=("P0-SCENARIO-MISSING")
fi
if [[ ${scenario_unexpected} -gt 0 ]]; then
  gaps+=("P0-SCENARIO-UNEXPECTED")
fi
if [[ ${canonical_scenarios} -lt ${CANONICAL_SCENARIO_TARGET} ]]; then
  gaps+=("P0-SCENARIO-COVERAGE")
fi

if [[ ${#gaps[@]} -eq 0 ]]; then
  gaps+=("P0-NONE")
fi

gaps_json_entries=""
for gap in "${gaps[@]}"; do
  if [[ -n "${gaps_json_entries}" ]]; then
    gaps_json_entries+=$'\n,'
  fi
  case "${gap}" in
    P0-GATE-BUILD)
      gaps_json_entries+="  {\"id\": \"P0-GATE-BUILD\", \"domain\": \"Build\", \"severity\": \"S1\", \"status\": \"open\", \"user_impact\": \"Build gate failed or was not executed.\"}"
      ;;
    P0-GATE-TEST)
      gaps_json_entries+="  {\"id\": \"P0-GATE-TEST\", \"domain\": \"Testing\", \"severity\": \"S1\", \"status\": \"open\", \"user_impact\": \"Full ctest gate failed or was not executed.\"}"
      ;;
    P0-SCENARIO-NONE)
      gaps_json_entries+="  {\"id\": \"P0-SCENARIO-NONE\", \"domain\": \"Scenarios\", \"severity\": \"S2\", \"status\": \"open\", \"user_impact\": \"No scenario artifacts found.\"}"
      ;;
    P0-SCENARIO-FAIL)
      gaps_json_entries+="  {\"id\": \"P0-SCENARIO-FAIL\", \"domain\": \"Scenarios\", \"severity\": \"S1\", \"status\": \"open\", \"user_impact\": \"One or more scenarios failed.\"}"
      ;;
    P0-SCENARIO-MISSING)
      gaps_json_entries+="  {\"id\": \"P0-SCENARIO-MISSING\", \"domain\": \"Scenarios\", \"severity\": \"S2\", \"status\": \"open\", \"user_impact\": \"One or more canonical scenario artifacts are missing.\"}"
      ;;
    P0-SCENARIO-UNEXPECTED)
      gaps_json_entries+="  {\"id\": \"P0-SCENARIO-UNEXPECTED\", \"domain\": \"Scenarios\", \"severity\": \"S3\", \"status\": \"open\", \"user_impact\": \"Unexpected non-canonical scenario artifacts are present.\"}"
      ;;
    P0-SCENARIO-COVERAGE)
      gaps_json_entries+="  {\"id\": \"P0-SCENARIO-COVERAGE\", \"domain\": \"Scenarios\", \"severity\": \"S2\", \"status\": \"open\", \"user_impact\": \"Canonical scenario pack has fewer than 8 entries.\"}"
      ;;
    P0-NONE)
      gaps_json_entries+="  {\"id\": \"P0-NONE\", \"domain\": \"Dashboard\", \"severity\": \"S3\", \"status\": \"closed\", \"user_impact\": \"No current machine-detected gaps.\"}"
      ;;
  esac
done

cat >"${KNOWN_GAPS_PATH}" <<EOF
[
${gaps_json_entries}
]
EOF

known_gap_refs=""
for gap in "${gaps[@]}"; do
  if [[ -n "${known_gap_refs}" ]]; then
    known_gap_refs+=", "
  fi
  known_gap_refs+="\"${gap}\""
done

known_gap_refs_from_list() {
  local refs=""
  local gap=""
  for gap in "$@"; do
    if [[ -n "${refs}" ]]; then
      refs+=", "
    fi
    refs+="\"${gap}\""
  done
  echo "${refs}"
}

SUBSYSTEM_STATUS="green"
SUBSYSTEM_CONFIDENCE="high"
SUBSYSTEM_KNOWN_GAPS=""
SUBSYSTEM_REQUIRED=0
SUBSYSTEM_FAILED=0
SUBSYSTEM_MISSING=0
SUBSYSTEM_SIGNATURE="none"
GROUP_REQUIRED=0
GROUP_FAILED=0
GROUP_MISSING=0

compute_signature_for_scenarios() {
  local -a scenarios=("$@")
  if [[ ${#scenarios[@]} -eq 0 ]]; then
    echo "none"
    return
  fi

  local scenario_id=""
  local state=""
  local sig=""
  local payload=""
  for scenario_id in "${scenarios[@]}"; do
    state="${scenario_state[${scenario_id}]:-missing}"
    sig="none"
    if [[ -f "${ARTIFACT_ROOT}/${scenario_id}/deterministic_signature.txt" ]]; then
      sig="$(tr -d '\n' <"${ARTIFACT_ROOT}/${scenario_id}/deterministic_signature.txt")"
      if [[ -z "${sig}" ]]; then
        sig="none"
      fi
    fi
    payload+="${scenario_id}:${state}:${sig}"$'\n'
  done

  printf '%s' "${payload}" | sha256sum | awk '{print $1}'
}

group_status_from_counts() {
  local failed="$1"
  local missing="$2"
  if [[ ${failed} -gt 0 ]]; then
    echo "fail"
  elif [[ ${missing} -gt 0 ]]; then
    echo "missing"
  else
    echo "pass"
  fi
}

compute_scenario_group_counts() {
  local -a scenarios=("$@")
  local required=0
  local failed=0
  local missing=0
  local scenario_id=""
  local state=""

  for scenario_id in "${scenarios[@]}"; do
    required=$((required + 1))
    state="${scenario_state[${scenario_id}]:-missing}"
    if [[ "${state}" == "fail" ]]; then
      failed=$((failed + 1))
    elif [[ "${state}" == "missing" ]]; then
      missing=$((missing + 1))
    fi
  done

  GROUP_REQUIRED=${required}
  GROUP_FAILED=${failed}
  GROUP_MISSING=${missing}
}

compute_subsystem_health() {
  local -a required_scenarios=("$@")
  local status="green"
  local confidence="high"
  local required=0
  local failed=0
  local missing=0
  local -a local_gaps=()

  if [[ ${build_status} == "fail" ]]; then
    status="red"
    confidence="low"
    local_gaps+=("P0-GATE-BUILD")
  elif [[ ${build_status} != "pass" ]]; then
    status="yellow"
    confidence="medium"
    local_gaps+=("P0-GATE-BUILD")
  fi

  if [[ ${test_status} == "fail" ]]; then
    status="red"
    confidence="low"
    local_gaps+=("P0-GATE-TEST")
  elif [[ ${test_status} != "pass" ]]; then
    if [[ ${status} != "red" ]]; then
      status="yellow"
      confidence="medium"
    fi
    local_gaps+=("P0-GATE-TEST")
  fi

  if [[ ${canonical_scenarios} -lt ${CANONICAL_SCENARIO_TARGET} ]]; then
    if [[ ${status} != "red" ]]; then
      status="yellow"
      confidence="medium"
    fi
    local_gaps+=("P0-SCENARIO-COVERAGE")
  fi

  local scenario_id=""
  for scenario_id in "${required_scenarios[@]}"; do
    required=$((required + 1))
    state="${scenario_state[${scenario_id}]:-missing}"
    if [[ "${state}" == "fail" ]]; then
      failed=$((failed + 1))
      status="red"
      confidence="low"
      local_gaps+=("P0-SCENARIO-FAIL")
    elif [[ "${state}" == "missing" ]]; then
      missing=$((missing + 1))
      if [[ ${status} != "red" ]]; then
        status="yellow"
        confidence="medium"
      fi
      local_gaps+=("P0-SCENARIO-MISSING")
    fi
  done

  local -a deduped_gaps=()
  local -A seen_gaps=()
  local gap=""
  for gap in "${local_gaps[@]}"; do
    if [[ -n "${seen_gaps[${gap}]+x}" ]]; then
      continue
    fi
    seen_gaps["${gap}"]=1
    deduped_gaps+=("${gap}")
  done

  SUBSYSTEM_STATUS="${status}"
  SUBSYSTEM_CONFIDENCE="${confidence}"
  SUBSYSTEM_KNOWN_GAPS="$(known_gap_refs_from_list "${deduped_gaps[@]}")"
  SUBSYSTEM_REQUIRED=${required}
  SUBSYSTEM_FAILED=${failed}
  SUBSYSTEM_MISSING=${missing}
  SUBSYSTEM_SIGNATURE="$(compute_signature_for_scenarios "${required_scenarios[@]}")"
}

emit_subsystem() {
  local name="$1"
  shift
  compute_subsystem_health "$@"
  cat <<EOF
    {
      "name": "${name}",
      "status": "${SUBSYSTEM_STATUS}",
      "confidence": "${SUBSYSTEM_CONFIDENCE}",
      "required_gates": {
        "build": "${build_status}",
        "build_exit_code": ${build_exit_code},
        "tests": "${test_status}",
        "tests_exit_code": ${test_exit_code},
        "scenario_checks": {
          "required": ${SUBSYSTEM_REQUIRED},
          "failed": ${SUBSYSTEM_FAILED},
          "missing": ${SUBSYSTEM_MISSING}
        }
      },
      "deterministic_signature": "${SUBSYSTEM_SIGNATURE}",
      "perf_summary": { "median_ms": "pending", "baseline_ms": "pending", "delta_percent": "pending" },
      "known_gaps": [${SUBSYSTEM_KNOWN_GAPS}]
    }
EOF
}

emit_asset_automation_subsystem() {
  compute_subsystem_health asset_importer_dependency_mode_scene asset_automation_pipeline_scene

  compute_scenario_group_counts asset_importer_dependency_mode_scene
  local asset_graph_required=${GROUP_REQUIRED}
  local asset_graph_failed=${GROUP_FAILED}
  local asset_graph_missing=${GROUP_MISSING}
  local asset_graph_status
  asset_graph_status="$(group_status_from_counts "${asset_graph_failed}" "${asset_graph_missing}")"
  local asset_graph_signature
  asset_graph_signature="$(compute_signature_for_scenarios asset_importer_dependency_mode_scene)"

  compute_scenario_group_counts asset_automation_pipeline_scene
  local automation_required=${GROUP_REQUIRED}
  local automation_failed=${GROUP_FAILED}
  local automation_missing=${GROUP_MISSING}
  local automation_status
  automation_status="$(group_status_from_counts "${automation_failed}" "${automation_missing}")"
  local automation_signature
  automation_signature="$(compute_signature_for_scenarios asset_automation_pipeline_scene)"

  cat <<EOF
    {
      "name": "Asset and Automation",
      "status": "${SUBSYSTEM_STATUS}",
      "confidence": "${SUBSYSTEM_CONFIDENCE}",
      "required_gates": {
        "build": "${build_status}",
        "build_exit_code": ${build_exit_code},
        "tests": "${test_status}",
        "tests_exit_code": ${test_exit_code},
        "scenario_checks": {
          "required": ${SUBSYSTEM_REQUIRED},
          "failed": ${SUBSYSTEM_FAILED},
          "missing": ${SUBSYSTEM_MISSING},
          "asset_graph": {
            "required": ${asset_graph_required},
            "failed": ${asset_graph_failed},
            "missing": ${asset_graph_missing},
            "status": "${asset_graph_status}",
            "deterministic_signature": "${asset_graph_signature}",
            "scenario_ids": ["asset_importer_dependency_mode_scene"]
          },
          "automation_pipeline": {
            "required": ${automation_required},
            "failed": ${automation_failed},
            "missing": ${automation_missing},
            "status": "${automation_status}",
            "deterministic_signature": "${automation_signature}",
            "scenario_ids": ["asset_automation_pipeline_scene"]
          }
        }
      },
      "deterministic_signature": "${SUBSYSTEM_SIGNATURE}",
      "perf_summary": { "median_ms": "pending", "baseline_ms": "pending", "delta_percent": "pending" },
      "known_gaps": [${SUBSYSTEM_KNOWN_GAPS}]
    }
EOF
}

subsystems_json="$(emit_subsystem "Graphics Backend" render_scheduler_baseline)"
subsystems_json+=$'\n,'
subsystems_json+="$(emit_subsystem "Render Pipeline" render_scheduler_baseline shadow_chain_contract_scene composite_binding_contract_scene)"
subsystems_json+=$'\n,'
subsystems_json+="$(emit_subsystem "Scene and Eval Graph" nodescene_reconstruction_diagnostics eval_graph_deterministic_evaluation)"
subsystems_json+=$'\n,'
subsystems_json+="$(emit_subsystem "Geometry Core" parametric_rectangle_replay)"
subsystems_json+=$'\n,'
subsystems_json+="$(emit_subsystem "Parametric Foundation" parametric_rectangle_replay)"
subsystems_json+=$'\n,'
subsystems_json+="$(emit_subsystem "Simulation Core" simulation_deterministic_trajectory)"
subsystems_json+=$'\n,'
subsystems_json+="$(emit_subsystem "Gaussian and Neural Integration" gaussian_mesh_coexistence_scene)"
subsystems_json+=$'\n,'
subsystems_json+="$(emit_asset_automation_subsystem)"

cat >"${OUTPUT_PATH}" <<EOF
{
  "generated_at_utc": "${timestamp}",
  "git_commit": "${commit}",
  "environment": {
    "os": "linux",
    "backend_default": "null"
  },
  "subsystems": [
${subsystems_json}
  ],
  "deterministic_checks": {
    "scenario_signature": "${scenario_signature}",
    "status": "${deterministic_status}"
  },
  "perf_smoke": {
    "status": "pending",
    "note": "Phase 0 generator stub does not execute perf harness."
  },
  "known_gaps": $(cat "${KNOWN_GAPS_PATH}"),
  "monthly_demo_status": {
    "status": "pending",
    "last_demo_month": "none"
  },
  "gate_results": {
    "build": {
      "status": "${build_status}",
      "exit_code": ${build_exit_code}
    },
    "ctest": {
      "status": "${test_status}",
      "exit_code": ${test_exit_code},
      "tests_total": ${tests_total},
      "tests_passed": ${tests_passed},
      "tests_failed": ${tests_failed}
    }
  },
  "scenarios": {
    "canonical_target": ${CANONICAL_SCENARIO_TARGET},
    "canonical_total": ${canonical_scenarios},
    "present": ${scenario_present},
    "total": ${scenario_total},
    "passed": ${scenario_passed},
    "failed": ${scenario_failed},
    "missing": ${scenario_missing},
    "unexpected": ${scenario_unexpected}
  }
}
EOF

echo "Wrote dashboard: ${OUTPUT_PATH}"
echo "Wrote known gaps: ${KNOWN_GAPS_PATH}"
