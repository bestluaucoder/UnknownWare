#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "global.h"

namespace fs = std::filesystem;

namespace config
{
    inline std::string dir()
    {
        char* appdata = nullptr;
        size_t size = 0;
        if (_dupenv_s(&appdata, &size, "APPDATA") == 0 && appdata)
        {
            fs::path dir = fs::path(appdata) / "Unknown";
            free(appdata);
            if (!fs::exists(dir))
                fs::create_directories(dir);
            return dir.string() + "\\";
        }
        fs::path fallback = "C:\\Unknown";
        if (!fs::exists(fallback))
            fs::create_directories(fallback);
        return fallback.string() + "\\";
    }

    inline void refresh(std::vector<std::string>& list)
    {
        list.clear();
        for (auto& entry : fs::directory_iterator(dir()))
            if (entry.path().extension() == ".ini")
                list.push_back(entry.path().stem().string());
    }

    inline void remove(const std::string& name)
    {
        fs::path p = dir() + name + ".ini";
        if (fs::exists(p)) fs::remove(p);
    }

    inline fs::path path(const std::string& name)
    {
        return fs::path(dir()) / (name + ".ini");
    }

    inline std::string encode64(const std::string& input)
    {
        static constexpr char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((input.size() + 2) / 3) * 4);

        int val = 0;
        int valb = -6;
        for (unsigned char c : input)
        {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0)
            {
                out.push_back(table[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6)
            out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4)
            out.push_back('=');
        return out;
    }

