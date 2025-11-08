#pragma once
#include "HfsmCommon.hpp"
#include "Logging.hpp"

struct BossIdle;
struct BossChase;

// Root machine type for Bosss
using BossFSM = AIMachine::PeerRoot<BossIdle, BossChase>;
using BossUpdateCtrl = typename BossFSM::FullControl;

struct BossIdle : BossFSM::State {
    template <typename TControl>
    void enter(TControl& ctrl) noexcept { ENGINE_PRINT("[Boss] enter Idle\n"); }

    template <typename TControl>
    void exit(TControl& /*ctrl*/) noexcept { /* optional */ }

    void update(BossUpdateCtrl& ctrl) noexcept {
        auto& ctx = ctrl.context();
        if (ctx.ev /*&& std::holds_alternative<SeenTarget>(*ctx.ev)*/)
            ctrl.changeTo<BossChase>();
    }
};

struct BossChase : BossFSM::State {
    template <typename TControl>
    void enter(TControl& ctrl) noexcept { ENGINE_PRINT("[Boss] enter Chase\n"); }

    template <typename TControl>
    void exit(TControl& /*ctrl*/) noexcept { /* optional */ }

    void update(BossUpdateCtrl& ctrl) noexcept {
        auto& ctx = ctrl.context();
        if (/* lost target */ false)
            ctrl.changeTo<BossIdle>();
    }
};