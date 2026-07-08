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
#include "feature/game.h"
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

    // Adaptive pattern matching for custom body part names
    bool matches_pattern(const std::string& name, const std::string& pattern) {
        std::string lower_name = name;
        std::string lower_pattern = pattern;
        
        // Convert to lowercase for case-insensitive comparison
        for (auto& c : lower_name) c = std::tolower(c);
        for (auto& c : lower_pattern) c = std::tolower(c);
        
        // Check if the name contains the pattern
        return lower_name.find(lower_pattern) != std::string::npos;
    }

    sdk::instance sdk::player::* find_member_for_pattern(const std::string& name) {
        // Pattern matching for common body part variations
        if (matches_pattern(name, "humanoid") && !matches_pattern(name, "root")) {
            return &sdk::player::humanoid;
        }
        if (matches_pattern(name, "humanoidrootpart") || matches_pattern(name, "hrp")) {
            return &sdk::player::HumanoidRootPart;
        }
        if (matches_pattern(name, "head")) {
            return &sdk::player::Head;
        }
        if (matches_pattern(name, "uppertorso")) {
            return &sdk::player::UpperTorso;
        }
        if (matches_pattern(name, "lowertorso")) {
            return &sdk::player::LowerTorso;
        }
        if (matches_pattern(name, "torso") && !matches_pattern(name, "upper") && !matches_pattern(name, "lower")) {
            return &sdk::player::Torso;
        }
        if (matches_pattern(name, "leftarm") && !matches_pattern(name, "upper") && !matches_pattern(name, "lower")) {
            return &sdk::player::LeftArm;
        }
        if (matches_pattern(name, "rightarm") && !matches_pattern(name, "upper") && !matches_pattern(name, "lower")) {
            return &sdk::player::RightArm;
        }
        if (matches_pattern(name, "leftleg") && !matches_pattern(name, "upper") && !matches_pattern(name, "lower") && !matches_pattern(name, "foot")) {
            return &sdk::player::LeftLeg;
        }
        if (matches_pattern(name, "rightleg") && !matches_pattern(name, "upper") && !matches_pattern(name, "lower") && !matches_pattern(name, "foot")) {
            return &sdk::player::RightLeg;
        }
        if (matches_pattern(name, "leftupperleg")) {
            return &sdk::player::LeftUpperLeg;
        }
        if (matches_pattern(name, "rightupperleg")) {
            return &sdk::player::RightUpperLeg;
        }
        if (matches_pattern(name, "leftlowerleg")) {
            return &sdk::player::LeftLowerLeg;
        }
        if (matches_pattern(name, "rightlowerleg")) {
            return &sdk::player::RightLowerLeg;
        }
        if (matches_pattern(name, "leftfoot")) {
            return &sdk::player::LeftFoot;
        }
        if (matches_pattern(name, "rightfoot")) {
            return &sdk::player::RightFoot;
        }
        if (matches_pattern(name, "lefthand")) {
            return &sdk::player::LeftHand;
        }
        if (matches_pattern(name, "righthand")) {
            return &sdk::player::RightHand;
        }
        if (matches_pattern(name, "leftupperarm")) {
            return &sdk::player::LeftUpperArm;
        }
        if (matches_pattern(name, "rightupperarm")) {
            return &sdk::player::RightUpperArm;
        }
        if (matches_pattern(name, "leftlowerarm")) {
            return &sdk::player::LeftLowerArm;
        }
        if (matches_pattern(name, "rightlowerarm")) {
            return &sdk::player::RightLowerArm;
        }
        
        return nullptr;
    }


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

    static void find_parts(sdk::instance container, sdk::player& player)
    {
        for (const auto& child : container.children())
        {
            if (!child.Address) continue;

            std::string child_name = child.name();
            std::string kind = child.kind();

            // Only map named parts - skip accessories, tools, scripts etc.
            if (kind == "Part" || kind == "MeshPart" || kind == "Humanoid" || kind == "HumanoidController") {

                // Try exact match first
                auto it = Part_Lookup.find(child_name);
                if (it != Part_Lookup.end()) {
                    player.*(it->second) = child;
                }
                else {
                    // Try pattern matching for custom body part names
                    auto member = find_member_for_pattern(child_name);
                    if (member != nullptr) {
                        player.*member = child;
                    }
                }

                // Store body parts in Bones for fallback rendering
                if (kind == "Part" || kind == "MeshPart") {
                    player.Bones.push_back(child);
                }
            }

            // Only recurse into Folders, not into Models (accessories, tools, etc.)
            // Recursing into nested Models causes overwriting of correctly found parts
            if (kind == "Folder")
                find_parts(child, player);
        }
    }

    void data(sdk::player& player, const sdk::vector3& Local_Pos, bool Is_Local) {
        if (player.character.Address == 0) return;

        // Clear stale bone data before finding parts
        player.Bones.clear();

        find_parts(player.character, player);

        // Always re-read humanoid health live — never trust cached values
        // This ensures death state updates immediately even if character is stale
        if (player.humanoid.Address) {
            try {
                sdk::humanoid humanoid(player.humanoid.Address);
                float h  = humanoid.health();
                float mh = humanoid.maxhealth();
                // Sanity check: if values are NaN or negative treat as 0
                player.Health    = (std::isnan(h)  || h  < 0.f) ? 0.f : h;
                player.MaxHealth = (std::isnan(mh) || mh < 0.f) ? 0.f : mh;
                player.Rig_Type  = humanoid.rig();
            } catch (...) {
                player.Health    = 0.f;
                player.MaxHealth = 0.f;
            }
        } else {
            // No humanoid found — player is likely dead/respawning
            player.Health    = 0.f;
            player.MaxHealth = 0.f;
        }

        player.Tool_Name.clear();
        sdk::instance tool = player.character.childclass("Tool");
        if (tool.Address) {
            player.Tool_Name = tool.name();
        }

        if (!Is_Local && global::camera.Address != 0) {

            sdk::camera camera(global::camera.Address);
            sdk::vector3 Camera_Pos = camera.position();

            sdk::vector3 Target_Pos{};
            bool found_pos = false;

            if (player.Head.Address) {
                sdk::part p(player.Head.Address);
                Target_Pos = p.partposition();
                found_pos = validpos(Target_Pos);
            }
            if (!found_pos && player.HumanoidRootPart.Address) {
                sdk::part p(player.HumanoidRootPart.Address);
                Target_Pos = p.partposition();
                found_pos = validpos(Target_Pos);
            }
            if (!found_pos && player.UpperTorso.Address) {
                sdk::part p(player.UpperTorso.Address);
                Target_Pos = p.partposition();
                found_pos = validpos(Target_Pos);
            }
            if (!found_pos && player.Torso.Address) {
                sdk::part p(player.Torso.Address);
                Target_Pos = p.partposition();
                found_pos = validpos(Target_Pos);
            }
            if (!found_pos && !player.Bones.empty()) {
                sdk::part p(player.Bones[0].Address);
                Target_Pos = p.partposition();
                found_pos = validpos(Target_Pos);
            }

            if (found_pos && validpos(Camera_Pos))
                player.Distance = distance(Target_Pos, Camera_Pos);
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
            // Always re-fetch character — it changes on death/respawn
            player.character = player.player.character();
            player.name = instance.name();
            player.UserID = player.player.userid();
            player.Display_Name = player.player.display();
            player.Local_Player = false;

            // Skip players with no character (haven't spawned yet)
            if (!player.character.Address) continue;

            data(player, Local_Pos, false);

            sdk::actor Entity(instance.Address);
            sdk::actor LocalPlayer(Local_Inst.Address);

            if (global::setting::Team_Check) {
                uint64_t localTeam = LocalPlayer.teamid();
                if (localTeam != 0 && Entity.teamid() == localTeam) {
                    continue;
                }
            }

            if (global::setting::BotCheck && is_bot(player.name)) {
                continue;
            }

            if (!global::setting::Client_Check || 
                (global::LocalPlayer.HumanoidRootPart.Address == 0) ||
                (player.HumanoidRootPart.Address != global::LocalPlayer.HumanoidRootPart.Address)) {

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

        static int empty_attempts = 0;

        auto children = global::actor.children();
        
        if (!children.empty()) {
            empty_attempts = 0;
            update(Local_Position, LocalPlayer.name);
        }
        else if (++empty_attempts >= 2) {  // Reduced from 3 to 2 for faster fallback
            // Use fallback detection (workspace search)
            std::vector<sdk::player> players;
            cacheplayer(players, Local_Position, LocalPlayer.name);
            if (!players.empty()) {
                std::lock_guard<std::mutex> lock(Mutex);
                global::Player_Cache = std::move(players);
                empty_attempts = 0; // Reset after successful fallback
            }
        }

        game::detect();
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

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}




