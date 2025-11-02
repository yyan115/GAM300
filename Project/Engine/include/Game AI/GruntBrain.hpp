#pragma once
#include "HfsmCommon.hpp"
#include "Logging.hpp"

struct GruntIdle;
struct GruntChase;

// Root machine type for grunts
using GruntFSM = AIMachine::PeerRoot<GruntIdle, GruntChase>;
using GruntUpdateCtrl = typename GruntFSM::FullControl;

struct GruntIdle : GruntFSM::State {
    template <typename TControl>
    void enter(TControl& ctrl) noexcept { ENGINE_PRINT("[Grunt] enter Idle\n"); }

    template <typename TControl>
    void exit(TControl& /*ctrl*/) noexcept {}

    void update(GruntUpdateCtrl& ctrl) noexcept {
        auto& ctx = ctrl.context();
        if (ctx.ev /*&& std::holds_alternative<SeenTarget>(*ctx.ev)*/)
            ctrl.changeTo<GruntChase>();
    }
};

struct GruntChase : GruntFSM::State {
    template <typename TControl>
    void enter(TControl& ctrl) noexcept { ENGINE_PRINT("[Grunt] enter Chase\n"); }

    template <typename TControl>
    void exit(TControl& /*ctrl*/) noexcept {}

    void update(GruntUpdateCtrl& ctrl) noexcept {
        auto& ctx = ctrl.context();
        if (/* lost target */ false)
            ctrl.changeTo<GruntIdle>();
    }
};