// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — AnimationClip serialization extension commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AnimationSerializationExtension.h>
#include <nexus/animation/AnimationCore.h>
#include <nexus/animation/AnimationSerialization.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
asGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = asGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

struct AnimSerialState {
    nexus::animation::AnimationClip clip;
    bool hasClip = false;
    std::string lastPath;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerAnimationSerializationCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<AnimSerialState>();

    // ── anim_serial.make_clip ─────────────────────────────────────────────────
    //
    // Creates a synthetic AnimationClip with given parameters.
    //
    // Arguments:
    //   duration=<f>   duration in seconds (default 1.0)
    //   rate=<f>       sample rate in Hz   (default 30.0)
    //   wrap=Clamp|Loop  wrap mode         (default Clamp)
    //
    // On success: "anim_serial.make_clip ok duration=<D> rate=<R>"
    harness.registry().registerCommand("anim_serial.make_clip",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const float duration = floatArg(cmd, "duration", 1.0f);
            const float rate     = floatArg(cmd, "rate",    30.0f);
            const auto  wrapStr  = asGetArg(cmd, "wrap");

            state->clip = nexus::animation::AnimationClip(duration, rate);
            if (wrapStr && *wrapStr == "Loop")
                state->clip.setWrapMode(nexus::animation::ClipWrapMode::Loop);

            state->hasClip = true;

            messages.push_back("anim_serial.make_clip ok"
                " duration=" + std::to_string(duration) +
                " rate="     + std::to_string(rate));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim_serial.save ──────────────────────────────────────────────────────
    //
    // Serializes the current clip to a binary file.
    // Returns false when no clip is loaded or the write fails.
    //
    // Arguments:
    //   path=<P>   file path (required)
    //
    // On success: "anim_serial.save ok path=<P> tracks=<T>"
    // On error:   "anim_serial.save error: ..."
    harness.registry().registerCommand("anim_serial.save",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasClip) {
                messages.push_back("anim_serial.save error: no clip loaded");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const auto pathArg = asGetArg(cmd, "path");
            if (!pathArg) {
                messages.push_back("anim_serial.save error: missing path argument");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string path(pathArg->data(), pathArg->size());

            const auto report =
                nexus::animation::AnimationClipSerializer::save(state->clip, path);

            if (!report.valid) {
                messages.push_back("anim_serial.save error: diagnostic=" +
                    std::to_string(static_cast<uint32_t>(report.diagnostic)));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->lastPath = path;
            messages.push_back("anim_serial.save ok"
                " path="   + path +
                " tracks=" + std::to_string(state->clip.trackCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim_serial.load ──────────────────────────────────────────────────────
    //
    // Deserializes a binary clip file into the internal state.
    //
    // Arguments:
    //   path=<P>   file path (default: last saved path)
    //
    // On success: "anim_serial.load ok tracks=<T> duration=<D>"
    // On error:   "anim_serial.load error: ..."
    harness.registry().registerCommand("anim_serial.load",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto pathArg = asGetArg(cmd, "path");
            const std::string path = pathArg
                ? std::string(pathArg->data(), pathArg->size())
                : state->lastPath;

            if (path.empty()) {
                messages.push_back("anim_serial.load error: no path");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::animation::AnimationClip loaded;
            const auto report =
                nexus::animation::AnimationClipSerializer::load(path, loaded);

            if (!report.valid) {
                messages.push_back("anim_serial.load error: diagnostic=" +
                    std::to_string(static_cast<uint32_t>(report.diagnostic)));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->clip    = std::move(loaded);
            state->hasClip = true;

            messages.push_back("anim_serial.load ok"
                " tracks="   + std::to_string(report.trackCount) +
                " duration=" + std::to_string(state->clip.durationSec()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── anim_serial.describe ─────────────────────────────────────────────────
    //
    // Reports current clip state.  Always returns true.
    //
    // "anim_serial.describe has_clip=<0|1> duration=<D> rate=<R> tracks=<T>"
    harness.registry().registerCommand("anim_serial.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const bool has = state->hasClip;
            messages.push_back("anim_serial.describe"
                " has_clip=" + std::string(has ? "1" : "0") +
                " duration=" + std::to_string(has ? state->clip.durationSec() : 0.f) +
                " rate="     + std::to_string(has ? state->clip.sampleRateHz() : 0.f) +
                " tracks="   + std::to_string(has ? state->clip.trackCount() : 0u));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
