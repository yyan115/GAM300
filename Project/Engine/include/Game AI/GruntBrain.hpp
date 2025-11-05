#pragma once
#include "HfsmCommon.hpp"
#include "Logging.hpp"
#include "Animation/AnimationComponent.hpp"

struct GruntIdle;
struct GruntAttack;

// Root machine type for grunts
using GruntFSM = AIMachine::PeerRoot<GruntIdle, GruntAttack>;
using GruntUpdateCtrl = typename GruntFSM::FullControl;

inline constexpr int kAttackClipIndex = 0;

// ---------- small helpers ----------
namespace {
    inline AnimationComponent* animFrom(auto& ctx) {
        if (auto opt = ctx.ecs->TryGetComponent<AnimationComponent>(ctx.e))
            return &opt->get();
        return nullptr;
    }

    inline void stopAll(AnimationComponent& a) {
        a.Stop();
    }

    inline void playOnce(AnimationComponent& a, int clipIdx) {
        a.PlayOnce(static_cast<std::size_t>(clipIdx));
    }

    // robust “is finished” check:
    inline bool finished(const AnimationComponent& a) {
        // if your player stops itself at the end (common), this works:
        if (!a.isPlay) return true;

        // optional: derive from duration if you expose it
        // (uncomment/adapt if you have these fields)
        // const auto& clip = a.clips[a.activeClip];
        // const float lenS = clip.durationTicks / std::max(1.0f, a.ticksPerSecond);
        // return a.time >= lenS;

        return false;
    }
}

struct GruntIdle : GruntFSM::State {
    float timer = 0.f;

    template <typename TControl>
    void enter(TControl& ctrl) noexcept { 
        ENGINE_PRINT("[Grunt] enter Idle\n"); 
        timer = 3.f;

        auto& ctx = ctrl.context();
        if (auto* a = animFrom(ctrl.context()))    // ensure nothing is playing
            stopAll(*a);
    }

    template <typename TControl>
    void exit(TControl& /*ctrl*/) noexcept {}

    void update(GruntUpdateCtrl& ctrl) noexcept {
        auto& ctx = ctrl.context();
        timer -= ctx.dt;
        if (timer <= 0.f)
            ctrl.changeTo<GruntAttack>();
    }
};

struct GruntAttack : GruntFSM::State {
    template <typename TControl>
    void enter(TControl& ctrl) noexcept { 
        ENGINE_PRINT("[Grunt] enter Attack\n"); 
        if (auto* a = animFrom(ctrl.context()))
            playOnce(*a, kAttackClipIndex);
    }

    template <typename TControl>
    void exit(TControl& ctrl) noexcept {
        // Make sure we leave in a clean state
        auto& ctx = ctrl.context();
        if (auto* a = animFrom(ctrl.context()))
            a->isPlay = false; // leave cleanly
    }

    void update(GruntUpdateCtrl& ctrl) noexcept {
        if (auto* a = animFrom(ctrl.context())) {
            if (finished(*a))
                ctrl.changeTo<GruntIdle>();
        }
        else {
            // no animation component -> immediately go back
            ctrl.changeTo<GruntIdle>();
        }
    }
};