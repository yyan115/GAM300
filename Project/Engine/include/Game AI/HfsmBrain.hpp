#pragma once
#include "HfsmCommon.hpp"
#include "Brain.hpp"

template <class RootFSM>
class HfsmBrain final : public IBrain {
public:
    using FSM = RootFSM;
    using Instance = typename FSM::Instance;

    //HfsmBrain() : _ctx{}, _fsm{ _ctx } {}

    void onEnter(ECSManager& ecs, unsigned e) override {
        _ctx.ecs = &ecs;
        _ctx.e = e;
        _ctx.dt = 0.f;
        _ctx.ev.reset();

        if (!_fsm)
            _fsm = std::make_unique<Instance>(_ctx);  // pass the persistent context

        //_fsm->enter();
    }

    void onUpdate(ECSManager& ecs, unsigned e, float dt) override {
        if (!_fsm) return;
        _ctx.ecs = &ecs;
        _ctx.e = e;
        _ctx.dt = dt;

        _fsm->update();
    }

    void onExit(ECSManager& ecs, unsigned e) override {
        _ctx.ecs = &ecs;
        _ctx.e = e;
        
        _fsm.reset();
        _ctx.ev.reset();
        _ctx.dt = 0.f;
    }

private:
    HfsmContext _ctx{};
	std::unique_ptr<Instance> _fsm;
};