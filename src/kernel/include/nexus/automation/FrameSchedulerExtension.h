#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — IFrameScheduler headless extension commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

void registerFrameSchedulerCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
