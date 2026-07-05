#include "engine/engine.h"
#include "global.h"

namespace sdk {

    void light::ambient(uintptr_t LightingAddress, sdk::vector3 tocolor)
    {
        drive->write<sdk::vector3>(LightingAddress + offset::light::Ambient, tocolor);
        drive->write<sdk::vector3>(LightingAddress + offset::light::OutdoorAmbient, tocolor);
    }

    void light::fog(uintptr_t LightingAddress, float end, sdk::vector3 tocolor)
    {
        drive->write<float>(LightingAddress + offset::light::FogEnd, end);
        drive->write<sdk::vector3>(LightingAddress + offset::light::FogColor, tocolor);
    }

    void light::brightness(uintptr_t LightingAddress, float Value)
    {
        drive->write<float>(LightingAddress + offset::light::Brightness, Value);
    }

    void light::exposure(uintptr_t LightingAddress, float Value)
    {
        drive->write<float>(LightingAddress + offset::light::ExposureCompensation, Value);
    }

    void light::fov(uintptr_t CameraAddress, float Degrees)
    {
        drive->write<float>(CameraAddress + offset::camera::FieldOfView, Degrees * 0.0174533f);
    }
}
