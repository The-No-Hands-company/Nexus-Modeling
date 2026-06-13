#pragma once

namespace nexus::automation {

class ScriptRegistry;

void registerMetaCommands(ScriptRegistry& registry);
void registerMeshCommands(ScriptRegistry& registry);
void registerSceneCommands(ScriptRegistry& registry);
void registerRenderCommands(ScriptRegistry& registry);
void registerAnimationCommands(ScriptRegistry& registry);
void registerSimRigidCommands(ScriptRegistry& registry);
void registerSimClothCommands(ScriptRegistry& registry);
void registerSimFluidCommands(ScriptRegistry& registry);
void registerGaussianCommands(ScriptRegistry& registry);
void registerCrossSolverCommands(ScriptRegistry& registry);
void registerParametricCommands(ScriptRegistry& registry);

} // namespace nexus::automation
