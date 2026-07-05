#include "cache.h"

#include <vector>
#include <unordered_map>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <cmath>
#include <cstdio>
#include "log.h"
#include "global.h"
#include "feature/phantom.h"
#include "feature/wallcheck.h"
#include "ui/notify.h"
namespace cache {

    struct partmap {

        std::string_view name;
        sdk::instance sdk::player::* Member;
    };

    constexpr partmap Part_Mappings[] = {

        {"Humanoid", &sdk::player::humanoid},
        {"HumanoidRootPart", &sdk::player::HumanoidRootPart},
        {"Head", &sdk::player::Head},
        {"Torso", &sdk::player::Torso},
        {"UpperTorso", &sdk::player::UpperTorso},
        {"LowerTorso", &sdk::player::LowerTorso},
        {"Left Arm", &sdk::player::LeftArm},
        {"Right Arm", &sdk::player::RightArm},
        {"Left Leg", &sdk::player::LeftLeg},
        {"Right Leg", &sdk::player::RightLeg},
        {"LeftUpperLeg", &sdk::player::LeftUpperLeg},
        {"RightUpperLeg", &sdk::player::RightUpperLeg},
        {"LeftLowerLeg", &sdk::player::LeftLowerLeg},
        {"RightLowerLeg", &sdk::player::RightLowerLeg},
        {"LeftFoot", &sdk::player::LeftFoot},
        {"RightFoot", &sdk::player::RightFoot},
        {"LeftHand", &sdk::player::LeftHand},
        {"RightHand", &sdk::player::RightHand},
        {"LeftUpperArm", &sdk::player::LeftUpperArm},
        {"RightUpperArm", &sdk::player::RightUpperArm},
        {"LeftLowerArm", &sdk::player::LeftLowerArm},
        {"RightLowerArm", &sdk::player::RightLowerArm}
    };

    std::unordered_map<std::string, sdk::instance sdk::player::*> partlookup() {

        std::unordered_map<std::string, sdk::instance sdk::player::*> Map;
        Map.reserve(sizeof(Part_Mappings) / sizeof(Part_Mappings[0]));
        for (const auto& Mapping : Part_Mappings) {
            Map.emplace(Mapping.name, Mapping.Member);
        }
        return Map;
    }

    const auto Part_Lookup = partlookup();
    std::atomic<bool> References_Updated{ false };
    std::atomic<std::uint64_t> Current_GameID{ 0 };
    std::mutex Mutex;

    struct healthsample
    {
        float Health = 0.f;
        std::chrono::steady_clock::time_point LastLog{};
    };

    std::unordered_map<std::uint64_t, healthsample> Health_Log;

    std::vector<sdk::player> snapshot()
    {
        std::lock_guard<std::mutex> lock(Mutex);
        return global::Player_Cache;
    }

    inline float distance(const sdk::vector3& P1, const sdk::vector3& P2) {

        float Dx = P1.x - P2.x;
        float Dy = P1.y - P2.y;
        float Dz = P1.z - P2.z;
        return std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
    }

    bool validpos(const sdk::vector3& Pos) {

        return !std::isnan(Pos.x) && !std::isnan(Pos.y) && !std::isnan(Pos.z);
    }

    void damage_logs(const std::vector<sdk::player>& players)
    {
        if (!global::overlay::DamageLogs)
        {
            Health_Log.clear();
            return;
        }

        const sdk::player& local = global::LocalPlayer;
        if (!local.character.Address || local.Health <= 0.f)
        {
            Health_Log.clear();
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        std::unordered_map<std::uint64_t, bool> seen;
        seen.reserve(players.size());

        for (const sdk::player& player : players)
        {
            if (!player.character.Address || player.Health <= 0.f || player.MaxHealth <= 0.f)
                continue;
            if (local.character.Address && player.character.Address == local.character.Address)
                continue;

            const std::uint64_t key = player.UserID ? player.UserID : player.character.Address;
            seen[key] = true;

            auto& sample = Health_Log[key];
            if (sample.Health > 0.f)
            {
                const float delta = sample.Health - player.Health;
                const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - sample.LastLog).count();
                if (delta >= 1.f && since >= 220)
                {
                    char body[128]{};
                    std::snprintf(body, sizeof(body), "%s -%.0f HP (%.0f left)",
                        player.name.empty() ? "Player" : player.name.c_str(),
                        delta,
                        player.Health);
                    notify::push(notify::kind::damage, "Damage", body, 2.7f);
                    sample.LastLog = now;
                }
            }

            sample.Health = player.Health;
        }

        for (auto it = Health_Log.begin(); it != Health_Log.end();)
        {
            if (seen.find(it->first) == seen.end())
                it = Health_Log.erase(it);
            else
                ++it;
        }
    }

