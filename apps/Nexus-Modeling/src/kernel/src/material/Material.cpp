#include "nexus/material/Material.h"

#include <algorithm>
#include <cstdint>

namespace nexus::material {

bool PBRMaterial::hasTexture(TextureType t) const {
    return std::any_of(textures.begin(), textures.end(),
                        [t](const TextureRef& ref) { return ref.type == t; });
}

uint32_t MaterialLibrary::addMaterial(const PBRMaterial& mat) {
    const uint32_t index = static_cast<uint32_t>(m_materials.size());
    m_materials.push_back(mat);
    return index;
}

const PBRMaterial& MaterialLibrary::material(uint32_t index) const {
    return m_materials[index];
}

PBRMaterial& MaterialLibrary::material(uint32_t index) {
    return m_materials[index];
}

size_t MaterialLibrary::materialCount() const {
    return m_materials.size();
}

void MaterialLibrary::addSlot(const MaterialSlot& slot) {
    m_slots.push_back(slot);
}

const std::vector<MaterialSlot>& MaterialLibrary::slots() const {
    return m_slots;
}

} // namespace nexus::material
