#include "feature/game.h"
#include "feature/cache.h"
#include "global.h"

namespace game {
    static Profile Current;

    const Profile& get() { return Current; }

    void detect() {
        Profile p;

        auto players = cache::snapshot();
        p.player_count = (int)players.size();

        const sdk::player* ref = nullptr;
        for (auto& plr : players) {
            if (!plr.Local_Player) { ref = &plr; break; }
        }

        if (!ref) {
            Current = p;
            return;
        }

        p.has_head = ref->Head.Address != 0;
        p.has_torso = ref->Torso.Address != 0;
        p.has_upper_torso = ref->UpperTorso.Address != 0;
        p.has_lower_torso = ref->LowerTorso.Address != 0;
        p.has_root = ref->HumanoidRootPart.Address != 0;

        p.is_r15 = p.has_upper_torso && p.has_lower_torso;
        p.is_r6 = p.has_torso && !p.is_r15;
        p.has_custom_rig = !p.is_r6 && !p.is_r15;

        if (ref->character.Address) {
            sdk::instance be = ref->character.child("BodyEffects");
            if (be.Address) {
                sdk::instance ko = be.child("K.O");
                p.dead_uses_bodyeffects = ko.Address != 0;
            }
        }

        if (ref->player.Address) {
            sdk::actor a(ref->player.Address);
            p.has_teams = a.teamid() != 0;
        }

        int count = 0;
        if (p.has_head) count++;
        if (p.has_torso || p.has_upper_torso || p.has_lower_torso) count++;
        if (ref->LeftArm.Address || ref->RightArm.Address) count++;
        if (ref->LeftLeg.Address || ref->RightLeg.Address) count++;
        p.hitbox_part_count = count;

        Current = p;
    }
}
