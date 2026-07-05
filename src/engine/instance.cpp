#include "engine/engine.h"

namespace sdk {

    std::string instance::name() const {

        if (!Address) return "?";
        uintptr_t StringPointer = drive->read<uintptr_t>(Address + offset::instance::name);
        if (!StringPointer) return "?";
        return drive->readstring(StringPointer);
    }

    std::string instance::text() const {

        return drive->readstring(this->Address + offset::gui::text);
    }

    std::string instance::kind() const {

        if (!Address) return "?";
        uintptr_t Descriptor = drive->read<uintptr_t>(Address + offset::instance::ClassDescriptor);
        if (!Descriptor) return "?";
        uintptr_t StringPointer = drive->read<uintptr_t>(Descriptor + 0x8);
        if (!StringPointer) return "?";
        return drive->readstring(StringPointer);
    }

    instance instance::parent() const {

        if (!Address) return instance();
        return drive->read<instance>(Address + offset::instance::parent);
    }

    std::vector<instance> instance::children() const {

        std::vector<instance> Container;
        if (!Address) return Container;

        auto Start = drive->read<uintptr_t>(Address + offset::instance::ChildrenStart);
        if (!Start) return Container;

        auto end = drive->read<uintptr_t>(Start + offset::instance::ChildrenEnd);
        for (auto instances = drive->read<uintptr_t>(Start); instances != end; instances += 16)
            Container.emplace_back(drive->read<instance>(instances));

        return Container;
    }

    instance instance::child(const std::string& name) const {

        if (!Address || name.empty()) return instance();

        for (instance Child : children())
        {
            if (!Child.Address) continue;
            if (Child.name() == name) return Child;
        }

        return instance();
    }

    instance instance::childclass(const std::string& Class_Name) const {

        if (!Address || Class_Name.empty()) return instance();

        for (instance Child : children())
        {
            if (!Child.Address) continue;
            if (Child.kind() == Class_Name) return Child;
        }

        return instance();
    }
}
