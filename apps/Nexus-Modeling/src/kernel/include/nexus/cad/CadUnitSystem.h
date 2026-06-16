#pragma once
#include <cstdint>
#include <string>

namespace nexus::cad {
enum class CadUnit : uint8_t { Millimeter, Centimeter, Meter, Inch, Foot, Thou };
struct CadUnitSystem { CadUnit length=CadUnit::Millimeter; double scale=1.0; [[nodiscard]] static CadUnitSystem metric() noexcept; [[nodiscard]] static CadUnitSystem imperial() noexcept; [[nodiscard]] double toBase(double v) const noexcept; [[nodiscard]] double fromBase(double v) const noexcept; [[nodiscard]] std::string suffix() const noexcept; };
}
