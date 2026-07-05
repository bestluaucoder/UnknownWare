#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "feature/ball.h"
#include "global.h"
#include "engine/engine.h"
#include "engine/offset.h"
#include "imgui/imgui.h"

namespace ball
{
    static bool s_ballInRange = false;
    static bool s_recentParry = false;
    static auto s_lastParry = std::chrono::steady_clock::now();

    struct track
    {
        sdk::vector3 Position{};
        sdk::vector3 Velocity{};
        std::chrono::steady_clock::time_point Time{};
        bool Ready = false;
    };

    struct threat
    {
        bool valid = false;
        std::uint64_t Address = 0;
        sdk::vector3 Position{};
        float Distance = FLT_MAX;
        float Speed = 0.f;
        float ApproachSpeed = 0.f;
        float RelativeApproachSpeed = 0.f;
        float TimeToImpactMs = FLT_MAX;
        float AutoRange = 0.f;
    };

    static std::unordered_map<std::uint64_t, track> s_tracks;
    static std::uint64_t s_lastBall = 0;
    static std::mutex s_ballsMutex;
    static std::vector<sdk::instance> s_cachedBalls;
    static auto s_lastBallScan = std::chrono::steady_clock::time_point{};
    static bool s_spamToggle = false;
    static bool s_spamWasDown = false;

    static bool valid(const sdk::vector3& v)
    {
        return !std::isnan(v.x) && !std::isnan(v.y) && !std::isnan(v.z) &&
            !(v.x == 0.f && v.y == 0.f && v.z == 0.f);
    }

