#include "feature/wallcheck.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>

#include "global.h"

namespace wallcheck
{
    struct PartEntry {
        std::uintptr_t address;
    };

    static std::vector<PartEntry> part_cache;
    static std::mutex cache_mutex;

    bool valid(const sdk::vector3& v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    static bool is_world_part(const std::string& kind)
    {
        return kind == "Part" || kind == "MeshPart" || kind == "WedgePart" ||
            kind == "CornerWedgePart" || kind == "TrussPart" || kind == "UnionOperation" ||
            kind == "PartOperation" || kind == "NegateOperation";
    }

    static void get_parts(const sdk::instance& parent, std::vector<PartEntry>& parts, int depth = 0)
    {
        if (!parent.Address || depth > 64)
            return;

        for (const sdk::instance& child : parent.children())
        {
            if (!child.Address)
                continue;

            const std::string kind = child.kind();
            if (is_world_part(kind))
            {
                bool is_char = false;
                sdk::instance cur = child.parent();
                for (int i = 0; i < 32 && cur.Address; i++) {
                    if (cur.kind() == "Model" && cur.childclass("Humanoid").Address) {
                        is_char = true;
                        break;
                    }
                    cur = cur.parent();
                }

                if (!is_char)
                {
                    sdk::part part(child.Address);
                    if (part.transparency() <= 0.95f)
                        parts.push_back({ child.Address });
                }
            }

            get_parts(child, parts, depth + 1);
        }
    }

    void update_cache()
    {
        if (!global::workspace.Address)
            return;

        std::vector<PartEntry> next;
        next.reserve(2048);
        get_parts(global::workspace, next);

        std::lock_guard<std::mutex> lock(cache_mutex);
        part_cache = std::move(next);
    }

    static bool intersects_obb(
        const sdk::vector3& ray_origin,
        const sdk::vector3& ray_dir,
        const sdk::vector3& part_pos,
        const sdk::matrix3& part_rot,
        const sdk::vector3& part_size,
        float& distance)
    {
        sdk::vector3 half = part_size * 0.5f;
        half.x = std::abs(half.x);
        half.y = std::abs(half.y);
        half.z = std::abs(half.z);
        if (half.x < 1e-6f && half.y < 1e-6f && half.z < 1e-6f)
            return false;

        const sdk::vector3 relative = ray_origin - part_pos;

        const sdk::vector3 local_origin{
            relative.x * part_rot.data[0] + relative.y * part_rot.data[3] + relative.z * part_rot.data[6],
            relative.x * part_rot.data[1] + relative.y * part_rot.data[4] + relative.z * part_rot.data[7],
            relative.x * part_rot.data[2] + relative.y * part_rot.data[5] + relative.z * part_rot.data[8]
        };

        const sdk::vector3 local_dir{
            ray_dir.x * part_rot.data[0] + ray_dir.y * part_rot.data[3] + ray_dir.z * part_rot.data[6],
            ray_dir.x * part_rot.data[1] + ray_dir.y * part_rot.data[4] + ray_dir.z * part_rot.data[7],
            ray_dir.x * part_rot.data[2] + ray_dir.y * part_rot.data[5] + ray_dir.z * part_rot.data[8]
        };

        float t_min = -1e30f;
        float t_max = 1e30f;

        auto slab = [&](float origin, float dir, float half_size) -> bool
        {
            if (half_size < 1e-6f)
                return std::abs(origin) < 1e-4f;

            if (std::abs(dir) > 1e-8f)
            {
                const float inv_dir = 1.f / dir;
                float t1 = (-half_size - origin) * inv_dir;
                float t2 = (half_size - origin) * inv_dir;
                if (t1 > t2) std::swap(t1, t2);
                t_min = (std::max)(t_min, t1);
                t_max = (std::min)(t_max, t2);
                return t_min <= t_max;
            }

            return std::abs(origin) <= half_size + 1e-4f;
        };

        if (!slab(local_origin.x, local_dir.x, half.x)) return false;
        if (!slab(local_origin.y, local_dir.y, half.y)) return false;
        if (!slab(local_origin.z, local_dir.z, half.z)) return false;
        if (t_max < 0.f || t_min > t_max) return false;

        distance = (std::max)(t_min, 0.f);
        return true;
    }

    bool can_see(const sdk::vector3& origin, const sdk::vector3& target, std::uintptr_t skip_char)
    {
        if (!valid(origin) || !valid(target))
            return false;

        const sdk::vector3 delta = target - origin;
        const float target_dist = delta.magnitude();
        if (target_dist < 0.1f)
            return true;

        const sdk::vector3 dir = delta / target_dist;

        std::vector<PartEntry> snapshot;
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            snapshot = part_cache;
        }

        const std::uintptr_t local_char = global::LocalPlayer.character.Address;

        for (const auto& entry : snapshot)
        {
            if (!entry.address)
                continue;

            sdk::part part(entry.address);
            sdk::part primitive = part.primitive();
            if (!primitive.Address)
                continue;

            const sdk::vector3 pos = primitive.position();
            const sdk::vector3 size = primitive.size();
            const sdk::matrix3 rot = primitive.rotation();
            if (!valid(pos) || !valid(size))
                continue;

            float hit_dist = 0.f;
            if (intersects_obb(origin, dir, pos, rot, size, hit_dist) &&
                hit_dist > 0.01f && hit_dist < target_dist - 0.5f)
                return false;
        }

        return true;
    }
}
