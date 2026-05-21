# Editor Bring-Up Paper Cuts — Deferred Backlog

Captured during Week 1 and Week 2 of the Month-1 editor bring-up (PR-1 through PR-10).
These are intentionally **deferred** — none of them block the Month-1 milestones. They
are filed here so that later milestones can address them without re-discovery.

Scope guard: items in this document must not be silently fixed inside an unrelated PR.
Each entry should graduate to a tracked task before code changes land.

## Tooling app CLI

- **`--show-dashboard` exits 0 on malformed JSON.** The loader degrades to
  `subsystem summary: unavailable` rather than a hard failure. Useful for resilient
  scripting today, but a future `--strict` flag (or CI-mode default) should surface
  a non-zero exit so CI can detect dashboard regressions earlier.
- **`loadKnownGaps` treats unparseable input as "no gaps".** Same trade-off as above:
  resilient now, ambiguous later. Consider surfacing a `parse error` summary line
  when the file exists but yields zero brace-balanced objects.
- **Manifest parse error is fatal even when later rows are well-formed.** A single
  malformed row aborts `--list-scenarios` / `--shell`. Acceptable while the manifest
  is hand-edited; revisit when scenarios scale past ~50 entries.
- **`--refresh-dashboard` overwrites `build/capability_dashboard.json` in place by
  default.** Intentional for dev workflow, but should be called out explicitly in
  `--help` once we have more than one writer.
- **`--skip-gates` discoverability.** The flag is only documented under `--help`
  for `--refresh-dashboard`; first-time users tend to wait for the full gate run.

## Shell / viewer layout

- **Pane numbering is `[1][2][3][4][4b][5]`.** The `[4b] Known Gaps` pane was
  inserted between `[4]` and `[5]` in PR-9 without renumbering to avoid breaking
  earlier smoke tests. Renumber to `[1..6]` once a TUI replaces the text shell.
- **`[5] Run Controls` is static text.** No interactive prompt yet — selection is
  driven entirely by `--scenario`. Interactive selection is deferred to the TUI
  milestone (post Month-1).
- **No progress indicator during scenario runs.** `--run-scenario` streams raw
  stdout from the runner; fine for headless tests, but the eventual interactive
  shell will want a spinner or per-step heartbeat.
- **Frame / artifact viewer is filename + size only.** No image preview, no diff.
  Tracked under the asset-pipeline checklist; do not address inside the editor
  bring-up window.

## Scenario runner integration

- **Exit code semantics conflate runner failure and scenario failure.** Today
  `--run-scenario` returns 1 both when the runner script exits non-zero *and* when
  the refreshed `summary.json` reports `status: fail`. Adequate for CI gates; a
  future PR should distinguish (e.g. `2` for infrastructure failure, `1` for
  scenario failure).
- **`--run-scenario` requires the scenario id to be a "safe" shell token.** The
  `isSafeShellToken` allowlist (`[A-Za-z0-9_.-]`) is intentional defense in depth.
  Document this constraint in the scenario authoring guide before opening the
  manifest to external contributors.

## Parsing

- **`extractSubsystemStatuses` is line-based.** It requires `"name": ...` and
  `"status": ...` to appear on separate trimmed lines. Compact JSON or
  minified dashboards will not parse. Replace with a real JSON reader before
  swapping in any non-`tools/capability_dashboard.sh` producer.
- **Known-gaps parser uses brace-balanced object slicing.** It is *not* a full
  JSON parser. String escapes inside object values are tolerated by the field
  extractor, but unusual whitespace or trailing commas may misbehave. Same
  remediation path as above.

## Tests

- **Subprocess tests rely on `popen` + `WEXITSTATUS`.** Works on Linux/macOS;
  Windows port will need an abstraction layer.
- **`makeTempDir` uses `getpid() + rand()` rather than `mkdtemp`.** Sufficient
  for serial ctest runs; revisit if we ever parallelize the tooling smoke suite
  (currently serial by default).
