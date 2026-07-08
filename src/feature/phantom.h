#pragma once
#include <iostream>
#include <vector>
#include <mutex>
#include <cmath>
#include <chrono>
#include <thread>
#include <functional>
#include "feature/cache.h"
#include "global.h"

std::mutex CMutex;

inline float distance(const sdk::vector3& P1, const sdk::vector3& P2) {
    float Dx = P1.x - P2.x;
    float Dy = P1.y - P2.y;
    float Dz = P1.z - P2.z;
    return std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
}

inline bool validpos(const sdk::vector3& Pos) {
    return !std::isnan(Pos.x) && !std::isnan(Pos.y) && !std::isnan(Pos.z);
}

// cacheplayer is only called when global::actor.children() returns empty
// (e.g. during game transitions, 1v1 arenas where characters are moved in workspace)
void cacheplayer(std::vector<sdk::player>& actor, const sdk::vector3& LocalPos, const std::string& LocalName) {
    std::lock_guard<std::mutex> Lock(CMutex);

    actor.clear();

    // Primary: enumerate every Player object from the Players service
    if (global::actor.Address != 0) {
        auto Player_Instances = global::actor.children();
        for (const auto& instance : Player_Instances) {
            if (!instance.Address) continue;

            sdk::player player{};
            player.player = sdk::actor(instance.Address);
            player.character = player.player.character();
            player.name = instance.name();
            player.UserID = player.player.userid();
            player.Display_Name = player.player.display();
            player.Local_Player = false;

            if (player.name == LocalName) continue;
            if (!player.character.Address) continue;

            // Find body parts
            for (auto& Child : player.character.childrenas<sdk::instance>()) {
                std::string ClassName = Child.kind();
                std::string ChildName = Child.name();

                if (ClassName == "Humanoid")           player.humanoid = Child;
                if (ChildName == "HumanoidRootPart")   player.HumanoidRootPart = Child;
                else if (ChildName == "Head")          player.Head = Child;
                else if (ChildName == "UpperTorso")    player.UpperTorso = Child;
                else if (ChildName == "LowerTorso")    player.LowerTorso = Child;
                else if (ChildName == "Torso")         player.Torso = Child;
                else if (ChildName == "LeftUpperArm")  player.LeftUpperArm = Child;
                else if (ChildName == "RightUpperArm") player.RightUpperArm = Child;
                else if (ChildName == "LeftLowerArm")  player.LeftLowerArm = Child;
                else if (ChildName == "RightLowerArm") player.RightLowerArm = Child;
                else if (ChildName == "LeftHand")      player.LeftHand = Child;
                else if (ChildName == "RightHand")     player.RightHand = Child;
                else if (ChildName == "LeftUpperLeg")  player.LeftUpperLeg = Child;
                else if (ChildName == "RightUpperLeg") player.RightUpperLeg = Child;
                else if (ChildName == "LeftLowerLeg")  player.LeftLowerLeg = Child;
                else if (ChildName == "RightLowerLeg") player.RightLowerLeg = Child;
                else if (ChildName == "LeftFoot")      player.LeftFoot = Child;
                else if (ChildName == "RightFoot")     player.RightFoot = Child;
                else if (ChildName == "Left Arm")      player.LeftArm = Child;
                else if (ChildName == "Right Arm")     player.RightArm = Child;
                else if (ChildName == "Left Leg")      player.LeftLeg = Child;
                else if (ChildName == "Right Leg")     player.RightLeg = Child;

                if (ClassName == "Part" || ClassName == "MeshPart")
                    player.Bones.push_back(Child);
            }

            if (player.HumanoidRootPart.Address != 0) {
                sdk::part Root(player.HumanoidRootPart.Address);
                sdk::vector3 PlayerPos = Root.partposition();
                if (validpos(PlayerPos) && validpos(LocalPos))
                    player.Distance = distance(LocalPos, PlayerPos);
            }

            if (global::setting::BotCheck && is_bot(player.name))
                continue;

            actor.push_back(std::move(player));
        }
    }

    // Fallback: if Players service gave us nothing, scan workspace for characters
    // This handles 1v1 arenas where characters are teleported to a separate location
    if (actor.empty() && global::workspace.Address != 0) {
        sdk::instance workspace(global::workspace.Address);

        std::function<void(sdk::instance, int)> scan;
        scan = [&](sdk::instance container, int depth) {
            if (depth > 4) return;
            for (auto& child : container.childrenas<sdk::instance>()) {
                if (!child.Address) continue;
                std::string kind = child.kind();
                std::string name = child.name();

                if (kind == "Model") {
                    // Detect character models by looking for a Humanoid child
                    bool isChar = false;
                    sdk::instance humanoidInst{};
                    sdk::instance hrpInst{};
                    sdk::instance headInst{};

                    for (auto& c : child.childrenas<sdk::instance>()) {
                        if (c.kind() == "Humanoid") { humanoidInst = c; isChar = true; }
                        if (c.name() == "HumanoidRootPart") hrpInst = c;
                        if (c.name() == "Head") headInst = c;
                    }

                    if (isChar && name != LocalName) {
                        sdk::player player{};
                        player.character = child;
                        player.name = name;
                        player.humanoid = humanoidInst;
                        player.HumanoidRootPart = hrpInst;
                        player.Head = headInst;

                        // Collect all body parts
                        for (auto& c : child.childrenas<sdk::instance>()) {
                            std::string ck = c.kind();
                            std::string cn = c.name();
                            if (ck == "Part" || ck == "MeshPart") player.Bones.push_back(c);
                            if (cn == "UpperTorso")    player.UpperTorso = c;
                            if (cn == "LowerTorso")    player.LowerTorso = c;
                            if (cn == "Torso")         player.Torso = c;
                            if (cn == "LeftUpperArm")  player.LeftUpperArm = c;
                            if (cn == "RightUpperArm") player.RightUpperArm = c;
                            if (cn == "LeftLowerArm")  player.LeftLowerArm = c;
                            if (cn == "RightLowerArm") player.RightLowerArm = c;
                            if (cn == "LeftHand")      player.LeftHand = c;
                            if (cn == "RightHand")     player.RightHand = c;
                            if (cn == "LeftUpperLeg")  player.LeftUpperLeg = c;
                            if (cn == "RightUpperLeg") player.RightUpperLeg = c;
                            if (cn == "LeftLowerLeg")  player.LeftLowerLeg = c;
                            if (cn == "RightLowerLeg") player.RightLowerLeg = c;
                            if (cn == "LeftFoot")      player.LeftFoot = c;
                            if (cn == "RightFoot")     player.RightFoot = c;
                            if (cn == "Left Arm")      player.LeftArm = c;
                            if (cn == "Right Arm")     player.RightArm = c;
                            if (cn == "Left Leg")      player.LeftLeg = c;
                            if (cn == "Right Leg")     player.RightLeg = c;
                        }

                        if (player.HumanoidRootPart.Address) {
                            sdk::part root(player.HumanoidRootPart.Address);
                            sdk::vector3 pos = root.partposition();
                            if (validpos(pos) && validpos(LocalPos))
                                player.Distance = distance(pos, LocalPos);
                        }

                        if (global::setting::BotCheck && is_bot(player.name))
                            continue;

                        actor.push_back(std::move(player));
                    }
                }

                if (kind == "Folder" || kind == "Model")
                    scan(child, depth + 1);
            }
        };

        scan(workspace, 0);
    }
}

void rescancache(std::vector<sdk::player>& actor, const sdk::vector3& LocalPos, const std::string& LocalName) {
    cacheplayer(actor, LocalPos, LocalName);
}
