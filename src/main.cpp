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
#include "feature/misc.h"
#include "feature/silent.h"

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
        MessageBoxA(nullptr,
            "made by @n1kvz credits to je.rk for motivation",
            "UW External",
            MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
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
    std::thread(misc::run).detach();
    std::thread(silent::run).detach();

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
