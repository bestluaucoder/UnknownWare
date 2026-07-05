#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <memory>

class driver final {
public:
    driver() = default;
    ~driver() { if (handle) CloseHandle(handle); }

    uint32_t pid(const std::string& name);
    uint64_t module(const std::string& name);
    bool attach(const std::string& name);

    template <typename T>
    T read(uint64_t addr);
    template <typename T>
    void write(uint64_t addr, T val);

    std::string readstring(uint64_t addr);
    void writestring(uint64_t addr, const std::string& val);
    uint32_t processid() const { return pid_; }
    uint64_t modulebase() const { return base_; }
    HANDLE processhandle() const { return handle; }

    void process(const std::string& name) { pid(name); }

private:
    uint32_t pid_ = 0;
    uint64_t base_ = 0;
    HANDLE handle = nullptr;
};

template <typename T>
T driver::read(uint64_t addr) {
    T buf{};
    ReadProcessMemory(handle, (LPCVOID)addr, &buf, sizeof(T), nullptr);
    return buf;
}

template <typename T>
void driver::write(uint64_t addr, T val) {
    WriteProcessMemory(handle, (LPVOID)addr, &val, sizeof(T), nullptr);
}

inline std::unique_ptr<driver> g_driver = std::make_unique<driver>();
inline driver* drive = g_driver.get();