    static float dot(const sdk::vector3& a, const sdk::vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static sdk::vector3 normalize(const sdk::vector3& v)
    {
        const float len = v.magnitude();
        if (len < 0.001f || std::isnan(len))
            return {};
        return v / len;
    }

    static sdk::vector3 localpos()
    {
        sdk::instance part = global::LocalPlayer.HumanoidRootPart.Address
            ? global::LocalPlayer.HumanoidRootPart
            : global::LocalPlayer.Torso;
        if (!part.Address)
            part = global::LocalPlayer.Head;
        if (!part.Address)
            return {};

        sdk::part primitive = sdk::part(part.Address).primitive();
        if (!primitive.Address)
            return {};

        return primitive.position();
    }

    static sdk::vector3 localvel()
    {
        sdk::instance part = global::LocalPlayer.HumanoidRootPart.Address
            ? global::LocalPlayer.HumanoidRootPart
            : global::LocalPlayer.Torso;
        if (!part.Address)
            return {};

        sdk::part primitive = sdk::part(part.Address).primitive();
        if (!primitive.Address)
            return {};

        sdk::vector3 velocity = drive->read<sdk::vector3>(primitive.Address + offset::primitive::AssemblyLinearVelocity);
        if (std::isnan(velocity.x) || std::isnan(velocity.y) || std::isnan(velocity.z))
            return {};
        return velocity;
    }

    static std::uint64_t localchar()
    {
        return global::LocalPlayer.character.Address;
    }

    static bool partclass(const std::string& klass)
    {
        return klass == "Part" || klass == "MeshPart" || klass == "UnionOperation";
    }

    static sdk::instance findchild(const sdk::instance& inst, const std::string& wanted)
    {
        if (!inst.Address)
            return {};

        std::string lowerWanted = wanted;
        std::transform(lowerWanted.begin(), lowerWanted.end(), lowerWanted.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });

        for (const sdk::instance& child : inst.children())
        {
            std::string name = child.name();
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });
            if (name == lowerWanted)
                return child;
        }

        return {};
    }

    static std::vector<sdk::instance> scanball()
    {
        std::vector<sdk::instance> out;
        std::unordered_set<std::uint64_t> seen;
        if (!global::workspace.Address)
            return out;

        auto add = [&](const sdk::instance& inst)
            {
                if (!inst.Address || seen.find(inst.Address) != seen.end())
                    return;
                seen.insert(inst.Address);
                out.push_back(inst);
            };

        auto looksLikeBall = [](const sdk::instance& inst)
            {
                const std::string name = inst.name();
                const std::string klass = inst.kind();

                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c) { return (char)std::tolower(c); });

                const bool numeric = !name.empty() && std::all_of(name.begin(), name.end(),
                    [](unsigned char c) { return std::isdigit(c) != 0; });
                const bool ballName = lower.find("ball") != std::string::npos;
                const bool hasTarget = findchild(inst, "target").Address != 0;
                return partclass(klass) && (numeric || ballName || hasTarget);
            };

        auto scanChildren = [&](const sdk::instance& root, int depth, bool broadPartMatch, auto&& scanChildrenRef) -> void
            {
                if (!root.Address || depth < 0)
                    return;

                for (const sdk::instance& child : root.children())
                {
                    if (!child.Address)
                        continue;

                    if (looksLikeBall(child) || (broadPartMatch && partclass(child.kind())))
                        add(child);

                    const std::string name = child.name();
                    std::string lower = name;
                    std::transform(lower.begin(), lower.end(), lower.begin(),
                        [](unsigned char c) { return (char)std::tolower(c); });
                    const bool numeric = !name.empty() && std::all_of(name.begin(), name.end(),
                        [](unsigned char c) { return std::isdigit(c) != 0; });

                    if (depth > 0 && (numeric || lower.find("ball") != std::string::npos ||
                        lower == "workspace" || lower == "alive" || lower == "runtime"))
                    {
                        scanChildrenRef(child, depth - 1, broadPartMatch, scanChildrenRef);
                    }
                }
            };

        sdk::instance ballsFolder = global::workspace.child("Balls");
        if (ballsFolder.Address)
        {
            scanChildren(ballsFolder, 3, true, scanChildren);
            if (!out.empty()) return out;
        }

        scanChildren(global::workspace, 2, false, scanChildren);

        return out;
    }

    static void refreshball(bool force = false)
    {
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(s_ballsMutex);
            if (!force && now - s_lastBallScan < std::chrono::milliseconds(20))
                return;
            s_lastBallScan = now;
        }

        std::vector<sdk::instance> balls = scanball();
        {
            std::lock_guard<std::mutex> lock(s_ballsMutex);
            s_cachedBalls = std::move(balls);
        }
    }

    static std::vector<sdk::instance> balllist()
    {
        std::lock_guard<std::mutex> lock(s_ballsMutex);
        return s_cachedBalls;
    }

    static bool targetlocal(const sdk::instance& ball)
    {
        const std::uint64_t character = localchar();
        if (!character)
            return false;

        sdk::instance target = findchild(ball, "target");
        if (!target.Address)
            return false;

        const std::string klass = target.kind();
        if (klass.find("StringValue") != std::string::npos)
        {
            const std::string name = drive->readstring(target.Address + offset::misc::Value);
            return !name.empty() && name == global::LocalPlayer.name;
        }

        const std::uint64_t ref = drive->read<std::uint64_t>(target.Address + offset::misc::Value);
        if (!ref)
            return false;
        return ref == character || ref == global::LocalPlayer.player.Address;
    }

    static float parryrange(float relativeSpeed, float manualRange)
    {
        if (!global::ball::AutoRange)
            return std::max(1.f, manualRange);

        const float speed = std::max(0.f, relativeSpeed);
        const float latencyPad = 6.5f;
        const float reactionWindow = std::clamp(global::ball::ParryTimingMs / 1000.f, .085f, .22f);
        const float speedPad = std::clamp(speed * reactionWindow, 10.f, 70.f);
        return std::clamp(speedPad + latencyPad, 12.f, 95.f);
    }

    static float timingwindow(float relativeSpeed)
    {
        if (!global::ball::AutoTiming)
            return std::clamp(global::ball::ParryTimingMs, 60.f, 420.f);

        const float speed = std::max(0.f, relativeSpeed);
        if (speed >= 260.f) return 245.f;
        if (speed >= 180.f) return 205.f;
        if (speed >= 120.f) return 170.f;
        if (speed >= 70.f) return 135.f;
        return 110.f;
    }

    static bool ballpart(const sdk::instance& ball, sdk::part& primitive)
    {
        if (!ball.Address)
            return false;

        primitive = sdk::part(ball.Address).primitive();
        if (!primitive.Address)
            return false;
        return true;
    }

    static bool ballpos(const sdk::instance& ball, sdk::vector3& out)
    {
        sdk::part primitive{};
        if (!ballpart(ball, primitive))
            return false;

        out = primitive.position();
        return valid(out);
    }

    static sdk::vector3 ballvelocity(const sdk::instance& ball)
    {
        sdk::part primitive{};
        if (!ballpart(ball, primitive))
            return {};

        sdk::vector3 velocity = drive->read<sdk::vector3>(primitive.Address + offset::primitive::AssemblyLinearVelocity);
        if (std::isnan(velocity.x) || std::isnan(velocity.y) || std::isnan(velocity.z))
            velocity = {};

        sdk::instance zoomies = findchild(ball, "zoomies");
        if (zoomies.Address)
        {
            const sdk::vector3 vectorVelocity = drive->read<sdk::vector3>(zoomies.Address + offset::misc::Value);
            if (!std::isnan(vectorVelocity.x) && !std::isnan(vectorVelocity.y) && !std::isnan(vectorVelocity.z) &&
                vectorVelocity.magnitude() > velocity.magnitude())
            {
                velocity = vectorVelocity;
            }
        }
        return velocity;
    }

    static threat bestthreat(const sdk::vector3& local, const sdk::vector3& localVelocity)
    {
        threat best{};
        const auto now = std::chrono::steady_clock::now();
        const float manualRange = std::max(1.f, global::ball::ParryDistance);
        std::vector<std::uint64_t> seen;

        for (const sdk::instance& ball : balllist())
        {
            sdk::vector3 ballPos{};
            if (!ballpos(ball, ballPos))
                continue;
            if (!targetlocal(ball))
                continue;

            seen.push_back(ball.Address);
            const float dist = local.distance(ballPos);

            track& track = s_tracks[ball.Address];
            sdk::vector3 velocity = ballvelocity(ball);
            if (track.Ready)
            {
                const float dt = std::chrono::duration<float>(now - track.Time).count();
                if (dt > 0.001f && dt < 0.25f)
                {
                    const sdk::vector3 sampled = (ballPos - track.Position) / dt;
                    if (sampled.magnitude() > velocity.magnitude())
                        velocity = sampled;
                }
            }

            track.Position = ballPos;
            track.Velocity = velocity;
            track.Time = now;
            track.Ready = true;

            const sdk::vector3 toLocal = local - ballPos;
            const sdk::vector3 dirToLocal = normalize(toLocal);
            const float speed = velocity.magnitude();
            const float approach = dot(velocity, dirToLocal);
            const float playerApproach = dot(localVelocity, dirToLocal);
            const float relativeApproach = approach - playerApproach;
            const bool movingAtLocal = relativeApproach > std::max(1.f, global::ball::MinBallSpeed);
            const float parryRange = parryrange(relativeApproach, manualRange);
            const float buffer = global::ball::AutoRange
                ? std::clamp(parryRange * .38f, 5.f, 26.f)
                : std::clamp(global::ball::DistanceBuffer, 0.f, 100.f);
            const float impactDistance = std::max(0.f, dist - buffer);
            const float speedTti = (speed > 1.f && dist <= parryRange * .72f) ? (impactDistance / speed) * 1000.f : FLT_MAX;
            const float approachTti = movingAtLocal ? (impactDistance / relativeApproach) * 1000.f : FLT_MAX;
            const float tti = std::min(approachTti, speedTti);

            const bool ultraClose = dist <= std::min(8.f, parryRange * .35f);
            const bool inDistanceRange = dist <= parryRange;
            float scriptTimingWindow = timingwindow(relativeApproach);
            if (global::ball::DynamicTiming)
            {
                const float speedBoost = std::clamp(relativeApproach / 350.f, 0.f, 1.f);
                scriptTimingWindow = std::clamp(scriptTimingWindow + speedBoost * 95.f, 80.f, 420.f);
            }
            const bool inTimingWindow = global::ball::PredictTiming &&
                movingAtLocal &&
                tti <= scriptTimingWindow &&
                dist <= parryRange * 1.65f;

            if (!ultraClose && !inDistanceRange && !inTimingWindow)
                continue;

            const bool better = !best.valid ||
                tti < best.TimeToImpactMs ||
                (tti == best.TimeToImpactMs && dist < best.Distance);
            if (better)
            {
                best.valid = true;
                best.Address = ball.Address;
                best.Position = ballPos;
                best.Distance = dist;
                best.Speed = speed;
                best.ApproachSpeed = approach;
                best.RelativeApproachSpeed = relativeApproach;
                best.TimeToImpactMs = tti;
                best.AutoRange = parryRange;
            }
        }

        for (auto it = s_tracks.begin(); it != s_tracks.end();)
        {
            const bool stillSeen = std::find(seen.begin(), seen.end(), it->first) != seen.end();
            const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.Time).count();
            if (!stillSeen && age > 1)
                it = s_tracks.erase(it);
            else
                ++it;
        }

        return best;
    }

    static void click()
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    static void pressf()
    {
        INPUT input[2]{};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = 'F';
        input[0].ki.wScan = MapVirtualKeyA('F', MAPVK_VK_TO_VSC);
        input[1] = input[0];
        input[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, input, sizeof(INPUT));
    }

    static void parry(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            click();
            if (global::ball::pressf)
                pressf();
            if (i + 1 < count)
                std::this_thread::sleep_for(std::chrono::microseconds(500));
        }

        s_lastParry = std::chrono::steady_clock::now();
        s_recentParry = false;
    }

    static int imguikey(ImGuiKey key)
    {
        if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
            return 'A' + (int)(key - ImGuiKey_A);
        if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
            return '0' + (int)(key - ImGuiKey_0);
        if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12)
            return VK_F1 + (int)(key - ImGuiKey_F1);

        switch (key)
        {
        case ImGuiKey_LeftShift: return VK_LSHIFT;
        case ImGuiKey_RightShift: return VK_RSHIFT;
        case ImGuiKey_LeftCtrl: return VK_LCONTROL;
        case ImGuiKey_RightCtrl: return VK_RCONTROL;
        case ImGuiKey_LeftAlt: return VK_LMENU;
        case ImGuiKey_RightAlt: return VK_RMENU;
        case ImGuiKey_Space: return VK_SPACE;
        case ImGuiKey_Insert: return VK_INSERT;
        case ImGuiKey_Delete: return VK_DELETE;
        case ImGuiKey_Home: return VK_HOME;
        case ImGuiKey_End: return VK_END;
        case ImGuiKey_PageUp: return VK_PRIOR;
        case ImGuiKey_PageDown: return VK_NEXT;
        default: return 0;
        }
    }

    static bool keydown(ImGuiKey key)
    {
        const int vk = imguikey(key);
        return vk != 0 && (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    static bool spamactive()
    {
        if (!global::ball::SpamParry)
        {
            s_spamToggle = false;
            s_spamWasDown = false;
            return false;
        }

        const bool down = keydown(global::ball::SpamParry_Key);
        if (global::ball::SpamParry_Mode == ImKeyBindMode_Hold)
            return down;

        if (down && !s_spamWasDown)
            s_spamToggle = !s_spamToggle;
        s_spamWasDown = down;
        return s_spamToggle;
    }

    void run()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        for (;;)
        {
            if (global::GameID != global::ball::PlaceId)
            {
                s_ballInRange = false;
                s_recentParry = false;
                s_lastBall = 0;
                s_tracks.clear();
                {
                    std::lock_guard<std::mutex> lock(s_ballsMutex);
                    s_cachedBalls.clear();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                continue;
            }

            refreshball();

            if (!global::ball::AutoParry)
            {
                s_ballInRange = false;
                s_recentParry = false;
                s_lastBall = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }

            const sdk::vector3 local = localpos();
            if (!valid(local))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
                continue;
            }

            if (spamactive())
            {
                parry(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }

            const sdk::vector3 localVelocity = localvel();
            const threat threat = bestthreat(local, localVelocity);
            const bool inRange = threat.valid;
            const bool close = threat.valid && threat.Distance <= 10.f;
            const bool danger = threat.valid && (threat.Distance <= 10.f || threat.TimeToImpactMs <= 65.f);

            const auto now = std::chrono::steady_clock::now();
            const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastParry).count();
            int cooldown = (int)std::max(1.f, global::ball::ParryCooldownMs);
            if (threat.valid)
            {
                if (threat.Distance <= 5.f)
                    cooldown = 1;
                else if (threat.TimeToImpactMs <= 55.f || threat.Distance <= 10.f)
                    cooldown = 3;
                else if (threat.TimeToImpactMs <= 95.f || threat.RelativeApproachSpeed >= 120.f)
                    cooldown = std::min(cooldown, 18);
                else if (threat.RelativeApproachSpeed >= 80.f)
                    cooldown = std::min(cooldown, 35);
            }
            const bool cooldownReady = since >= cooldown;
            const bool newBall = threat.valid && threat.Address != 0 && threat.Address != s_lastBall;
            const bool perfectWindow = threat.valid &&
                (threat.Distance <= std::max(8.f, threat.AutoRange) ||
                    threat.TimeToImpactMs <= timingwindow(threat.RelativeApproachSpeed));
            const bool emergency = threat.valid && (danger || threat.Distance <= 12.f || threat.TimeToImpactMs <= 45.f);

            if (global::ball::ClashMode)
            {
                s_recentParry = false;
                if ((cooldownReady || newBall || emergency) && inRange && perfectWindow && (!s_ballInRange || newBall || emergency))
                    parry(danger ? 4 : 1);
                if (inRange && danger && since >= 3)
                    parry(2);
            }
            else
            {
                if (s_recentParry && since >= cooldown)
                    s_recentParry = false;
                if ((cooldownReady || newBall || emergency) && (!s_recentParry || newBall || emergency) && inRange && perfectWindow)
                {
                    const bool fastBall = threat.RelativeApproachSpeed > 95.f || threat.Speed > 110.f || threat.TimeToImpactMs <= 85.f;
                    parry((close || fastBall) ? 2 : 1);
                    s_recentParry = true;
                }
            }

            s_ballInRange = inRange;
            if (threat.valid)
                s_lastBall = threat.Address;
            if (!inRange)
                s_lastBall = 0;

            std::this_thread::sleep_for(std::chrono::milliseconds(inRange ? 1 : 3));
        }
    }

    static ImU32 tocolor(const float col[4])
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3]));
    }

    static ImU32 coloralpha(const float col[4], float alpha)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], std::clamp(col[3] * alpha, 0.f, 1.f)));
    }

    void render()
    {
        if (global::GameID != global::ball::PlaceId)
            return;

        if (!global::ball::BallEsp && !global::ball::DrawParryRange)
            return;

        const sdk::vector3 local = localpos();
        if (!valid(local) || !global::render.Address)
            return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float range = std::max(1.f, global::ball::ParryDistance);

        if (global::ball::DrawParryRange)
        {
            constexpr int kSegments = 64;
            ImVec2 points[kSegments + 1]{};
            int count = 0;
            bool closed = true;
            const float pulse = (sinf((float)ImGui::GetTime() * 4.2f) + 1.f) * .5f;

            for (int i = 0; i <= kSegments; ++i)
            {
                const float a = (float)i / (float)kSegments * 6.28318530718f;
                const sdk::vector3 point{
                    local.x + cosf(a) * range,
                    local.y,
                    local.z + sinf(a) * range
                };
                sdk::vector2 screen = global::render.screen(point);
                if (screen.x <= -0.5f || screen.y <= -0.5f)
                {
                    closed = false;
                    continue;
                }
                points[count++] = { screen.x, screen.y };
            }

            if (count > 1)
            {
                const ImDrawFlags flags = closed ? ImDrawFlags_Closed : ImDrawFlags_None;
                dl->AddPolyline(points, count, coloralpha(global::ball::ParryRangeColor, .13f + pulse * .05f),
                    flags, 7.5f);
                dl->AddPolyline(points, count, coloralpha(global::ball::ParryRangeColor, .28f),
                    flags, 3.2f);
                dl->AddPolyline(points, count, tocolor(global::ball::ParryRangeColor),
                    flags, 1.25f);
            }
        }

        if (!global::ball::BallEsp)
            return;

        refreshball();

        sdk::vector2 localScreen = global::render.screen(local);
        if (localScreen.x <= -0.5f || localScreen.y <= -0.5f)
            return;

        const ImU32 lineColor = tocolor(global::ball::BallEspColor);
        const ImU32 lineGlow = coloralpha(global::ball::BallEspColor, .22f);
        for (const sdk::instance& ball : balllist())
        {
            sdk::vector3 ballPos{};
            if (!ballpos(ball, ballPos))
                continue;

            sdk::vector2 ballScreen = global::render.screen(ballPos);
            if (ballScreen.x <= -0.5f || ballScreen.y <= -0.5f)
                continue;

            dl->AddLine({ localScreen.x, localScreen.y }, { ballScreen.x, ballScreen.y }, lineGlow, 5.f);
            dl->AddLine({ localScreen.x, localScreen.y }, { ballScreen.x, ballScreen.y }, lineColor, 1.6f);
            dl->AddCircleFilled({ ballScreen.x, ballScreen.y }, 7.f, lineGlow, 24);
            dl->AddCircleFilled({ ballScreen.x, ballScreen.y }, 3.8f, lineColor, 18);
        }
    }
}
