#include <nexus/geometry/MeshBoolean.h>
#include <nexus/geometry/BooleanOperation.h>

#include <cstdint>
#include <string>

namespace nexus::geometry {

static BooleanOperationType toBooleanOperationType(BooleanOp op) noexcept {
    switch (op) {
        case BooleanOp::Union:        return BooleanOperationType::Union;
        case BooleanOp::Difference:    return BooleanOperationType::Difference;
        case BooleanOp::Intersection:  return BooleanOperationType::Intersection;
    }
    return BooleanOperationType::Union;
}

MeshBooleanResult MeshBoolean::compute(const Mesh& a, const Mesh& b, BooleanOp op) {
    BooleanOperationOptions opts;
    Mesh result;
    auto report = BooleanOperation::compute(a, b, toBooleanOperationType(op), opts, result);
    if (report.valid) return {true, "", std::move(result)};
    return {false, "Boolean operation failed", {}};
}

} // namespace nexus::geometry
