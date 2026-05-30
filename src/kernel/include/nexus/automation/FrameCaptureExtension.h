#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Frame capture and Gaussian splat pass extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register capture.begin, capture.record_pass, capture.end,
/// capture.describe, capture.snapshot, capture.diff,
/// splat_pass.configure, splat_pass.attach, splat_pass.clear,
/// splat_pass.compute_stats, splat_pass.describe commands.
void registerFrameCaptureCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
