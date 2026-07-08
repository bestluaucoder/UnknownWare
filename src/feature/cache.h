#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string_view>

#include <atomic>
#include <cmath>

#include "engine/engine.h"

namespace cache
{
    extern std::mutex Mutex;
    extern std::atomic<bool> Is_Running;
    void run();
}

namespace sdk
{
    class player
    {
    public:
		bool PartsCached = false;
        bool Local_Player;

        float Health;
        float MaxHealth;
        float Distance = 0.f;

        std::uint8_t Rig_Type;

        sdk::instance team;
        std::string name;
        std::string Display_Name;
        std::string Tool_Name;
        std::vector<sdk::instance> children;
        std::vector<sdk::instance> Bones;
        sdk::actor player;
        sdk::instance character;
        std::uint64_t UserID;
        sdk::instance Head;
        sdk::instance HumanoidRootPart;
        sdk::instance humanoid;

        sdk::instance LeftArm;
        sdk::instance RightArm;

        sdk::instance LeftHand;
        sdk::instance RightHand;
        sdk::instance LeftLowerArm;
        sdk::instance RightLowerArm;
        sdk::instance LeftUpperArm;
        sdk::instance RightUpperArm;

        sdk::instance LeftFoot;
        sdk::instance RightFoot;
        sdk::instance RightUpperLeg;
        sdk::instance LeftUpperLeg;
        sdk::instance LeftLowerLeg;
        sdk::instance RightLowerLeg;

        sdk::instance UpperTorso;
        sdk::instance LowerTorso;
        sdk::instance Torso;

        sdk::instance LeftLeg;
        sdk::instance RightLeg;
    };
}

namespace cache
{
    std::vector<sdk::player> snapshot();
}

inline bool is_bot(const std::string& name)
{
    std::string lower = name;
    for (auto& c : lower)
        if (c >= 'A' && c <= 'Z') c += 32;

    const char* patterns[] = { "bot", "npc", "ai_" };
    for (auto pat : patterns)
        if (lower.find(pat) != std::string::npos)
            return true;
    return false;
}
