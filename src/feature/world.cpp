#include "engine/engine.h"
#include "global.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <thread>

namespace world {

    namespace
    {
        using skybox_faces = std::array<const char*, 6>;

        constexpr std::array<skybox_faces, 11> kSkyboxes{ {
            { "rbxassetid://12635309703", "rbxassetid://12635311686", "rbxassetid://12635312870", "rbxassetid://12635313718", "rbxassetid://12635315817", "rbxassetid://12635316856" },
            { "rbxassetid://12064107", "rbxassetid://12064152", "rbxassetid://12064121", "rbxassetid://12063984", "rbxassetid://12064115", "rbxassetid://12064131" },
            { "rbxassetid://271042516", "rbxassetid://271077243", "rbxassetid://271042556", "rbxassetid://271042310", "rbxassetid://271042467", "rbxassetid://271077958" },
            { "rbxassetid://1876545003", "rbxassetid://1876544331", "rbxassetid://1876542941", "rbxassetid://1876543392", "rbxassetid://1876543764", "rbxassetid://1876544642" },
            { "rbxassetid://116758234", "rbxassetid://116758314", "rbxassetid://116758367", "rbxassetid://116758446", "rbxassetid://116758478", "rbxassetid://116758496" },
            { "rbxassetid://1233158420", "rbxassetid://1233158838", "rbxassetid://1233157105", "rbxassetid://1233157640", "rbxassetid://1233157995", "rbxassetid://1233159158" },
            { "rbxassetid://1327358", "rbxassetid://1327359", "rbxassetid://1327355", "rbxassetid://1327357", "rbxassetid://1327356", "rbxassetid://1327360" },
            { "rbxassetid://570555736", "rbxassetid://570555964", "rbxassetid://570555800", "rbxassetid://570555840", "rbxassetid://570555882", "rbxassetid://570555929" },
            { "rbxassetid://95020137072033", "rbxassetid://92862258103959", "rbxassetid://107665368823185", "rbxassetid://126542804346203", "rbxassetid://103716549795832", "rbxassetid://131036626982613" },
            { "rbxassetid://169210090", "rbxassetid://169210108", "rbxassetid://169210121", "rbxassetid://169210133", "rbxassetid://169210143", "rbxassetid://169210149" },
            { "rbxassetid://47974894", "rbxassetid://47974690", "rbxassetid://47974821", "rbxassetid://47974776", "rbxassetid://47974859", "rbxassetid://47974909" }
        } };

        struct applied_state
        {
            bool Skybox = false;
            int SkyboxType = -1;
            float Ambience[3]{ -1.f, -1.f, -1.f };
            float Fog[4]{ -1.f, -1.f, -1.f, -1.f };
            float Brightness = -1.f;
            float Exposure = -1000.f;
            float Fov = -1.f;
        };

        bool same3(const float lhs[3], const float rhs[4])
        {
            return std::fabs(lhs[0] - rhs[0]) < .001f &&
                std::fabs(lhs[1] - rhs[1]) < .001f &&
                std::fabs(lhs[2] - rhs[2]) < .001f;
        }

        void copy3(float dst[3], const float src[4])
        {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
        }

        bool samefog(const float lhs[4], float distance, const float color[4])
        {
            return std::fabs(lhs[0] - distance) < .01f &&
                std::fabs(lhs[1] - color[0]) < .001f &&
                std::fabs(lhs[2] - color[1]) < .001f &&
                std::fabs(lhs[3] - color[2]) < .001f;
        }

        void copyfog(float dst[4], float distance, const float color[4])
        {
            dst[0] = distance;
            dst[1] = color[0];
            dst[2] = color[1];
            dst[3] = color[2];
        }

