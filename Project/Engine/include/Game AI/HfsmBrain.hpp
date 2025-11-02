#pragma once
#include "HfsmCommon.hpp"
#include "Brain.hpp"

template <class RootFSM>
class HfsmBrain final : public IBrain {
public:
    using FSM = RootFSM;
    using Instance = typename FSM::Instance;

    void onEnter(ECSManager& ecs, unsigned e) override {
        _ctx.ecs = &ecs;
        _ctx.e = e;
        _ctx.dt = 0.f;
        _ctx.ev.reset();
        // instance already holds reference to _ctx via ctor
    }

    void onUpdate(ECSManager& ecs, unsigned e, float dt) override {
        _ctx.ecs = &ecs;
        _ctx.e = e;
        _ctx.dt = dt;
        _fsm.update();
    }

    void onExit(ECSManager& ecs, unsigned e) override {
        _ctx.ecs = &ecs;
        _ctx.e = e;
        _ctx.ev.reset();
        // nothing else required; states can react if you post an event
    }

private:
    HfsmContext _ctx{};
    Instance    _fsm{ _ctx };   // hfsm2 Instance constructed with context
};