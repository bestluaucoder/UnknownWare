#include <iostream>
#include <thread>
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

#include "driver/driver.h"
#include "global.h"
#include "log.h"
#include "ui/graphic.h"
#include "engine/engine.h"
#include "feature/cache.h"
#include "feature/aim.h"

#include <ShlObj.h>
#pragma comment(lib, "Shell32.lib")

namespace
{
    static std::string sx(const unsigned char* data, std::size_t size, unsigned char key)
    {
        std::string out;
        out.resize(size);
        for (std::size_t i = 0; i < size; ++i)
            out[i] = static_cast<char>(data[i] ^ static_cast<unsigned char>(key + (i * 13u)));
        return out;
    }

    void gate()
    {
        static constexpr unsigned char body[] = {
            0x22,0xeb,0xf9,0xee,0x8a,0xc7,0xb6,0xbe,0xb9,0x99,0x99,0x68,0x32,0x76,0x5f,0x19,
            0x25,0x3c,0x0d,0x1d,0x16,0xe2,0xe0,0xc4,0xc2,0xc2,0xe8,0x93,0xb0,0xaa,0xb9,0x29,
            0x77,0x4d,0x54,0x1d,0x25,0x39,0x08,0x08,0x5e,0xea,0xee,0xc4,0xdb,0xd3,0xad,0xbb,
            0x8a,0x96,0x20,0x62,0x74,0x07,0x55,0x34,0x3a,0x34,0x18,0x06,0xfb,0xa1,0xf0,0xc6,
            0xda,0xed,0xda,0xd7,0xa3,0x91,0x24,0x68,0x71,0x5e,0x18,0x35,0x33,0x36,0x08,0x59,
            0xe0,0xfc,0xd2,0x8d,0xce,0xaf,0xbd,0x92,0xc2,0xdb,0x67,0x67,0x02,0x48,0x53,0x3d,
            0x76,0x0a,0x04,0x5d,0xec,0xe5,0xcb,0xdc,0x9e,0xb8,0xb7,0x88,0x97,0x88,0x64,0x7c,
            0x54,0x56,0x60,0x28,0x36,0x14,0x11,0xad,0xae,0xe2,0xc7,0xc0,0xe2,0xa8,0xb3,0x9d,
            0xd6,0x70,0x73,0x7c,0x47,0x5a,0x21,0x35,0x70
        };
        static constexpr unsigned char title[] = {
            0x24,0x27,0x2b,0xc3,0xc9,0xf5,0xea,0xee,0xa1,0xb5,0x8b
        };

        const std::string text = sx(body, sizeof(body), 0x76);
        const std::string cap = sx(title, sizeof(title), 0x65);
        MessageBoxA(nullptr, text.c_str(), cap.c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    }

    bool process(const char* processName)
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return false;

        PROCESSENTRY32 entry{};
        entry.dwSize = sizeof(entry);

        bool found = false;
        if (Process32First(snapshot, &entry))
        {
            do
            {
                if (_stricmp(entry.szExeFile, processName) == 0)
                {
                    found = true;
                    break;
                }
            } while (Process32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return found;
    }

    void clearstate()
    {
        {
            std::lock_guard<std::mutex> lock(cache::Mutex);
            global::Player_Cache.clear();
        }

        global::GameID = 0;
        global::LocalPlayer = {};
        global::model.Address = 0;
        global::render.Address = 0;
        global::actor.Address = 0;
        global::workspace.Address = 0;
        global::camera.Address = 0;
        global::light = {};
    }

    bool relaunch()
    {
        char path[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
            return false;

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);

        std::string cmd = "\"";
        cmd += path;
        cmd += "\"";

        const bool created = CreateProcessA(
            nullptr,
            cmd.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi) != FALSE;

        if (created)
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }

        return created;
    }

    void watch(const char* processName)
    {
        for (;;)
        {
            Sleep(1000);
            if (!process(processName))
            {
                clearstate();

                while (!process(processName))
                    Sleep(1000);

                relaunch();
                ExitProcess(0);
            }
        }
    }

}

std::int32_t main(std::int32_t argc, char** argv[])
{
    gate();

    static constexpr const char* BINARY_NAME = { "RobloxPlayerBeta.exe" };
    const bool alreadyRunning = process(BINARY_NAME);

    if (!alreadyRunning)
    {
        std::cout << "[unknownware] Waiting for Roblox to open..." << std::endl;
        while (!process(BINARY_NAME))
        {
            Sleep(500);
        }
    }

    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        ShowWindow(hwnd, SW_HIDE);
    }

    if (!alreadyRunning)
        Sleep(5000);

    std::cout << "[unknownware] External loaded" << std::endl;

    std::thread(watch, BINARY_NAME).detach();

    drive->process(BINARY_NAME);
    drive->attach(BINARY_NAME);
    drive->module(BINARY_NAME);

    auto fakemodel = drive->read<std::uint64_t>(drive->modulebase() + offset::fakemodel::Pointer);
    global::model.Address = drive->read<std::uint64_t>(fakemodel + offset::fakemodel::RealDataModel);
    global::render.Address = drive->read<std::uint64_t>(drive->modulebase() + offset::render::Pointer);
    global::actor.Address = global::model.childclass("Players").Address;
    global::workspace.Address = global::model.childclass("Workspace").Address;
    global::camera.Address = global::workspace.childclass("Camera").Address;
    auto Lightin = global::model.childclass("Lighting");
    global::light = sdk::light(Lightin.Address);

    std::thread(cache::run).detach();
    std::thread(aim::run).detach();

    auto workspacetoworld = drive->read<uintptr_t>(global::workspace.Address + offset::workspace::world);
    drive->write<float>(workspacetoworld + 0x658, 200 * 4.f);

    screen->window();
    screen->device();
    screen->imgui();

    for (;;)
    {
        screen->begin();
        screen->visual();
        screen->menu();
        screen->end();
    }

    return 0;
}
