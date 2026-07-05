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
        writef("Overlay", "Watermark_X", global::overlay::Watermark_Pos.x, path);
        writef("Overlay", "Watermark_Y", global::overlay::Watermark_Pos.y, path);
        writef("Overlay", "Hotkeys_X", global::overlay::Hotkeys_Pos.x, path);
        writef("Overlay", "Hotkeys_Y", global::overlay::Hotkeys_Pos.y, path);
        writef("Overlay", "AimView_MaxLength", global::overlay::AimView_MaxLength, path);
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
        global::overlay::DamageLogs = readb("Overlay", "DamageLogs", global::overlay::DamageLogs, path);
        global::overlay::Watermark_Pos.x = readf("Overlay", "Watermark_X", global::overlay::Watermark_Pos.x, path);
        global::overlay::Watermark_Pos.y = readf("Overlay", "Watermark_Y", global::overlay::Watermark_Pos.y, path);
        global::overlay::Hotkeys_Pos.x = readf("Overlay", "Hotkeys_X", global::overlay::Hotkeys_Pos.x, path);
        global::overlay::Hotkeys_Pos.y = readf("Overlay", "Hotkeys_Y", global::overlay::Hotkeys_Pos.y, path);
        global::overlay::AimView_MaxLength = readf("Overlay", "AimView_MaxLength", global::overlay::AimView_MaxLength, path);
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
