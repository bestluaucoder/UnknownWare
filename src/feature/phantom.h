#pragma once
#include <iostream>
#include <vector>
#include <mutex>
#include <cmath>
#include <chrono>
#include <thread>
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

std::vector<sdk::instance> playerlist() {
    std::vector<sdk::instance> TargetPlayers;

    const std::uint64_t WorkspaceAddr = global::workspace.Address;
    if (!WorkspaceAddr) return TargetPlayers;

    sdk::instance workspace(WorkspaceAddr);
    sdk::instance PlayersFolder = workspace.child("Players");
    if (!PlayersFolder.Address) return TargetPlayers;

    auto Teams = PlayersFolder.childrenas<sdk::instance>();
    if (Teams.size() != 2) return TargetPlayers;

    const std::uint64_t LocalPlayerAddr = global::actor.local().Address;
    if (!LocalPlayerAddr) return TargetPlayers;

    if (!global::setting::Team_Check) {
        for (auto& team : Teams) {
            for (auto& player : team.childrenas<sdk::instance>()) {
                if (player.kind() == "Model") {
                    TargetPlayers.push_back(player);
                }
            }
        }
    }
    else {
        sdk::instance LocalPlayer(LocalPlayerAddr);
        sdk::instance LocalChar = LocalPlayer.character();
    }

    return TargetPlayers;
}

void cacheplayer(std::vector<sdk::player>& actor, const sdk::vector3& LocalPos, const std::string& LocalName) {
    std::lock_guard<std::mutex> Lock(CMutex);

    actor.clear();

    auto PlayerModels = playerlist();

    for (auto& model : PlayerModels) {
        if (model.kind() != "Model") continue;

        sdk::player player{};
        player.character = model;
        player.Bones.clear();

        for (auto& Child : model.childrenas<sdk::instance>()) {
            std::string ClassName = Child.kind();

            if (ClassName == "Folder") {
                for (auto& part : Child.childrenas<sdk::instance>()) {
                    std::string PartClass = part.kind();
                    if (PartClass == "Part" || PartClass == "MeshPart") {
                        player.Bones.push_back(part);
                    }
                }
            }
            else if (ClassName == "Part" || ClassName == "MeshPart") {
                player.Bones.push_back(Child);
            }
        }

        if (!player.Bones.empty()) {
            player.HumanoidRootPart = player.Bones[0];
        }

        for (auto& part : player.Bones) {
            for (auto& Child : part.childrenas<sdk::instance>()) {
                if (Child.kind() == "BillboardGui") {
                    player.Head = part;
                    sdk::instance TextLabel = Child.childclass("TextLabel");
                    if (TextLabel.Address != 0) {
                        player.name = TextLabel.text();
                    }
                    break;
                }
            }
            if (player.Head.Address != 0) break;
        }

        if (player.HumanoidRootPart.Address != 0) {
            sdk::part Root(player.HumanoidRootPart.Address);
            sdk::vector3 PlayerPos = Root.partposition();
            if (validpos(PlayerPos) && validpos(LocalPos)) {
                player.Distance = distance(LocalPos, PlayerPos);
            }
        }

        if (!player.name.empty() && player.name != LocalName) {
            actor.push_back(std::move(player));
        }
    }
}

void rescancache(std::vector<sdk::player>& actor, const sdk::vector3& LocalPos, const std::string& LocalName) {
    cacheplayer(actor, LocalPos, LocalName);
}
