// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Stroke history serialization extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/StrokeHistoryExtension.h>
#include <nexus/sculpt/StrokeHistorySerialization.h>
#include <nexus/sculpt/SculptSession.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
shGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = shGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static nexus::sculpt::BrushKind parseBrushKind(std::string_view s)
{
    if (s == "Smooth")  return nexus::sculpt::BrushKind::Smooth;
    if (s == "Inflate") return nexus::sculpt::BrushKind::Inflate;
    if (s == "Flatten") return nexus::sculpt::BrushKind::Flatten;
    if (s == "Pinch")   return nexus::sculpt::BrushKind::Pinch;
    return nexus::sculpt::BrushKind::Draw;
}

struct StrokeHistState {
    std::vector<nexus::sculpt::StrokeDelta> history;
    std::string                             serialized;
    bool                                    hasSerialized = false;
    nexus::sculpt::StrokeId                 nextId        = 1u;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerStrokeHistoryCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<StrokeHistState>();

    // ── stroke_hist.add ───────────────────────────────────────────────────────
    //
    // Appends a StrokeDelta to the in-memory history.
    //
    // Arguments:
    //   kind=Draw|Smooth|Inflate|Flatten|Pinch   (default Draw)
    //   vi=<N>   vertex index for one delta  (optional; adds one vertex delta)
    //   dx=<f> dy=<f> dz=<f>                 displacement (default 0,0,0)
    //
    // On success: "stroke_hist.add ok id=<I> kind=<K>"
    harness.registry().registerCommand("stroke_hist.add",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto kindStr = shGetArg(cmd, "kind");
            const nexus::sculpt::BrushKind kind =
                parseBrushKind(kindStr.value_or("Draw"));

            nexus::sculpt::StrokeDelta delta;
            delta.id   = state->nextId++;
            delta.kind = kind;

            // Optionally record one vertex delta
            const auto viOpt = shGetArg(cmd, "vi");
            if (viOpt) {
                uint32_t vi = 0;
                std::from_chars(viOpt->data(), viOpt->data() + viOpt->size(), vi);
                const float dx = floatArg(cmd, "dx", 0.f);
                const float dy = floatArg(cmd, "dy", 0.f);
                const float dz = floatArg(cmd, "dz", 0.f);
                delta.vertexDeltas.push_back({vi, {dx, dy, dz}});
            }

            state->history.push_back(std::move(delta));

            const std::string kindName = kindStr ? std::string(*kindStr) : "Draw";
            messages.push_back("stroke_hist.add ok"
                " id="   + std::to_string(state->history.back().id) +
                " kind=" + kindName);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── stroke_hist.serialize ─────────────────────────────────────────────────
    //
    // Serializes the current in-memory history to text.
    // Stores the result internally for later deserialize/describe.
    //
    // On success: "stroke_hist.serialize ok strokes=<N> bytes=<B>"
    // On error:   "stroke_hist.serialize fail: <diagnostic>"
    harness.registry().registerCommand("stroke_hist.serialize",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            nexus::sculpt::StrokeHistorySerializationReport report;
            const std::string data =
                nexus::sculpt::StrokeHistorySerializer::serialize(
                    state->history, report);

            if (!report.ok) {
                for (const auto& e : report.errors)
                    messages.push_back("stroke_hist.serialize fail: " + e);
                if (messages.empty())
                    messages.push_back("stroke_hist.serialize fail: unknown error");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->serialized    = data;
            state->hasSerialized = true;

            messages.push_back("stroke_hist.serialize ok"
                " strokes=" + std::to_string(report.strokeCount) +
                " bytes="   + std::to_string(data.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── stroke_hist.deserialize ───────────────────────────────────────────────
    //
    // Deserializes the last serialized text back into the in-memory history.
    // Returns false when nothing has been serialized yet or on parse errors.
    //
    // On success: "stroke_hist.deserialize ok strokes=<N>"
    // On error:   "stroke_hist.deserialize error: ..."
    harness.registry().registerCommand("stroke_hist.deserialize",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasSerialized) {
                messages.push_back("stroke_hist.deserialize error: nothing serialized yet");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::vector<nexus::sculpt::StrokeDelta> out;
            const auto report =
                nexus::sculpt::StrokeHistorySerializer::deserialize(
                    state->serialized, out);

            if (!report.ok) {
                for (const auto& e : report.errors)
                    messages.push_back("stroke_hist.deserialize error: " + e);
                if (messages.empty())
                    messages.push_back("stroke_hist.deserialize error: parse failure");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            // Replace in-memory history with deserialized result
            state->history = std::move(out);

            messages.push_back("stroke_hist.deserialize ok"
                " strokes=" + std::to_string(report.strokeCount));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── stroke_hist.clear ─────────────────────────────────────────────────────
    //
    // Clears the in-memory history and any cached serialized text.
    //
    // On success: "stroke_hist.clear ok"
    harness.registry().registerCommand("stroke_hist.clear",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->history.clear();
            state->serialized.clear();
            state->hasSerialized = false;
            state->nextId = 1u;

            messages.push_back("stroke_hist.clear ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── stroke_hist.describe ──────────────────────────────────────────────────
    //
    // Reports current in-memory history size and serialization state.
    // Always returns true.
    //
    // "stroke_hist.describe strokes=<N> serialized=<0|1>"
    harness.registry().registerCommand("stroke_hist.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("stroke_hist.describe"
                " strokes="    + std::to_string(state->history.size()) +
                " serialized=" + std::string(state->hasSerialized ? "1" : "0"));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
