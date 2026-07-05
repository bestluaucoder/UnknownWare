#pragma once
#include <vector>
#include "global.h"

namespace esp {

    std::vector<const sdk::instance*> bone(const sdk::player& player);
    void run();
}
