#pragma once
#include <nexus/gfx/Allocator.h>
#include <cstdlib>

namespace nexus::gfx {

class NullAllocator final : public IGPUAllocator {
public:
    void* alloc(const AllocationInfo& info) override {
        return std::malloc(info.sizeBytes);
    }
    void free(void* allocation) override {
        std::free(allocation);
    }
    void commitTile(TextureHandle, uint32_t, uint32_t, uint32_t) override {}
    void evictTile(TextureHandle, uint32_t, uint32_t, uint32_t) override {}
    uint64_t budgetBytes() const noexcept override { return 1ULL << 30; }
    uint64_t allocatedBytes() const noexcept override { return 0; }
    uint64_t usedBytes() const noexcept override { return 0; }
};

} // namespace nexus::gfx
