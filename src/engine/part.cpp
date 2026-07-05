#include "engine/engine.h"

namespace sdk {

    sdk::vector3 part::position() const {

        return drive->read<sdk::vector3>(Address + offset::primitive::Position);
    }

    sdk::matrix3 part::rotation() const {

        return drive->read<sdk::matrix3>(Address + offset::primitive::Rotation);
    }

    sdk::vector3 part::size() const {

        return drive->read<sdk::vector3>(Address + offset::primitive::Size);
    }

    sdk::vector3 part::movedir() const {

        return drive->read<sdk::vector3>(Address + offset::player::CameraMode);
    }

    sdk::vector3 part::velocity() const {

        if (!Address) return {};
        uintptr_t primitive = drive->read<uintptr_t>(Address + offset::basepart::primitive);
        if (!primitive) return {};
        return drive->read<sdk::vector3>(primitive + offset::primitive::AssemblyAngularVelocity);
    }

    bool part::anchored() const {

        return drive->read<bool>(Address + offset::primitiveflag::Anchored);
    }

    sdk::vector3 part::cframe() const {

        return drive->read<sdk::vector3>(Address + offset::primitive::Rotation);
    }

    part part::primitive() const {

        uintptr_t primitiveAddress = drive->read<uintptr_t>(Address + offset::basepart::primitive);
        return part{ primitiveAddress };
    }

    sdk::vector3 part::partposition() const {

        auto item = this->primitive();
        if (!item.Address) return {};
        return drive->read<sdk::vector3>(item.Address + offset::primitive::Position);
    }

    float part::transparency() const {

        return drive->read<float>(Address + offset::basepart::Transparency);
    }

    void part::velocity(const sdk::vector3& Velocity) const {

        auto item = this->primitive();
        if (!item.Address) return;
        drive->write<sdk::vector3>(item.Address + offset::primitive::AssemblyLinearVelocity, Velocity);
    }

    void part::partposition(const sdk::vector3& Position) const {

        auto item = this->primitive();
        if (!item.Address) return;
        drive->write<sdk::vector3>(item.Address + offset::primitive::Position, Position);
    }

    void part::rotation(const sdk::matrix3& Rotation) const {

        drive->write<sdk::matrix3>(Address + offset::primitive::Rotation, Rotation);
    }

    void part::mesh(const std::string& MeshID) const {

        drive->writestring(Address + offset::meshpart::MeshId, MeshID);
    }

    void part::transparency(float Transparency) const {

        drive->write<float>(Address + offset::basepart::Transparency, Transparency);
    }

    void part::movedir(const sdk::vector3& MoveDir) const {

        drive->write<sdk::vector3>(Address + offset::player::CameraMode, MoveDir);
    }
}
