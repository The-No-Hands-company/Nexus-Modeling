#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SoftwareRasterizer extension commands
//
//  This extension must be compiled by targets that link BOTH nexus_gfx_core
//  AND nexus_softrast (the rasterizer depends on the core, so the core cannot
//  depend back on it — this free function lives in nexus_automation_ext which
//  links both).
//
//  Usage:
//    ScriptBatchHarness harness;                    // built-in commands
//    nexus::automation::registerSoftrastCommands(harness); // add softrast.*
//
//  Commands registered:
//    softrast.render   — render context.mesh to a PPM file
//    softrast.describe — report current mesh + render config
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register softrast.* commands into harness.registry().
/// Safe to call multiple times — duplicate registrations are a no-op.
void registerSoftrastCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
