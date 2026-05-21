#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DASHBOARD_PATH="${ROOT_DIR}/build/capability_dashboard.json"
KNOWN_GAPS_PATH="${ROOT_DIR}/build/known_gaps.json"
SCHEMA_DIR="${ROOT_DIR}/tools/schema"

usage() {
    cat <<EOF
Usage:
  $0 [--dashboard <path>] [--known-gaps <path>] [--schema-dir <path>]
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dashboard)
            DASHBOARD_PATH="${2:?missing value for --dashboard}"
            shift 2
            ;;
        --known-gaps)
            KNOWN_GAPS_PATH="${2:?missing value for --known-gaps}"
            shift 2
            ;;
        --schema-dir)
            SCHEMA_DIR="${2:?missing value for --schema-dir}"
            shift 2
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

python3 - "${DASHBOARD_PATH}" "${KNOWN_GAPS_PATH}" "${SCHEMA_DIR}" <<'PY'
import json
import pathlib
import sys

dashboard_path = pathlib.Path(sys.argv[1])
known_gaps_path = pathlib.Path(sys.argv[2])
schema_dir = pathlib.Path(sys.argv[3])

dashboard_schema_path = schema_dir / "capability_dashboard.schema.json"
known_gaps_schema_path = schema_dir / "known_gaps.schema.json"

errors = []


def type_ok(instance, expected):
    mapping = {
        "object": dict,
        "array": list,
        "string": str,
        "integer": int,
        "number": (int, float),
        "boolean": bool,
    }
    py_type = mapping.get(expected)
    if py_type is None:
        return True
    if expected == "integer":
        return isinstance(instance, int) and not isinstance(instance, bool)
    if expected == "number":
        return (isinstance(instance, (int, float)) and not isinstance(instance, bool))
    return isinstance(instance, py_type)


def validate(instance, schema, path):
    expected = schema.get("type")
    if expected and not type_ok(instance, expected):
        errors.append(f"{path}: expected type {expected}, got {type(instance).__name__}")
        return

    if "enum" in schema and instance not in schema["enum"]:
        errors.append(f"{path}: value {instance!r} not in enum {schema['enum']}")

    if expected == "object":
        required = schema.get("required", [])
        for key in required:
            if key not in instance:
                errors.append(f"{path}: missing required key {key!r}")
        props = schema.get("properties", {})
        for key, sub_schema in props.items():
            if key in instance:
                validate(instance[key], sub_schema, f"{path}.{key}")

    if expected == "array":
        items_schema = schema.get("items")
        if items_schema is not None:
            for idx, item in enumerate(instance):
                validate(item, items_schema, f"{path}[{idx}]")

    minimum = schema.get("minimum")
    if minimum is not None and isinstance(instance, (int, float)):
        if instance < minimum:
            errors.append(f"{path}: value {instance} is below minimum {minimum}")


for p in [dashboard_path, known_gaps_path, dashboard_schema_path, known_gaps_schema_path]:
    if not p.is_file():
        errors.append(f"missing file: {p}")

if errors:
    for e in errors:
        print(f"ERROR: {e}")
    sys.exit(1)

with dashboard_schema_path.open("r", encoding="utf-8") as f:
    dashboard_schema = json.load(f)
with known_gaps_schema_path.open("r", encoding="utf-8") as f:
    known_gaps_schema = json.load(f)
with dashboard_path.open("r", encoding="utf-8") as f:
    dashboard = json.load(f)
with known_gaps_path.open("r", encoding="utf-8") as f:
    known_gaps = json.load(f)

validate(dashboard, dashboard_schema, "dashboard")
validate(known_gaps, known_gaps_schema, "known_gaps")

if not isinstance(dashboard.get("known_gaps"), list):
    errors.append("dashboard.known_gaps: expected array")
if dashboard.get("known_gaps") != known_gaps:
    errors.append("dashboard.known_gaps does not match known_gaps.json contents")

if errors:
    for e in errors:
        print(f"ERROR: {e}")
    sys.exit(1)

print("Validation OK: capability_dashboard.json and known_gaps.json")
PY
