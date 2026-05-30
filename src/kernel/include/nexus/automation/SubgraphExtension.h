#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Subgraph extension commands
//  (compiled into nexus_automation_ext)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>

namespace nexus::automation {

/// Register subgraph.template_new, subgraph.add_node, subgraph.connect,
/// subgraph.declare_input, subgraph.declare_output, subgraph.validate,
/// subgraph.instantiate, subgraph.describe,
/// subgraph.registry_add, subgraph.registry_list, subgraph.registry_remove
/// commands.
void registerSubgraphCommands(ScriptBatchHarness& harness);

} // namespace nexus::automation