    void rescan() {
        static std::uint64_t Stored_GameID = 0;

        while (true) {
            try {
                auto fakemodel = drive->read<std::uint64_t>(drive->modulebase() + offset::fakemodel::Pointer);
                global::model.Address = drive->read<std::uint64_t>(fakemodel + offset::fakemodel::RealDataModel);

                if (global::model.Address != 0) {
                    std::uint64_t GameID = drive->read<uint64_t>(global::model.Address + offset::datamodel::PlaceId);

                    if (GameID != Stored_GameID) {
                        Stored_GameID = GameID;
                        Current_GameID.store(GameID);
                        global::GameID = GameID;
                        char body[96]{};
                        std::snprintf(body, sizeof(body), "New game id detected: %llu",
                            static_cast<unsigned long long>(GameID));
                        notify::push(notify::kind::info, "Game changed", body);
                        global::actor.Address = global::model.childclass("Players").Address;
                        auto Lightin = global::model.childclass("Lighting");
                        global::light = sdk::light(Lightin.Address);
                        global::workspace.Address = global::model.childclass("Workspace").Address;
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        global::camera.Address = global::workspace.childclass("Camera").Address;

                        References_Updated.store(true);

                        output::ok("%-18s%u", "process", drive->processid());
                        output::ok("%-18s0x%llx", "handle", drive->processhandle());
                        output::ok("%-18s0x%llx", "module", drive->modulebase());
                        output::core("datamodel", "0x%llx", global::model);
                        output::core("visual engine", "0x%llx", global::render);
                        output::core("players", "0x%llx", global::actor);
                        output::core("local player", "0x%llx", global::LocalPlayer);
                        output::core("workspace", "0x%llx", global::workspace);
                        output::core("camera", "0x%llx", global::camera);
                        output::core("lighting", "0x%llx", global::light);
                    }
                }
            }
            catch (...) {}

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    void data(sdk::player& player, const sdk::vector3& Local_Pos, bool Is_Local) {
        if (player.character.Address == 0) return;

        auto children = player.character.children();
        for (const auto& part : children) {

            auto it = Part_Lookup.find(part.name());
            if (it != Part_Lookup.end()) {

                player.*(it->second) = part;
            }
        }

        if (player.humanoid.Address) {

            sdk::humanoid humanoid(player.humanoid.Address);
            player.Health = humanoid.health();
            player.MaxHealth = humanoid.maxhealth();
            player.Rig_Type = humanoid.rig();
        }

        player.Tool_Name.clear();
        sdk::instance tool = player.character.childclass("Tool");
        if (tool.Address) {

            player.Tool_Name = tool.name();
        }

        if (!Is_Local && player.Head.Address != 0 && global::camera.Address != 0) {

            sdk::part Head(player.Head.Address);
            sdk::vector3 Head_Pos = Head.partposition();

            sdk::camera camera(global::camera.Address);
            sdk::vector3 Camera_Pos = camera.position();

            if (validpos(Head_Pos) && validpos(Camera_Pos)) {
                player.Distance = distance(Head_Pos, Camera_Pos);
            }
        }
    }

    void update(const sdk::vector3& Local_Pos, const std::string& Local_Name) {
        if (global::actor.Address == 0)
            return;

        sdk::actor Local_SDK_Player = global::actor.local();
        sdk::instance Local_Inst(Local_SDK_Player.Address);

        auto Player_Instances = global::actor.children();

        std::vector<sdk::player> actor;

        actor.reserve(Player_Instances.size());

        for (const auto& instance : Player_Instances) {

            sdk::player player{};
            player.player = sdk::actor(instance.Address);
            player.character = player.player.character();
            player.name = instance.name();
            player.UserID = player.player.userid();
            player.Display_Name = player.player.display();
            player.Local_Player = false;

            data(player, Local_Pos, false);

            sdk::actor Entity(instance.Address);
            sdk::actor LocalPlayer(Local_Inst.Address);

            if (global::setting::Team_Check) {
                uint64_t localTeam = LocalPlayer.teamid();
                if (localTeam != 0 && Entity.teamid() == localTeam) {
                    continue;
                }
            }

            if (!global::setting::Client_Check || player.HumanoidRootPart.Address != global::LocalPlayer.HumanoidRootPart.Address) {

                actor.push_back(std::move(player));
            }
        }

        damage_logs(actor);

        std::lock_guard<std::mutex> Lock(Mutex);
        global::Player_Cache = std::move(actor);
    }

    void runtime() {
        if (global::actor.Address == 0)
            return;

        sdk::actor LocalPlayerInstance = global::actor.local();
        sdk::instance LocalPlayer2(LocalPlayerInstance.Address);
        if (LocalPlayer2.Address == 0)
            return;

        sdk::instance Player_Instance(LocalPlayer2);

        sdk::player LocalPlayer{};
        LocalPlayer.player = sdk::actor(Player_Instance.Address);
        LocalPlayer.character = Player_Instance.character();
        LocalPlayer.name = Player_Instance.name();
        LocalPlayer.Local_Player = true;

        sdk::vector3 Local_Position{};
        data(LocalPlayer, Local_Position, true);

        if (LocalPlayer.HumanoidRootPart.Address != 0) {
            sdk::part Local_HumanoidRootPart(LocalPlayer.HumanoidRootPart.Address);
            Local_Position = Local_HumanoidRootPart.partposition();
        }

        global::LocalPlayer = LocalPlayer;
        global::GameID = Current_GameID.load();

        if (global::GameID == 292439477) {
            std::vector<sdk::player> players;

            cacheplayer(players, Local_Position, LocalPlayer.name);
            {
                std::lock_guard<std::mutex> lock(Mutex);
                global::Player_Cache = std::move(players);
            }
        }
        else {
            update(Local_Position, LocalPlayer.name);
        }
    }
}

void cache::run() {

    std::thread(rescan).detach();
    int wallcheck_counter = 0;

    while (true) {
        try {
            if (References_Updated.exchange(false)) {
                std::lock_guard<std::mutex> lock(Mutex);
                global::Player_Cache.clear();
                wallcheck::update_cache();
            }

            if (++wallcheck_counter >= 7) {
                wallcheck_counter = 0;
                wallcheck::update_cache();
            }

            runtime();
        }
        catch (...) {}

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}




