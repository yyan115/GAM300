#pragma once
#include <variant>

struct SeenTarget {};
struct LostTarget {};

using BrainEvent = std::variant<SeenTarget, LostTarget>;