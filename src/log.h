#include <windows.h>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>

namespace output
{
    inline bool Vt_Ready = false;

    inline void vt()
    {
        if (Vt_Ready)
            return;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode))
        {
            mode |= 0x0004;
            SetConsoleMode(h, mode);
        }
        Vt_Ready = true;
    }

    inline std::string time()
    {
        std::time_t t = std::time(nullptr);
        std::tm bt{};
        localtime_s(&bt, &t);
        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "[%02d:%02d:%02d]", bt.tm_hour, bt.tm_min, bt.tm_sec);
        return std::string(buf);
    }

    inline void prefix(const char* color, const char* tag)
    {
        const auto ts = time();
        std::printf("\033[38;2;86;101;114m%s\033[0m  ", ts.c_str());
        std::printf("\033[38;2;255;255;255munknownware\033[0m  ");
        std::printf("%s%-7s\033[0m  ", color, tag);
    }

    template<typename... A>
    inline void print(const char* color, const char* tag, const char* format, A&&... args)
    {
        vt();
        prefix(color, tag);
        std::printf(format, std::forward<A>(args)...);
        std::printf("\n");
    }

    template<typename... A>
    inline void info(const char* format, A&&... args)
    {
        print("\033[38;2;92;205;255m", "info", format, std::forward<A>(args)...);
    }

    template<typename... A>
    inline void warn(const char* format, A&&... args)
    {
        print("\033[38;2;255;208;106m", "warn", format, std::forward<A>(args)...);
    }

    template<typename... A>
    inline void error(const char* format, A&&... args)
    {
        print("\033[38;2;255;90;116m", "error", format, std::forward<A>(args)...);
    }

    template<typename... A>
    inline void ok(const char* format, A&&... args)
    {
        print("\033[38;2;88;244;178m", "ok", format, std::forward<A>(args)...);
    }

    template<typename... A>
    inline void core(const char* label, const char* format, A&&... args)
    {
        vt();
        prefix("\033[38;2;210;132;255m", "core");
        std::printf("\033[38;2;126;145;158m%-18s\033[0m", label);
        std::printf(format, std::forward<A>(args)...);
        std::printf("\n");
    }
}
