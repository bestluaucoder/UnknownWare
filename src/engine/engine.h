#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "driver/driver.h"
#include "offset.h"
#include "math.h"

namespace sdk {

    struct viewport {

        unsigned short x, y;
    };

    struct instance {
        uintptr_t Address = 0;

        instance() = default;
        explicit instance(uintptr_t addr) : Address(addr) {}

        std::string name() const;
        std::string text() const;
        std::string kind() const;

        instance parent() const;
        std::vector<instance> children() const;
        instance character() const;

        instance child(const std::string& name) const;
        instance childclass(const std::string& Class_Name) const;

        template <typename T>
        std::vector<T> childrenas() const
        {
            std::vector<T> list;
            if (!Address) return list;

            for (sdk::instance child : children())
            {
                if (!child.Address) continue;
                list.emplace_back(T(child.Address));
            }

            return list;
        }
    };

    struct model : public instance {

        using instance::instance;
        std::uint64_t place() const;
        std::uint64_t game() const;
        std::uint64_t creator() const;
        std::uint64_t server() const;
    };

    struct render : public instance {

        using instance::instance;
        sdk::vector2 size() const;
        sdk::matrix4 matrix() const;
        sdk::vector2 screen(const sdk::vector3& world) const;
    };

    struct actor : public instance {

        using instance::instance;
        std::uint64_t userid() const;
        std::uint64_t teamid() const;
        std::string display() const;
        actor local() const;
    };

    struct camera : public instance {

        using instance::instance;
        sdk::vector3 position() const;
        sdk::matrix3 rotation() const;
        void position(const sdk::vector3& pos) const;
        void rotation(const sdk::matrix3& rot) const;

        vector2 viewportat(vector2 target_screen_pos, vector2 screen_size);
        void viewport(sdk::viewport Vp) const;
    };

    struct humanoid : public instance {

        using instance::instance;
        float health() const;
        float maxhealth() const;
        void kill() const;
        int rig() const;
    };

    struct part : public instance {

        using instance::instance;
        sdk::vector3 position() const;
        sdk::matrix3 rotation() const;
        sdk::vector3 size() const;
        sdk::vector3 movedir() const;
        sdk::vector3 velocity() const;
        sdk::vector3 cframe() const;
        sdk::vector3 partposition() const;
        part primitive() const;
        float transparency() const;
        bool anchored() const;

        void velocity(const sdk::vector3& Velocity) const;
        void partposition(const sdk::vector3& Position) const;
        void rotation(const sdk::matrix3& Rotation) const;
        void mesh(const std::string& MeshID) const;
        void transparency(float Transparency) const;
        void movedir(const sdk::vector3& MoveDir) const;
    };

    struct view : public instance {

        using instance::instance;
        std::uint64_t baseview();

        static void invalidate();
        static void skybox();
    };

    struct light : public instance {

        using instance::instance;

        static void ambient(uintptr_t LightingAddress, sdk::vector3 tocolor);
        static void fog(uintptr_t LightingAddress, float end, sdk::vector3 tocolor);
        static void brightness(uintptr_t LightingAddress, float Value);
        static void exposure(uintptr_t LightingAddress, float Value);
        static void fov(uintptr_t CameraAddress, float Degrees);
    };
}
