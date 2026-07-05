#pragma once

#include <string>

namespace notify
{
    enum class kind
    {
        info,
        success,
        warning,
        damage
    };

    void push(kind type, std::string title, std::string body = {}, float lifetime = 3.2f);
    void render();
}
