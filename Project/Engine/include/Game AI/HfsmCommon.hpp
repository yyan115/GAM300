#pragma once

// Plans must be ON before any hfsm2 headers are included in the TU.
#ifndef HFSM2_ENABLE_PLANS
#define HFSM2_ENABLE_PLANS
#endif

#include <optional>
#include <variant>
#include <type_traits>
#include <machine.hpp>

// ---------------- Context ----------------
struct HfsmContext {
    class ECSManager* ecs = nullptr;
    unsigned         e = 0;      // your EntityID
    float            dt = 0.f;
    // Use std::monostate as a placeholder so variant is never empty.
    // Add your real event types after std::monostate, e.g. std::variant<std::monostate, SeenTarget, LostTarget>
    std::optional<std::variant<std::monostate /*, SeenTarget, LostTarget, etc.*/>> ev;
};

// -------------- Machine / Config ----------
using AIConfig = hfsm2::Config::ContextT<HfsmContext>;
using AIMachine = hfsm2::MachineT<AIConfig>;

// ---- Enter/Exit control type (plans or not) ----
// Safely detect nested types using overload-based SFINAE so MSVC doesn't
// attempt to instantiate missing nested types on hfsm2::detail::* wrapper types.

namespace detail {

// Overload detection for T::PlanControl
template <typename T>
auto detect_PlanControl(int) -> typename T::PlanControl; // chosen only if T::PlanControl exists

template <typename>
auto detect_PlanControl(...) -> void; // fallback

template <typename T>
using detected_PlanControl_t = decltype(detect_PlanControl<T>(0));

// Overload detection for T::FullControl
template <typename T>
auto detect_FullControl(int) -> typename T::FullControl; // chosen only if T::FullControl exists

template <typename>
auto detect_FullControl(...) -> void; // fallback

template <typename T>
using detected_FullControl_t = decltype(detect_FullControl<T>(0));

// Helper to check "is detected"
template <typename T>
constexpr bool is_detected_v = !std::is_same_v<T, void>;

// Primary selection: prefer FSM::PlanControl, then FSM::FullControl,
// then AIMachine::PlanControl, then AIMachine::FullControl.
template <typename FSM>
struct AIEnterCtrl_impl {
private:
    using fsm_plan  = detected_PlanControl_t<FSM>;
    using fsm_full  = detected_FullControl_t<FSM>;
    using aim_plan  = detected_PlanControl_t<AIMachine>;
    using aim_full  = detected_FullControl_t<AIMachine>;

    static constexpr bool have_fsm_plan = is_detected_v<fsm_plan>;
    static constexpr bool have_fsm_full = is_detected_v<fsm_full>;
    static constexpr bool have_aim_plan = is_detected_v<aim_plan>;
    static constexpr bool have_aim_full = is_detected_v<aim_full>;

    static_assert(have_fsm_plan || have_fsm_full || have_aim_plan || have_aim_full,
                  "HFSM: could not find 'PlanControl' or 'FullControl' on FSM or AIMachine. "
                  "Ensure HFSM2_ENABLE_PLANS is defined before hfsm2 headers and use the public Machine type (AIMachine) as the template parameter.");

public:
    using type =
        std::conditional_t<
            have_fsm_plan,
            fsm_plan,
            std::conditional_t<
                have_fsm_full,
                fsm_full,
                std::conditional_t<
                    have_aim_plan,
                    aim_plan,
                    aim_full
                >
            >
        >;
};

} // namespace detail

template <typename FSM>
using AIEnterCtrl = typename detail::AIEnterCtrl_impl<FSM>::type;

// Sanity ping (you should see this exactly once per TU in build log)
#if defined(HFSM2_ENABLE_PLANS)
#pragma message("HFSM2: Plans ON (PlanControl requested; resolved at compile-time)")
#else
#pragma message("HFSM2: Plans OFF")
#endif