        void applyskybox(applied_state& state, bool force)
        {
            if (!global::world::Skybox || !global::light.Address)
            {
                state.Skybox = false;
                return;
            }

            const int type = std::clamp(global::world::Skybox_Type, 0, (int)kSkyboxes.size() - 1);
            if (!force && state.Skybox && state.SkyboxType == type)
                return;

            const auto sky = global::light.child("Sky");
            if (!sky.Address)
                return;

            const skybox_faces& faces = kSkyboxes[(std::size_t)type];
            drive->writestring(sky.Address + offset::sky::SkyboxBk, faces[0]);
            drive->writestring(sky.Address + offset::sky::SkyboxDn, faces[1]);
            drive->writestring(sky.Address + offset::sky::SkyboxFt, faces[2]);
            drive->writestring(sky.Address + offset::sky::SkyboxLf, faces[3]);
            drive->writestring(sky.Address + offset::sky::SkyboxRt, faces[4]);
            drive->writestring(sky.Address + offset::sky::SkyboxUp, faces[5]);
            sdk::view::invalidate();
            sdk::view::skybox();

            state.Skybox = true;
            state.SkyboxType = type;
        }

        void rotateskybox()
        {
            if (!global::world::Skybox || !global::world::Rotate || !global::light.Address)
                return;

            const auto sky = global::light.childclass("Sky");
            if (!sky.Address)
                return;

            static float rotY = 0.0f;
            rotY += std::clamp(global::world::Skybox_Rotate_Speed, 0.0f, 20.0f);
            if (rotY >= 360.0f)
                rotY = std::fmod(rotY, 360.0f);

            drive->write<sdk::vector3>(sky.Address + offset::sky::SkyboxOrientation, { 0.0f, rotY, 0.0f });
        }

        void applylighting(applied_state& state, bool force)
        {
            if (!global::light.Address)
                return;

            if (global::world::Ambience && (force || !same3(state.Ambience, global::world::color::Ambience)))
            {
                sdk::light::ambient(global::light.Address,
                    { global::world::color::Ambience[0], global::world::color::Ambience[1], global::world::color::Ambience[2] });
                copy3(state.Ambience, global::world::color::Ambience);
                sdk::view::invalidate();
            }

            if (global::world::Fog && (force || !samefog(state.Fog, global::world::Fog_Distance, global::world::color::Fog)))
            {
                sdk::light::fog(global::light.Address,
                    global::world::Fog_Distance,
                    { global::world::color::Fog[0], global::world::color::Fog[1], global::world::color::Fog[2] });
                copyfog(state.Fog, global::world::Fog_Distance, global::world::color::Fog);
                sdk::view::invalidate();
            }

            if (global::world::Brightness && (force || std::fabs(state.Brightness - global::world::BrightnessI) > .001f))
            {
                sdk::light::brightness(global::light.Address, global::world::BrightnessI);
                state.Brightness = global::world::BrightnessI;
                sdk::view::invalidate();
            }

            if (global::world::Exposure && (force || std::fabs(state.Exposure - global::world::ExposureI) > .001f))
            {
                sdk::light::exposure(global::light.Address, global::world::ExposureI);
                state.Exposure = global::world::ExposureI;
                sdk::view::invalidate();
            }

            if (global::world::FOV && global::camera.Address &&
                (force || std::fabs(state.Fov - global::world::FOV_Distance) > .01f))
            {
                sdk::light::fov(global::camera.Address, global::world::FOV_Distance);
                state.Fov = global::world::FOV_Distance;
            }
        }
    }

    void run() {
        applied_state state{};
        auto lastSkyboxRefresh = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        auto lastLightingRefresh = lastSkyboxRefresh;

        for (;;)
        {
            const auto now = std::chrono::steady_clock::now();
            const bool forceSkybox = now - lastSkyboxRefresh >= std::chrono::milliseconds(750);
            const bool forceLighting = now - lastLightingRefresh >= std::chrono::milliseconds(350);

            applyskybox(state, forceSkybox);
            rotateskybox();
            applylighting(state, forceLighting);

            if (forceSkybox)
                lastSkyboxRefresh = now;
            if (forceLighting)
                lastLightingRefresh = now;

            const bool active =
                global::world::Skybox || global::world::Ambience || global::world::Fog ||
                global::world::Brightness || global::world::Exposure || global::world::FOV;

            std::this_thread::sleep_for(active
                ? std::chrono::milliseconds(33)
                : std::chrono::milliseconds(250));
        }
    }
}
