#pragma once
#include <string>
#include <vector>
#include "engine/engine.h"

namespace game {
    struct Profile {
        bool is_r15 = false;
        bool is_r6 = false;
        bool has_custom_rig = false;

        bool has_head = false;
        bool has_torso = false;
        bool has_upper_torso = false;
        bool has_lower_torso = false;
        bool has_root = false;

        enum Source { STANDARD, WORKSPACE, NONE };
        Source source = NONE;

        bool dead_uses_bodyeffects = false;

        bool has_teams = false;
        int player_count = 0;
        int hitbox_part_count = 0;
    };

    const Profile& get();
    void detect();
}
