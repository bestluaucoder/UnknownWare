#pragma once
#include <memory>
#include <cstdint>
#include <vector>
#include "engine/engine.h"
#include "feature/cache.h"

namespace silent
{
    void frame();
    void mouse();
    void magic_bullet();
    void run();
}

inline std::unique_ptr<sdk::instance> mouseservice{};

inline bool IsSilentReady{ false };
inline bool SilentTarget{ false };
inline bool TargetNeedsReset{ false };
inline sdk::vector2 SilentPartPos{};
inline std::uint64_t SilentCachedPositionX{ 0 };
inline std::uint64_t SilentCachedPositionY{ 0 };
inline sdk::instance SilentAimInstance{};
inline sdk::player SilentCachedTarget{};
inline sdk::player SilentCachedLastTarget{};
inline bool IsSilentEnabled{ true };
inline bool SilentStickyAim{ false };
inline int SilentAimKeybind{ 0 };
inline bool SilentAimLocked{ false };
inline bool SilentAimKeyWasPressed{ false };
inline bool SilentFTarget{ false };
inline bool SilentPrimitiveActive{ false };
inline bool SilentHasOriginalSizes{ false };
inline sdk::vector2 SilentOriginalSize{};
inline std::vector<std::pair<std::uint64_t, sdk::vector2>> SilentOriginalChildrenSizes{};

struct helper final
{
    std::uint64_t Address = 0;
    static std::uint64_t CachedInputObject;

    helper() = default;
    helper(std::uint64_t addr) : Address(addr) {}

    void x(std::uint64_t position);
    void y(std::uint64_t position);
    void init(std::uint64_t address);
    void writepos(std::uint64_t address, float x, float y);
};
