#include "engine/engine.h"
#include "global.h"

namespace sdk {

    std::uint64_t view::baseview() {

        auto Rv1 = drive->read<std::uintptr_t>(global::model.Address + 0x1D0);
        auto Rv2 = drive->read<std::uintptr_t>(Rv1 + 0x8);
        auto Rv3 = drive->read<std::uintptr_t>(Rv2 + 0x28);
        return Rv3;
    }

    void view::invalidate() {

        drive->write<bool>(global::view.baseview() + 0x148, false);
    }

    void view::skybox() {

        drive->write<bool>(global::view.baseview() + 0x2cd, false);
    }
}
