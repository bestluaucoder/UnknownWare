#include "driver.h"

uint32_t driver::pid(const std::string& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);
    uint32_t found = 0;

    if (Process32First(snap, &pe)) {
        do {
            if (!_stricmp(name.c_str(), pe.szExeFile)) {
                found = pe.th32ProcessID;
                pid_ = found;
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

uint64_t driver::module(const std::string& name) {
    if (!handle) return 0;

    DWORD pid = GetProcessId(handle);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32 me{};
    me.dwSize = sizeof(me);
    uint64_t found = 0;

    if (Module32First(snap, &me)) {
        do {
            if (!_stricmp(name.c_str(), me.szModule)) {
                found = (uint64_t)me.modBaseAddr;
                base_ = found;
                break;
            }
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

bool driver::attach(const std::string& name) {
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid(name));
    if (!h || h == INVALID_HANDLE_VALUE) return false;
    handle = h;
    return true;
}

std::string driver::readstring(uint64_t addr) {
    int32_t len = read<int32_t>(addr + 0x10);
    uint64_t str_addr = (len >= 16) ? read<uint64_t>(addr) : addr;

    if (len <= 0 || len > 255) return "Unknown";

    std::vector<char> buf(len + 1, 0);
    ReadProcessMemory(handle, (LPCVOID)str_addr, buf.data(), (SIZE_T)len, nullptr);
    return std::string(buf.data(), (size_t)len);
}

void driver::writestring(uint64_t addr, const std::string& val) {
    int32_t len = static_cast<int32_t>(val.size());
    int32_t maxlen = read<int32_t>(addr + 0x10);
    uint64_t str_addr = (maxlen >= 16) ? read<uint64_t>(addr) : addr;
    WriteProcessMemory(handle, (LPVOID)str_addr, val.data(), (SIZE_T)len, nullptr);
    if (maxlen < 16)
        WriteProcessMemory(handle, (LPVOID)(addr + 0x10), &len, sizeof(len), nullptr);
}
