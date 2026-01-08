#pragma once
#include "HfsmCommon.hpp"
#include "Logging.hpp"
#include "Animation/AnimationComponent.hpp"

struct GruntIdle;
struct GruntAttack;

// Root machine type for grunts
using Config = hfsm2::Config::ContextT<HfsmContext&>;
using GruntFSM = hfsm2::MachineT<Config>::PeerRoot<GruntIdle, GruntAttack>;
using GruntUpdateCtrl = typename GruntFSM::FullControl;

inline constexpr int kAttackClipIndex = 0;

// ---------- small helpers ----------
namespace {
    inline AnimationComponent* animFrom(auto& ctx) {
        if (auto opt = ctx.ecs->template TryGetComponent<AnimationComponent>(ctx.e))
            return &opt->get();
        return nullptr;
    }

    inline void stopAll(AnimationComponent& a) {
        a.Stop();
    }

    inline void playOnce(AnimationComponent& a, std::size_t clipIdx) {
        a.EnsureAnimator();        // make sure animator exists
        a.SetLooping(false);
        a.SetClip(clipIdx);
        a.Play();                  // your PlayOnce wraps this; using explicit path is fine
    }

    // robust "is finished" check:
    inline bool finished(const AnimationComponent& a) {
        if (a.clipCount < 1) return true;
        // Robust in both PLAY (AnimationSystem ticks) and EDIT (Inspector ticks).
        const Animator* anim = a.GetAnimatorPtr();
        if (!anim) return true;                       // no animator -> treat as finished
        const size_t idx = a.GetActiveClipIndex();
        const Animation& clip = a.GetClip(idx);
        const float t = anim->GetCurrentTime();
        const float len = clip.GetDuration();
        return !a.isLoop && (t >= len - 1e-4f);
    }
}

struct GruntIdle : GruntFSM::State {
    float timer = 0.f;
	bool armed = false;

    template <typename TControl>
    void enter(TControl& ctrl) noexcept { 
        auto& ctx = ctrl.context();
        auto& brain = ctx.ecs->template GetComponent<BrainComponent>(ctx.e);
        brain.activeState = "Idle";
        ENGINE_PRINT("[Grunt] enter Idle\n");

        timer = 3.f;
        armed = false;
        if (auto* a = animFrom(ctx)) a->Stop();
    }

    template <typename TControl>
    void update(TControl& ctrl) noexcept {
        auto& ctx = ctrl.context();

        if (!armed) {                                 // swallow the very first update
            armed = true;
            return;
        }

        const float d = std::clamp(ctx.dt, 0.0f, 0.2f);
        timer -= d;
        if (timer <= 0.f)
            ctrl.template changeTo<GruntAttack>();
    }
};

struct GruntAttack : GruntFSM::State {
    template <typename TControl>
    void enter(TControl& ctrl) noexcept {
        auto& ctx = ctrl.context();
        auto& brain = ctx.ecs->template GetComponent<BrainComponent>(ctx.e);
        brain.activeState = "Attack";
        ENGINE_PRINT("[Grunt] enter Attack\n");

        if (auto* a = animFrom(ctx)) {
            a->EnsureAnimator();
            if (0 <= kAttackClipIndex && kAttackClipIndex < a->clipCount) {
                a->SetLooping(false);
                a->SetClip(static_cast<size_t>(kAttackClipIndex));
                a->SetSpeed(1.0f);
                a->Play();                             // let component manage isPlay/time
            }
            else {
                ENGINE_PRINT("[Grunt] invalid attack clip index");
            }
        }
    }

    template <typename TControl>
    void exit(TControl& ctrl) noexcept {
        if (auto* a = animFrom(ctrl.context())) a->Stop();
    }

    template <typename TControl>
    void update(TControl& ctrl) noexcept {
        if (auto* a = animFrom(ctrl.context())) {
            if (finished(*a))
                ctrl.template changeTo<GruntIdle>();
        }
        else {
            // no animation component -> immediately go back
            ctrl.template changeTo<GruntIdle>();
        }
    }
};