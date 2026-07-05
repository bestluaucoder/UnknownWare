#pragma once

#include "engine/engine.h"

namespace wallcheck
{
    bool valid(const sdk::vector3& v);
    bool can_see(const sdk::vector3& origin, const sdk::vector3& target, std::uintptr_t skip_char = 0);
    void update_cache();
}
