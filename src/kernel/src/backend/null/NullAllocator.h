#pragma once
// Null/headless IGPUAllocator implementation — no Vulkan dependency.
#include <nexus/gfx/Allocator.h>

namespace nexus::gfx {

class NullAllocator final : public IGPUAllocator {
public:
    void* alloc  (const AllocationInfo&) override { return nullptr; }
    void  free   (void*)                 override {}
    void  commitTile(TextureHandle, uint32_t, uint32_t, uint32_t) override {}
    void  evictTile (TextureHandle, uint32_t, uint32_t, uint32_t) override {}
    [[nodiscard]] uint64_t budgetBytes()    const noexcept override { return 0; }
    [[nodiscard]] uint64_t allocatedBytes() const noexcept override { return 0; }
    [[nodiscard]] uint64_t usedBytes()      const noexcept override { return 0; }
};

} // namespace nexus::gfx
