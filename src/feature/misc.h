#pragma once
#include <vector>
#include <string>

namespace misc {
    void fly();
    void walkspeed();
    void hitbox();
    void desync();
    void tickrate();
    void anim_changer();
    void lighting();
    void run();

    // Returns list of animation pack names for the menu combo
    const char** anim_pack_names(int* count_out);
}