    inline bool decode64(const std::string& input, std::string& output)
    {
        static constexpr unsigned char invalid = 0xFF;
        unsigned char table[256];
        std::fill(std::begin(table), std::end(table), invalid);
        for (int i = 0; i < 26; ++i)
        {
            table[(unsigned char)('A' + i)] = (unsigned char)i;
            table[(unsigned char)('a' + i)] = (unsigned char)(26 + i);
        }
        for (int i = 0; i < 10; ++i)
            table[(unsigned char)('0' + i)] = (unsigned char)(52 + i);
        table[(unsigned char)'+'] = 62;
        table[(unsigned char)'/'] = 63;

        output.clear();
        int val = 0;
        int valb = -8;
        for (unsigned char c : input)
        {
            if (c == '=')
                break;
            if (c == '\r' || c == '\n' || c == '\t' || c == ' ')
                continue;
            if (table[c] == invalid)
                return false;

            val = (val << 6) + table[c];
            valb += 6;
            if (valb >= 0)
            {
                output.push_back((char)((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return !output.empty();
    }

    inline bool export64(const std::string& name, std::string& output)
    {
        const fs::path file_path = path(name);
        std::ifstream file(file_path, std::ios::binary);
        if (!file)
            return false;

        std::ostringstream ss;
        ss << file.rdbuf();
        output = encode64(ss.str());
        return true;
    }

    inline bool import64(const std::string& name, const std::string& encoded)
    {
        std::string decoded;
        if (name.empty() || !decode64(encoded, decoded))
            return false;

        std::ofstream file(path(name), std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file.write(decoded.data(), (std::streamsize)decoded.size());
        return file.good();
    }


    static void writes(const char* sec, const char* key, const std::string& val, const char* path)
    {
        WritePrivateProfileStringA(sec, key, val.c_str(), path);
    }
    static void writeb(const char* sec, const char* key, bool v, const char* path)
    {
        WritePrivateProfileStringA(sec, key, v ? "1" : "0", path);
    }
    static void writef(const char* sec, const char* key, float v, const char* path)
    {
        WritePrivateProfileStringA(sec, key, std::to_string(v).c_str(), path);
    }
    static void writei(const char* sec, const char* key, int v, const char* path)
    {
        WritePrivateProfileStringA(sec, key, std::to_string(v).c_str(), path);
    }
    static void writec(const char* sec, const char* key, float col[4], const char* path)
    {
        auto k = [&](int i) { return std::string(key) + std::to_string(i); };
        for (int i = 0; i < 4; i++)
            WritePrivateProfileStringA(sec, k(i).c_str(), std::to_string(col[i]).c_str(), path);
    }

    static bool readb(const char* sec, const char* key, bool def, const char* path)
    {
        return GetPrivateProfileIntA(sec, key, def ? 1 : 0, path) != 0;
    }
    static float readf(const char* sec, const char* key, float def, const char* path)
    {
        char buf[64];
        GetPrivateProfileStringA(sec, key, std::to_string(def).c_str(), buf, sizeof(buf), path);
        try { return std::stof(buf); }
        catch (...) { return def; }
    }
    static int readi(const char* sec, const char* key, int def, const char* path)
    {
        return GetPrivateProfileIntA(sec, key, def, path);
    }
    static void readc(const char* sec, const char* key, float col[4], const char* path)
    {
        for (int i = 0; i < 4; i++)
        {
            auto k = std::string(key) + std::to_string(i);
            col[i] = readf(sec, k.c_str(), col[i], path);
        }
    }


    inline void save(const std::string& name)
    {
        std::string p = dir() + name + ".ini";
        const char* path = p.c_str();

        writeb("Settings", "Team_Check", global::setting::Team_Check, path);
        writeb("Settings", "Client_Check", global::setting::Client_Check, path);
        writeb("Settings", "BotCheck", global::setting::BotCheck, path);
        writeb("Settings", "Streamproof", global::setting::Streamproof, path);
        writei("Settings", "Performance_Mode", global::setting::Performance_Mode, path);
        writeb("Settings", "Compact_UI", global::setting::Compact_UI, path);
        writei("Settings", "Menu_Key", (int)global::setting::Menu_Key, path);
        writec("Settings", "Theme_Accent", global::setting::color::Accent, path);
        writec("Settings", "Theme_Accent2", global::setting::color::Accent2, path);
        writec("Settings", "Theme_Window", global::setting::color::Window, path);
        writec("Settings", "Theme_Card", global::setting::color::card, path);
        writec("Settings", "Theme_Text", global::setting::color::text, path);

        writeb("Overlay", "Watermark", global::overlay::watermark, path);
        writeb("Overlay", "Hotkeys", global::overlay::hotkey, path);
        writeb("Overlay", "DamageLogs", global::overlay::DamageLogs, path);
        writeb("Overlay", "Notifications", global::overlay::Notifications, path);
        writeb("Overlay", "ShowKeybinds", global::overlay::ShowKeybinds, path);
        writef("Overlay", "Watermark_X", global::overlay::Watermark_Pos.x, path);
        writef("Overlay", "Watermark_Y", global::overlay::Watermark_Pos.y, path);
        writef("Overlay", "Hotkeys_X", global::overlay::Hotkeys_Pos.x, path);
        writef("Overlay", "Hotkeys_Y", global::overlay::Hotkeys_Pos.y, path);
        writef("Overlay", "NotifPanel_X", global::overlay::Notif_Pos.x, path);
        writef("Overlay", "NotifPanel_Y", global::overlay::Notif_Pos.y, path);
        writef("Overlay", "MediaTracker_X", global::overlay::Media_Pos.x, path);
        writef("Overlay", "MediaTracker_Y", global::overlay::Media_Pos.y, path);
        writef("Overlay", "AimView_MaxLength", global::overlay::AimView_MaxLength, path);
        writef("Overlay", "Spotify", global::overlay::Spotify, path);
        writec("Overlay", "Hud_Accent", global::overlay::color::Accent, path);
        writec("Overlay", "Hud_Accent2", global::overlay::color::Accent2, path);
        writec("Overlay", "Hud_Panel", global::overlay::color::panel, path);
        writec("Overlay", "Hud_Text", global::overlay::color::text, path);

        writeb("Triggerbot", "TriggerBot", global::aim::TriggerBot, path);
        writei("Triggerbot", "TriggerBot_Key", (int)global::aim::TriggerBot_Key, path);
        writei("Triggerbot", "TriggerBot_Mode", (int)global::aim::TriggerBot_Mode, path);
        writei("Triggerbot", "TriggerDelayMs", global::aim::TriggerDelayMs, path);
        writeb("Triggerbot", "IntervalToggle", global::aim::IntervalToggle, path);
        writef("Triggerbot", "TriggerRadius", global::aim::TriggerRadius, path);
        writeb("Triggerbot", "HitPart_Head", global::aim::HitPart_Head, path);
        writeb("Triggerbot", "HitPart_Torso", global::aim::HitPart_Torso, path);
        writeb("Triggerbot", "HitPart_UpperTorso", global::aim::HitPart_UpperTorso, path);
        writeb("Triggerbot", "HitPart_LowerTorso", global::aim::HitPart_LowerTorso, path);
        writeb("Triggerbot", "HitPart_HumanoidRootPart", global::aim::HitPart_HumanoidRootPart, path);
        writeb("Triggerbot", "HitPart_LeftArm", global::aim::HitPart_LeftArm, path);
        writeb("Triggerbot", "HitPart_RightArm", global::aim::HitPart_RightArm, path);
        writeb("Triggerbot", "HitPart_LeftLeg", global::aim::HitPart_LeftLeg, path);
        writeb("Triggerbot", "HitPart_RightLeg", global::aim::HitPart_RightLeg, path);
        writeb("Triggerbot", "VisualiseHitbox", global::aim::VisualiseHitbox, path);
        writeb("Triggerbot", "VisibleCheck", global::aim::VisibleCheck, path);
        writeb("Triggerbot", "DeadCheck", global::aim::DeadCheck, path);
        writeb("Triggerbot", "FovCheck", global::aim::FovCheck, path);
        writeb("Triggerbot", "DistanceScale", global::aim::DistanceScale, path);
        writef("Triggerbot", "AutoHeadScale", global::aim::AutoHeadScale, path);
        writef("Triggerbot", "HitPart_Head_Scale", global::aim::HitPart_Head_Scale, path);

        writeb("Aimbot", "Enabled", global::aimbot::Enabled, path);
        writei("Aimbot", "Key", (int)global::aimbot::Key, path);
        writei("Aimbot", "Mode", (int)global::aimbot::Mode, path);
        writeb("Aimbot", "Legit", global::aimbot::Legit, path);
        writef("Aimbot", "FOV", global::aimbot::FOV, path);
        writef("Aimbot", "Smoothing", global::aimbot::Smoothing, path);
        writef("Aimbot", "Speed", global::aimbot::Speed, path);
        writei("Aimbot", "Hitbox", global::aimbot::Hitbox, path);
        writeb("Aimbot", "VisibleCheck", global::aimbot::VisibleCheck, path);
        writeb("Aimbot", "DeadCheck", global::aimbot::DeadCheck, path);
        writeb("Aimbot", "TeamCheck", global::aimbot::TeamCheck, path);
        writef("Aimbot", "Jitter", global::aimbot::Jitter, path);
        writef("Aimbot", "RandomFOV", global::aimbot::RandomFOV, path);
        writei("Aimbot", "TargetPriority", global::aimbot::TargetPriority, path);
        writeb("Aimbot", "Prediction", global::aimbot::Prediction, path);
        writeb("Aimbot", "AutoShoot", global::aimbot::AutoShoot, path);
        writei("Aimbot", "ReactionMs", global::aimbot::ReactionMs, path);
        writeb("Aimbot", "Sticky", global::aimbot::Sticky, path);
        writeb("Aimbot", "AutoSens", global::aimbot::AutoSens, path);
        writei("Aimbot", "SmoothEaseIn",  global::aimbot::SmoothEaseIn,  path);
        writei("Aimbot", "SmoothEaseOut", global::aimbot::SmoothEaseOut, path);

        // Misc
        writeb("Misc", "Fly",         global::misc::fly,         path);
        writei("Misc", "Fly_Key",     (int)global::misc::Fly_Key, path);
        writei("Misc", "Fly_Mode",    (int)global::misc::Fly_Mode, path);
        writef("Misc", "Fly_Speed",   global::misc::Fly_Speed,   path);
        writeb("Misc", "Walkspeed",   global::misc::walkspeed,   path);
        writef("Misc", "WS_Speed",    global::misc::Walkspeed_Speed, path);
        writeb("Misc", "Hitbox",      global::misc::hitbox,      path);
        writef("Misc", "Hitbox_X",    global::misc::Hitbox_Size_X, path);
        writef("Misc", "Hitbox_Y",    global::misc::Hitbox_Size_Y, path);
        writef("Misc", "Hitbox_Z",    global::misc::Hitbox_Size_Z, path);
        writeb("Misc", "Desync",      global::misc::Desync,      path);
        writei("Misc", "Desync_Key",  (int)global::misc::Desync_Key, path);
        writei("Misc", "Desync_Mode", (int)global::misc::Desync_Mode, path);

        writeb("Misc", "Tickrate",      global::misc::Tickrate,      path);
        writei("Misc", "Tickrate_Key",  (int)global::misc::Tickrate_Key, path);
        writei("Misc", "Tickrate_Mode", (int)global::misc::Tickrate_Mode, path);
        writef("Misc", "Tickrate_Value",global::misc::Tickrate_Value, path);

        writeb("Misc", "AnimChanger",   global::misc::AnimChanger,   path);
        writei("Misc", "AnimKey",       (int)global::misc::AnimKey,  path);
        writei("Misc", "AnimMode",      (int)global::misc::AnimMode, path);
        writei("Misc", "AnimPack",      global::misc::AnimPack,      path);

        writeb("Misc", "Lighting",      global::misc::Lighting,      path);
        writeb("Misc", "ClockTime",     global::misc::ClockTime,     path);
        writei("Misc", "ClockKey",      (int)global::misc::ClockKey, path);
        writei("Misc", "ClockMode",     (int)global::misc::ClockMode,path);
        writef("Misc", "ClockTimeValue",global::misc::ClockTimeValue,path);

        // Crosshair
        writeb("Crosshair", "Enabled",   global::crosshair::Enabled,   path);
        writei("Crosshair", "Type",      global::crosshair::Type,      path);
        writei("Crosshair", "Position",  global::crosshair::Position,  path);
        writef("Crosshair", "Size",      global::crosshair::Size,      path);
        writef("Crosshair", "Gap",       global::crosshair::Gap,       path);
        writef("Crosshair", "Thickness", global::crosshair::Thickness, path);
        writef("Crosshair", "SpinSpeed", global::crosshair::SpinSpeed, path);
        writeb("Crosshair", "Outline",   global::crosshair::Outline,   path);
        writec("Crosshair", "Color",     global::crosshair::Color,     path);

        // Tracer
        writeb("Tracer", "Enabled",   global::tracer::Enabled,   path);
        writei("Tracer", "Style",     global::tracer::Style,     path);
        writei("Tracer", "Origin",    global::tracer::Origin,    path);
        writei("Tracer", "Dest",      global::tracer::Dest,      path);
        writef("Tracer", "Thickness", global::tracer::Thickness, path);
        writeb("Tracer", "Outline",   global::tracer::Outline,   path);
        writec("Tracer", "Color",     global::tracer::Color,     path);

        // Magic bullet
        writeb("Silent", "MagicBullet", global::silent::MagicBullet, path);

        // Silent aim settings
        writeb("Silent", "Enabled",       global::silent::Enabled,       path);
        writei("Silent", "Key",           (int)global::silent::Silent_Key, path);
        writei("Silent", "Mode",          (int)global::silent::Silent_Mode, path);
        writeb("Silent", "VisibleCheck",  global::silent::VisibleCheck,  path);
        writeb("Silent", "KnockedCheck",  global::silent::KnockedCheck,  path);
        writeb("Silent", "UseFov",        global::silent::UseFov,        path);
        writef("Silent", "Fov",           global::silent::fov,           path);
        writeb("Silent", "GunBasedFov",   global::silent::GunBasedFov,   path);
        writei("Silent", "AimPart",       global::silent::AimPart,       path);
        writei("Silent", "TargetPriority",global::silent::TargetPriority,path);
        writeb("Silent", "StickyAim",     global::silent::StickyAim,     path);
        writeb("Silent", "Prediction",    global::silent::Prediction,    path);
        writeb("Silent", "AutoPrediction",global::silent::AutoPrediction,path);
        writef("Silent", "PredX",         global::silent::PredictionX,   path);
        writef("Silent", "PredY",         global::silent::PredictionY,   path);
        writef("Silent", "PredZ",         global::silent::PredictionZ,   path);
        writef("Triggerbot", "HitPart_Torso_Scale", global::aim::HitPart_Torso_Scale, path);
        writef("Triggerbot", "HitPart_UpperTorso_Scale", global::aim::HitPart_UpperTorso_Scale, path);
        writef("Triggerbot", "HitPart_LowerTorso_Scale", global::aim::HitPart_LowerTorso_Scale, path);
        writef("Triggerbot", "HitPart_HumanoidRootPart_Scale", global::aim::HitPart_HumanoidRootPart_Scale, path);
        writef("Triggerbot", "HitPart_LeftArm_Scale", global::aim::HitPart_LeftArm_Scale, path);
        writef("Triggerbot", "HitPart_RightArm_Scale", global::aim::HitPart_RightArm_Scale, path);
        writef("Triggerbot", "HitPart_LeftLeg_Scale", global::aim::HitPart_LeftLeg_Scale, path);
        writef("Triggerbot", "HitPart_RightLeg_Scale", global::aim::HitPart_RightLeg_Scale, path);

        writeb("Visuals", "Enabled", global::esp::Enabled, path);
        writeb("Visuals", "Box", global::esp::Box, path);
        writeb("Visuals", "Box_Fill", global::esp::Box_Fill, path);
        writeb("Visuals", "Box_Fill_Gradient", global::esp::Box_Fill_Gradient, path);
        writeb("Visuals", "Box_Fill_Gradient_Rotate", global::esp::Box_Fill_Gradient_Rotate, path);
        writeb("Visuals", "Healthbar", global::esp::Healthbar, path);
        writeb("Visuals", "Health", global::esp::Health, path);
        writeb("Visuals", "Name", global::esp::name, path);
        writeb("Visuals", "Distance", global::esp::Distance, path);
        writeb("Visuals", "Rig_Type", global::esp::Rig_Type, path);
        writeb("Visuals", "Tool", global::esp::tool, path);
        writeb("Visuals", "Skeleton", global::esp::Skeleton, path);
        writeb("Visuals", "Chams", global::esp::Chams, path);
        writeb("Visuals", "ChamsFade", global::esp::ChamsFade, path);
        writeb("Visuals", "Trails", global::esp::Trails, path);
        writeb("Visuals", "Chinese_Hat", global::esp::Chinese_Hat, path);
        writeb("Visuals", "AimLine", global::esp::aimline, path);
        writeb("Visuals", "VisibleCheck", global::esp::VisibleCheck, path);
        writeb("Visuals", "Dead_Check", global::esp::Dead_Check, path);
        writeb("Visuals", "Local_Player", global::esp::Local_Player, path);
        writeb("Visuals", "Limit_Distance", global::esp::Limit_Distance, path);
        writef("Visuals", "Render_Distance", global::esp::Render_Distance, path);
        writei("Visuals", "ChamsFadeSpeed", global::esp::ChamsFadeSpeed, path);
        writei("Visuals", "BoxFillSpeed", global::esp::BoxFillSpeed, path);
        writei("Visuals", "Healthbar_Type", global::esp::Healthbar_Type, path);
        writei("Visuals", "Box_Type", global::esp::Box_Type, path);
        writei("Visuals", "Box_Fill_Type", global::esp::Box_Fill_Type, path);
        writei("Visuals", "Name_Type", global::esp::Name_Type, path);
        writei("Visuals", "Gap", global::esp::gap, path);
        writei("Visuals", "Thickness", global::esp::Thickness, path);
        writec("Visuals", "Box", global::esp::color::Box, path);
        writec("Visuals", "BoxFill_Top", global::esp::color::BoxFill_Top, path);
        writec("Visuals", "BoxFill_Bottom", global::esp::color::BoxFill_Bottom, path);
        writec("Visuals", "Healthbar", global::esp::color::Healthbar, path);
        writec("Visuals", "Name", global::esp::color::name, path);
        writec("Visuals", "Distance", global::esp::color::Distance, path);
        writec("Visuals", "Rig_Type", global::esp::color::Rig_Type, path);
        writec("Visuals", "Tool", global::esp::color::tool, path);
        writec("Visuals", "Health", global::esp::color::Health, path);
        writec("Visuals", "Skeleton", global::esp::color::Skeleton, path);
        writec("Visuals", "Chams", global::esp::color::Chams, path);
        writec("Visuals", "ChamsOutline", global::esp::color::ChamsOutline, path);
        writec("Visuals", "Trails", global::esp::color::Trails, path);
        writec("Visuals", "ChineseHat", global::esp::color::hat, path);
        writec("Visuals", "AimLine", global::esp::color::aimline, path);
        writec("Visuals", "VisibleColor", global::esp::color::Visible, path);
        writec("Visuals", "NotVisibleColor", global::esp::color::NotVisible, path);
        writec("Visuals", "Healthbar_Top", global::esp::color::Healthbar_Top, path);
        writec("Visuals", "Healthbar_Middle", global::esp::color::Healthbar_Middle, path);
        writec("Visuals", "Healthbar_Bottom", global::esp::color::Healthbar_Bottom, path);
    }


    inline void load(const std::string& name)
    {
        std::string p = dir() + name + ".ini";
        if (!fs::exists(p)) return;
        const char* path = p.c_str();

        global::setting::Team_Check = readb("Settings", "Team_Check", global::setting::Team_Check, path);
        global::setting::Client_Check = readb("Settings", "Client_Check", global::setting::Client_Check, path);
        global::setting::BotCheck = readb("Settings", "BotCheck", global::setting::BotCheck, path);
        global::setting::Streamproof = readb("Settings", "Streamproof", global::setting::Streamproof, path);
        global::setting::Performance_Mode = readi("Settings", "Performance_Mode", global::setting::Performance_Mode, path);
        global::setting::Compact_UI = readb("Settings", "Compact_UI", global::setting::Compact_UI, path);
        const int menuKey = readi("Settings", "Menu_Key", (int)global::setting::Menu_Key, path);
        global::setting::Menu_Key = (menuKey > ImGuiKey_None && menuKey < ImGuiKey_COUNT)
            ? (ImGuiKey)menuKey
            : ImGuiKey_Insert;
        readc("Settings", "Theme_Accent", global::setting::color::Accent, path);
        readc("Settings", "Theme_Accent2", global::setting::color::Accent2, path);
        readc("Settings", "Theme_Window", global::setting::color::Window, path);
        readc("Settings", "Theme_Card", global::setting::color::card, path);
        readc("Settings", "Theme_Text", global::setting::color::text, path);

        global::overlay::watermark = readb("Overlay", "Watermark", global::overlay::watermark, path);
        global::overlay::hotkey = readb("Overlay", "Hotkeys", global::overlay::hotkey, path);
        global::overlay::DamageLogs    = readb("Overlay", "DamageLogs",    global::overlay::DamageLogs,    path);
        global::overlay::Notifications = readb("Overlay", "Notifications", global::overlay::Notifications, path);
        global::overlay::ShowKeybinds  = readb("Overlay", "ShowKeybinds",  global::overlay::ShowKeybinds,  path);
        global::overlay::Watermark_Pos.x = readf("Overlay", "Watermark_X", global::overlay::Watermark_Pos.x, path);
        global::overlay::Watermark_Pos.y = readf("Overlay", "Watermark_Y", global::overlay::Watermark_Pos.y, path);
        global::overlay::Hotkeys_Pos.x = readf("Overlay", "Hotkeys_X", global::overlay::Hotkeys_Pos.x, path);
        global::overlay::Hotkeys_Pos.y = readf("Overlay", "Hotkeys_Y", global::overlay::Hotkeys_Pos.y, path);
        global::overlay::Notif_Pos.x   = readf("Overlay", "NotifPanel_X", global::overlay::Notif_Pos.x, path);
        global::overlay::Notif_Pos.y   = readf("Overlay", "NotifPanel_Y", global::overlay::Notif_Pos.y, path);
        global::overlay::Media_Pos.x   = readf("Overlay", "MediaTracker_X", global::overlay::Media_Pos.x, path);
        global::overlay::Media_Pos.y   = readf("Overlay", "MediaTracker_Y", global::overlay::Media_Pos.y, path);
        global::overlay::AimView_MaxLength = readf("Overlay", "AimView_MaxLength", global::overlay::AimView_MaxLength, path);
        global::overlay::Spotify = readf("Overlay", "Spotify", global::overlay::Spotify, path);
        readc("Overlay", "Hud_Accent", global::overlay::color::Accent, path);
        readc("Overlay", "Hud_Accent2", global::overlay::color::Accent2, path);
        readc("Overlay", "Hud_Panel", global::overlay::color::panel, path);
        readc("Overlay", "Hud_Text", global::overlay::color::text, path);

        global::aim::TriggerBot = readb("Triggerbot", "TriggerBot", global::aim::TriggerBot, path);
        global::aim::TriggerBot_Key = (ImGuiKey)readi("Triggerbot", "TriggerBot_Key", (int)global::aim::TriggerBot_Key, path);
        global::aim::TriggerBot_Mode = (ImKeyBindMode)readi("Triggerbot", "TriggerBot_Mode", (int)global::aim::TriggerBot_Mode, path);
        global::aim::TriggerDelayMs = readi("Triggerbot", "TriggerDelayMs", global::aim::TriggerDelayMs, path);
        global::aim::IntervalToggle = readb("Triggerbot", "IntervalToggle", global::aim::IntervalToggle, path);
        global::aim::TriggerRadius = readf("Triggerbot", "TriggerRadius", global::aim::TriggerRadius, path);
        global::aim::HitPart_Head = readb("Triggerbot", "HitPart_Head", global::aim::HitPart_Head, path);
        global::aim::HitPart_Torso = readb("Triggerbot", "HitPart_Torso", global::aim::HitPart_Torso, path);
        global::aim::HitPart_UpperTorso = readb("Triggerbot", "HitPart_UpperTorso", global::aim::HitPart_UpperTorso, path);
        global::aim::HitPart_LowerTorso = readb("Triggerbot", "HitPart_LowerTorso", global::aim::HitPart_LowerTorso, path);
        global::aim::HitPart_HumanoidRootPart = readb("Triggerbot", "HitPart_HumanoidRootPart", global::aim::HitPart_HumanoidRootPart, path);
        global::aim::HitPart_LeftArm = readb("Triggerbot", "HitPart_LeftArm", global::aim::HitPart_LeftArm, path);
        global::aim::HitPart_RightArm = readb("Triggerbot", "HitPart_RightArm", global::aim::HitPart_RightArm, path);
        global::aim::HitPart_LeftLeg = readb("Triggerbot", "HitPart_LeftLeg", global::aim::HitPart_LeftLeg, path);
        global::aim::HitPart_RightLeg = readb("Triggerbot", "HitPart_RightLeg", global::aim::HitPart_RightLeg, path);
        global::aim::VisualiseHitbox = readb("Triggerbot", "VisualiseHitbox", global::aim::VisualiseHitbox, path);
        global::aim::VisibleCheck = readb("Triggerbot", "VisibleCheck", global::aim::VisibleCheck, path);
        global::aim::DeadCheck = readb("Triggerbot", "DeadCheck", global::aim::DeadCheck, path);
        global::aim::FovCheck = readb("Triggerbot", "FovCheck", global::aim::FovCheck, path);
        global::aim::DistanceScale = readb("Triggerbot", "DistanceScale", global::aim::DistanceScale, path);
        global::aim::AutoHeadScale = readf("Triggerbot", "AutoHeadScale", global::aim::AutoHeadScale, path);
        global::aim::HitPart_Head_Scale = readf("Triggerbot", "HitPart_Head_Scale", global::aim::HitPart_Head_Scale, path);
        global::aim::HitPart_Torso_Scale = readf("Triggerbot", "HitPart_Torso_Scale", global::aim::HitPart_Torso_Scale, path);
        global::aim::HitPart_UpperTorso_Scale = readf("Triggerbot", "HitPart_UpperTorso_Scale", global::aim::HitPart_UpperTorso_Scale, path);
        global::aim::HitPart_LowerTorso_Scale = readf("Triggerbot", "HitPart_LowerTorso_Scale", global::aim::HitPart_LowerTorso_Scale, path);
        global::aim::HitPart_HumanoidRootPart_Scale = readf("Triggerbot", "HitPart_HumanoidRootPart_Scale", global::aim::HitPart_HumanoidRootPart_Scale, path);
        global::aim::HitPart_LeftArm_Scale = readf("Triggerbot", "HitPart_LeftArm_Scale", global::aim::HitPart_LeftArm_Scale, path);
        global::aim::HitPart_RightArm_Scale = readf("Triggerbot", "HitPart_RightArm_Scale", global::aim::HitPart_RightArm_Scale, path);
        global::aim::HitPart_LeftLeg_Scale = readf("Triggerbot", "HitPart_LeftLeg_Scale", global::aim::HitPart_LeftLeg_Scale, path);
        global::aim::HitPart_RightLeg_Scale = readf("Triggerbot", "HitPart_RightLeg_Scale", global::aim::HitPart_RightLeg_Scale, path);

        global::aimbot::Enabled = readb("Aimbot", "Enabled", global::aimbot::Enabled, path);
        global::aimbot::Key = (ImGuiKey)readi("Aimbot", "Key", (int)global::aimbot::Key, path);
        global::aimbot::Mode = (ImKeyBindMode)readi("Aimbot", "Mode", (int)global::aimbot::Mode, path);
        global::aimbot::Legit = readb("Aimbot", "Legit", global::aimbot::Legit, path);
        global::aimbot::FOV = readf("Aimbot", "FOV", global::aimbot::FOV, path);
        global::aimbot::Smoothing = readf("Aimbot", "Smoothing", global::aimbot::Smoothing, path);
        global::aimbot::Speed = readf("Aimbot", "Speed", global::aimbot::Speed, path);
        global::aimbot::Hitbox = readi("Aimbot", "Hitbox", global::aimbot::Hitbox, path);
        global::aimbot::VisibleCheck = readb("Aimbot", "VisibleCheck", global::aimbot::VisibleCheck, path);
        global::aimbot::DeadCheck = readb("Aimbot", "DeadCheck", global::aimbot::DeadCheck, path);
        global::aimbot::TeamCheck = readb("Aimbot", "TeamCheck", global::aimbot::TeamCheck, path);
        global::aimbot::Jitter = readf("Aimbot", "Jitter", global::aimbot::Jitter, path);
        global::aimbot::RandomFOV = readf("Aimbot", "RandomFOV", global::aimbot::RandomFOV, path);
        global::aimbot::TargetPriority = readi("Aimbot", "TargetPriority", global::aimbot::TargetPriority, path);
        global::aimbot::Prediction = readb("Aimbot", "Prediction", global::aimbot::Prediction, path);
        global::aimbot::AutoShoot = readb("Aimbot", "AutoShoot", global::aimbot::AutoShoot, path);
        global::aimbot::ReactionMs = readi("Aimbot", "ReactionMs", global::aimbot::ReactionMs, path);
        global::aimbot::Sticky    = readb("Aimbot", "Sticky",   global::aimbot::Sticky,   path);
        global::aimbot::AutoSens      = readb("Aimbot", "AutoSens",      global::aimbot::AutoSens,      path);
        global::aimbot::SmoothEaseIn  = readi("Aimbot", "SmoothEaseIn",  global::aimbot::SmoothEaseIn,  path);
        global::aimbot::SmoothEaseOut = readi("Aimbot", "SmoothEaseOut", global::aimbot::SmoothEaseOut, path);

        global::misc::fly          = readb("Misc","Fly",        global::misc::fly,          path);
        global::misc::Fly_Key      = (ImGuiKey)readi("Misc","Fly_Key", (int)global::misc::Fly_Key, path);
        global::misc::Fly_Mode     = (ImKeyBindMode)readi("Misc","Fly_Mode",(int)global::misc::Fly_Mode,path);
        global::misc::Fly_Speed    = readf("Misc","Fly_Speed",  global::misc::Fly_Speed,    path);
        global::misc::walkspeed    = readb("Misc","Walkspeed",  global::misc::walkspeed,    path);
        global::misc::Walkspeed_Speed = readf("Misc","WS_Speed",global::misc::Walkspeed_Speed,path);
        global::misc::hitbox       = readb("Misc","Hitbox",     global::misc::hitbox,       path);
        global::misc::Hitbox_Size_X= readf("Misc","Hitbox_X",  global::misc::Hitbox_Size_X,path);
        global::misc::Hitbox_Size_Y= readf("Misc","Hitbox_Y",  global::misc::Hitbox_Size_Y,path);
        global::misc::Hitbox_Size_Z= readf("Misc","Hitbox_Z",  global::misc::Hitbox_Size_Z,path);
        global::misc::Desync       = readb("Misc","Desync",     global::misc::Desync,       path);
        global::misc::Desync_Key   = (ImGuiKey)readi("Misc","Desync_Key",(int)global::misc::Desync_Key,path);
        global::misc::Desync_Mode  = (ImKeyBindMode)readi("Misc","Desync_Mode",(int)global::misc::Desync_Mode,path);

        global::misc::Tickrate       = readb("Misc","Tickrate",       global::misc::Tickrate,      path);
        global::misc::Tickrate_Key   = (ImGuiKey)readi("Misc","Tickrate_Key",(int)global::misc::Tickrate_Key,path);
        global::misc::Tickrate_Mode  = (ImKeyBindMode)readi("Misc","Tickrate_Mode",(int)global::misc::Tickrate_Mode,path);
        global::misc::Tickrate_Value = readf("Misc","Tickrate_Value", global::misc::Tickrate_Value,path);

        global::misc::AnimChanger = readb("Misc","AnimChanger", global::misc::AnimChanger,path);
        global::misc::AnimKey     = (ImGuiKey)readi("Misc","AnimKey",(int)global::misc::AnimKey,path);
        global::misc::AnimMode    = (ImKeyBindMode)readi("Misc","AnimMode",(int)global::misc::AnimMode,path);
        global::misc::AnimPack    = readi("Misc","AnimPack",    global::misc::AnimPack,   path);

        global::misc::Lighting      = readb("Misc","Lighting",      global::misc::Lighting,     path);
        global::misc::ClockTime     = readb("Misc","ClockTime",     global::misc::ClockTime,    path);
        global::misc::ClockKey      = (ImGuiKey)readi("Misc","ClockKey",(int)global::misc::ClockKey,path);
        global::misc::ClockMode     = (ImKeyBindMode)readi("Misc","ClockMode",(int)global::misc::ClockMode,path);
        global::misc::ClockTimeValue= readf("Misc","ClockTimeValue",global::misc::ClockTimeValue,path);

        global::crosshair::Enabled   = readb("Crosshair","Enabled",  global::crosshair::Enabled,  path);
        global::crosshair::Type      = readi("Crosshair","Type",     global::crosshair::Type,     path);
        global::crosshair::Position  = readi("Crosshair","Position", global::crosshair::Position, path);
        global::crosshair::Size      = readf("Crosshair","Size",     global::crosshair::Size,     path);
        global::crosshair::Gap       = readf("Crosshair","Gap",      global::crosshair::Gap,      path);
        global::crosshair::Thickness = readf("Crosshair","Thickness",global::crosshair::Thickness,path);
        global::crosshair::SpinSpeed = readf("Crosshair","SpinSpeed",global::crosshair::SpinSpeed,path);
        global::crosshair::Outline   = readb("Crosshair","Outline",  global::crosshair::Outline,  path);
        readc("Crosshair","Color",     global::crosshair::Color, path);

        global::tracer::Enabled   = readb("Tracer","Enabled",  global::tracer::Enabled,  path);
        global::tracer::Style     = readi("Tracer","Style",    global::tracer::Style,    path);
        global::tracer::Origin    = readi("Tracer","Origin",   global::tracer::Origin,   path);
        global::tracer::Dest      = readi("Tracer","Dest",     global::tracer::Dest,     path);
        global::tracer::Thickness = readf("Tracer","Thickness",global::tracer::Thickness,path);
        global::tracer::Outline   = readb("Tracer","Outline",  global::tracer::Outline,  path);
        readc("Tracer","Color",     global::tracer::Color, path);

        global::silent::MagicBullet    = readb("Silent","MagicBullet",   global::silent::MagicBullet,   path);
        global::silent::Enabled        = readb("Silent","Enabled",        global::silent::Enabled,        path);
        global::silent::Silent_Key     = (ImGuiKey)readi("Silent","Key",  (int)global::silent::Silent_Key,path);
        global::silent::Silent_Mode    = (ImKeyBindMode)readi("Silent","Mode",(int)global::silent::Silent_Mode,path);
        global::silent::VisibleCheck   = readb("Silent","VisibleCheck",   global::silent::VisibleCheck,   path);
        global::silent::KnockedCheck   = readb("Silent","KnockedCheck",   global::silent::KnockedCheck,   path);
        global::silent::UseFov         = readb("Silent","UseFov",         global::silent::UseFov,         path);
        global::silent::fov            = readf("Silent","Fov",            global::silent::fov,            path);
        global::silent::GunBasedFov    = readb("Silent","GunBasedFov",    global::silent::GunBasedFov,    path);
        global::silent::AimPart        = readi("Silent","AimPart",        global::silent::AimPart,        path);
        global::silent::TargetPriority = readi("Silent","TargetPriority", global::silent::TargetPriority, path);
        global::silent::StickyAim      = readb("Silent","StickyAim",      global::silent::StickyAim,      path);
        global::silent::Prediction     = readb("Silent","Prediction",     global::silent::Prediction,     path);
        global::silent::AutoPrediction = readb("Silent","AutoPrediction", global::silent::AutoPrediction, path);
        global::silent::PredictionX    = readf("Silent","PredX",          global::silent::PredictionX,    path);
        global::silent::PredictionY    = readf("Silent","PredY",          global::silent::PredictionY,    path);
        global::silent::PredictionZ    = readf("Silent","PredZ",          global::silent::PredictionZ,    path);

        global::esp::Enabled = readb("Visuals", "Enabled", global::esp::Enabled, path);
        global::esp::Box = readb("Visuals", "Box", global::esp::Box, path);
        global::esp::Box_Fill = readb("Visuals", "Box_Fill", global::esp::Box_Fill, path);
        global::esp::Box_Fill_Gradient = readb("Visuals", "Box_Fill_Gradient", global::esp::Box_Fill_Gradient, path);
        global::esp::Box_Fill_Gradient_Rotate = readb("Visuals", "Box_Fill_Gradient_Rotate", global::esp::Box_Fill_Gradient_Rotate, path);
        global::esp::Healthbar = readb("Visuals", "Healthbar", global::esp::Healthbar, path);
        global::esp::Health = readb("Visuals", "Health", global::esp::Health, path);
        global::esp::name = readb("Visuals", "Name", global::esp::name, path);
        global::esp::Distance = readb("Visuals", "Distance", global::esp::Distance, path);
        global::esp::Rig_Type = readb("Visuals", "Rig_Type", global::esp::Rig_Type, path);
        global::esp::tool = readb("Visuals", "Tool", global::esp::tool, path);
        global::esp::Skeleton = readb("Visuals", "Skeleton", global::esp::Skeleton, path);
        global::esp::Chams = readb("Visuals", "Chams", global::esp::Chams, path);
        global::esp::ChamsFade = readb("Visuals", "ChamsFade", global::esp::ChamsFade, path);
        global::esp::Trails = readb("Visuals", "Trails", global::esp::Trails, path);
        global::esp::Chinese_Hat = readb("Visuals", "Chinese_Hat", global::esp::Chinese_Hat, path);
        global::esp::aimline = readb("Visuals", "AimLine", global::esp::aimline, path);
        global::esp::VisibleCheck = readb("Visuals", "VisibleCheck", global::esp::VisibleCheck, path);
        global::esp::Dead_Check   = readb("Visuals", "Dead_Check",   global::esp::Dead_Check,   path);
        global::esp::Local_Player = readb("Visuals", "Local_Player", global::esp::Local_Player, path);
        global::esp::Limit_Distance = readb("Visuals", "Limit_Distance", global::esp::Limit_Distance, path);
        global::esp::Render_Distance = readf("Visuals", "Render_Distance", global::esp::Render_Distance, path);
        global::esp::ChamsFadeSpeed = readi("Visuals", "ChamsFadeSpeed", global::esp::ChamsFadeSpeed, path);
        global::esp::BoxFillSpeed = readi("Visuals", "BoxFillSpeed", global::esp::BoxFillSpeed, path);
        global::esp::Healthbar_Type = readi("Visuals", "Healthbar_Type", global::esp::Healthbar_Type, path);
        global::esp::Box_Type = readi("Visuals", "Box_Type", global::esp::Box_Type, path);
        global::esp::Box_Fill_Type = readi("Visuals", "Box_Fill_Type", global::esp::Box_Fill_Type, path);
        global::esp::Name_Type = readi("Visuals", "Name_Type", global::esp::Name_Type, path);
        global::esp::gap = readi("Visuals", "Gap", global::esp::gap, path);
        global::esp::Thickness = readi("Visuals", "Thickness", global::esp::Thickness, path);
        readc("Visuals", "Box", global::esp::color::Box, path);
        readc("Visuals", "BoxFill_Top", global::esp::color::BoxFill_Top, path);
        readc("Visuals", "BoxFill_Bottom", global::esp::color::BoxFill_Bottom, path);
        readc("Visuals", "Healthbar", global::esp::color::Healthbar, path);
        readc("Visuals", "Name", global::esp::color::name, path);
        readc("Visuals", "Distance", global::esp::color::Distance, path);
        readc("Visuals", "Rig_Type", global::esp::color::Rig_Type, path);
        readc("Visuals", "Tool", global::esp::color::tool, path);
        readc("Visuals", "Health", global::esp::color::Health, path);
        readc("Visuals", "Skeleton", global::esp::color::Skeleton, path);
        readc("Visuals", "Chams", global::esp::color::Chams, path);
        readc("Visuals", "ChamsOutline", global::esp::color::ChamsOutline, path);
        readc("Visuals", "Trails", global::esp::color::Trails, path);
        readc("Visuals", "ChineseHat", global::esp::color::hat, path);
        readc("Visuals", "AimLine", global::esp::color::aimline, path);
        readc("Visuals", "VisibleColor", global::esp::color::Visible, path);
        readc("Visuals", "NotVisibleColor", global::esp::color::NotVisible, path);
        readc("Visuals", "Healthbar_Top", global::esp::color::Healthbar_Top, path);
        readc("Visuals", "Healthbar_Middle", global::esp::color::Healthbar_Middle, path);
        readc("Visuals", "Healthbar_Bottom", global::esp::color::Healthbar_Bottom, path);
    }
}
