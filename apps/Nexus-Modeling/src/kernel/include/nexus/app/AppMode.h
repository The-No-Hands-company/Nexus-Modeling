#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Application — Mode System Architecture
//
//  Registry-based plugin architecture with chain-of-responsibility input
//  routing, per-mode action maps for dynamic UI generation, and centralized
//  undo stack with cross-mode command support.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadSelection.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

// ──────────── Input Events ──────────────────────────────────────────────────

enum class InputEventType : uint8_t {
    MouseDown, MouseUp, MouseMove, MouseDrag, MouseWheel,
    KeyDown, KeyUp, KeyRepeat,
    PenPressure, PenTilt,
    TouchBegin, TouchMove, TouchEnd,
    Enter, Exit, Focus, Blur,
};

struct InputEvent {
    InputEventType type;
    Vec3  position{}, delta{};
    int   button = 0, key = 0, modifiers = 0;
    float pressure = 1.f, wheelDelta = 0.f;
};

// ──────────── Event Result (Chain of Responsibility) ────────────────────────

enum class EventResult : uint8_t {
    Unconsumed,  // event not handled — propagate to next handler
    Consumed,    // event handled — stop propagation
};

// ──────────── Action Descriptor (for Dynamic UI) ────────────────────────────

struct ActionDescriptor {
    std::string id;              // "sketch.line"
    std::string label;           // "Draw Line"
    std::string shortcut;        // "L"
    std::string iconIdentifier;  // "icon_line_2d"
    std::string category;        // "Draw", "Modify", "Dimension"
    bool requiresSelection = false; // UI hint: gray-out when nothing selected
};

// ──────────── Application Context ───────────────────────────────────────────

struct AppContext {
    nexus::cad::CadDocument*   document = nullptr;
    nexus::cad::CadSelection*  selection = nullptr;
    nexus::render::SceneGraph* scene = nullptr;
    class ModeOrchestrator*    orchestrator = nullptr; // for mode switching from UI
    Vec3  cameraPosition{0, 0, 10}, cameraTarget{}, cameraUp{0, 1, 0};
    Vec3  cursorWorldPos{};  // world-space intersection with grid plane (y=0)
    enum class WorkPlane { XZ, XY, YZ };
    WorkPlane workPlane = WorkPlane::XZ;
    bool  snapToGrid = true;
    float viewportWidth = 1920, viewportHeight = 1080;
    std::optional<nexus::geometry::Mesh> previewMesh; // ghost preview during operations

    // Sub-object selection (face/edge/vertex editing).
    uint32_t selectedFace   = ~0u;
    uint32_t selectedEdge   = ~0u;
    uint32_t selectedVertex = ~0u;
    Vec3     hitPoint{};  // world-space intersection point

    // Dimension overlay (set by DimensionMode, rendered by main).
    bool  dimActive = false;
    Vec3  dimP1{}, dimP2{};
    bool  mouseOverViewport = false, viewportFocused = false;

    // Per-mode private data (type-erased sandboxing).
    template<typename T>
    T* getPrivateData(const std::string& modeId) {
        auto it = modePrivateData.find(modeId);
        if(it != modePrivateData.end()) return static_cast<T*>(it->second.ptr.get());
        return nullptr;
    }
    template<typename T>
    void setPrivateData(const std::string& modeId, std::shared_ptr<T> data) {
        modePrivateData[modeId] = ModeData{data};
    }
    void clearPrivateData(const std::string& modeId) { modePrivateData.erase(modeId); }

private:
    struct ModeData { std::shared_ptr<void> ptr; };
    std::unordered_map<std::string, ModeData> modePrivateData;
};

// ──────────── Abstract Mode Interface ──────────────────────────────────────

class AppMode {
public:
    virtual ~AppMode() = default;

    [[nodiscard]] virtual std::string modeId() const = 0;   // unique: "select", "sketch", "fillet"
    [[nodiscard]] virtual std::string displayName() const = 0; // human-readable: "Sketch Mode"
    [[nodiscard]] virtual std::string icon() const { return ""; }

    // ── Lifecycle ──────────────────────────────────────────────────
    virtual void onActivate(AppContext&)   {}
    virtual bool onDeactivate(AppContext&) { return true; } // false = reject, stay in mode
    virtual void onSuspend(AppContext&)    {}
    virtual void onResume(AppContext&)     {}

    // ── Input (Chain of Responsibility) ────────────────────────────
    virtual EventResult handleInput(AppContext&, const InputEvent&) { return EventResult::Unconsumed; }
    virtual bool wantsExclusiveInput() const noexcept { return false; }

    // ── Rendering ──────────────────────────────────────────────────
    virtual void renderOverlay(AppContext&) {}
    virtual void renderToolVisuals(AppContext&) {}

    // ── Action Map (for dynamic UI generation) ─────────────────────
    [[nodiscard]] virtual std::vector<ActionDescriptor> actions() const { return {}; }
    virtual bool executeAction(const std::string& actionId, AppContext&) { (void)actionId; return false; }

    // ── Status ─────────────────────────────────────────────────────
    [[nodiscard]] virtual std::string statusText() const { return ""; }
    [[nodiscard]] virtual bool canExit() const noexcept { return true; }
};

// ──────────── Viewport Mode (Drawing/Shading) ──────────────────────────────

enum class ViewportMode : uint8_t {
    Wireframe, Solid, Shaded, MaterialPreview, Rendered, XRay, HiddenLine
};

class ViewportController {
public:
    void setMode(ViewportMode m) { m_mode = m; }
    [[nodiscard]] ViewportMode mode() const noexcept { return m_mode; }
    [[nodiscard]] static std::string modeName(ViewportMode m) noexcept;
    void applyToScene(nexus::render::SceneGraph&) noexcept;
private:
    ViewportMode m_mode = ViewportMode::Solid;
};

// ──────────── Mode Registry (Singleton Plugin System) ──────────────────────

class ModeRegistry {
public:
    [[nodiscard]] static ModeRegistry& instance() noexcept;

    using Factory = std::function<std::unique_ptr<AppMode>()>;
    void registerMode(const std::string& modeId, Factory factory);
    [[nodiscard]] std::unique_ptr<AppMode> create(const std::string& modeId) const;
    [[nodiscard]] std::vector<std::string> registeredModeIds() const noexcept;
    [[nodiscard]] bool isRegistered(const std::string& modeId) const noexcept;

private:
    ModeRegistry() = default;
    std::unordered_map<std::string, Factory> m_factories;
};

// ──────────── Mode Orchestrator ────────────────────────────────────────────

class ModeOrchestrator {
public:
    explicit ModeOrchestrator(AppContext& ctx);

    void registerBuiltinModes();

    bool switchTo(const std::string& modeId);
    [[nodiscard]] std::string activeModeId() const noexcept;
    [[nodiscard]] AppMode* activeMode() noexcept { return m_active; }
    [[nodiscard]] bool executeAction(const std::string& actionId);

    void pushOverlay(std::unique_ptr<AppMode> overlay);
    void popOverlay();

    // Chain-of-responsibility input routing.
    EventResult routeInput(const InputEvent& event);

    void renderOverlays();
    void renderToolVisuals();

    [[nodiscard]] ViewportController& viewport() noexcept { return m_viewport; }
    [[nodiscard]] const std::vector<std::unique_ptr<AppMode>>& modes() const noexcept { return m_modes; }

private:
    AppContext& m_ctx;
    AppMode*    m_active = nullptr;
    std::vector<std::unique_ptr<AppMode>> m_modes;
    std::vector<std::unique_ptr<AppMode>> m_overlayStack;
    ViewportController m_viewport;
};

} // namespace nexus::app
