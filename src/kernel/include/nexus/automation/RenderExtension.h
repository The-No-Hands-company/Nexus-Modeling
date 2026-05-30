#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Render pipeline extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register render.graph.record, render.graph.validate, render.graph.clear,
/// render.graph.describe,
/// render.taa.configure, render.taa.advance, render.taa.describe
/// commands.
void registerRenderCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
