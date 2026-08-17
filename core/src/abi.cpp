/**
 * @file abi.cpp
 * @brief Runtime description of Larimar's public C layout and wire versions.
 */
#include "eshi/eshi.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace {

EshiAbiInfo current_abi(void) {
    EshiAbiInfo info;
    std::memset(&info, 0, sizeof info);

    info.struct_size = (uint32_t)sizeof(EshiAbiInfo);
    info.abi_info_alignment = (uint32_t)alignof(EshiAbiInfo);
    info.abi_version = ESHI_ABI_VERSION;
    info.command_protocol_version = ESHI_COMMAND_PROTOCOL_VERSION;
    info.event_protocol_version = ESHI_EVENT_PROTOCOL_VERSION;

    info.pointer_size = (uint32_t)sizeof(void*);
    info.size_t_size = (uint32_t)sizeof(size_t);
    info.function_pointer_size = (uint32_t)sizeof(EshiShaderFn);
    info.grade_size = (uint32_t)sizeof(EshiGrade);
    info.result_size = (uint32_t)sizeof(EshiResult);
    info.entity_size = (uint32_t)sizeof(EshiEntity);

    info.config_size = (uint32_t)sizeof(EshiConfig);
    info.config_alignment = (uint32_t)alignof(EshiConfig);
    info.config_width_offset = (uint32_t)offsetof(EshiConfig, width);
    info.config_height_offset = (uint32_t)offsetof(EshiConfig, height);
    info.config_grade_offset = (uint32_t)offsetof(EshiConfig, grade);
    info.config_fixed_dt_offset = (uint32_t)offsetof(EshiConfig, fixed_dt);
    info.config_seed_offset = (uint32_t)offsetof(EshiConfig, seed);
    info.config_max_entities_offset = (uint32_t)offsetof(EshiConfig, max_entities);

    info.transform_view_size = (uint32_t)sizeof(EshiTransformView);
    info.transform_view_alignment = (uint32_t)alignof(EshiTransformView);
    info.transform_view_entity_offset = (uint32_t)offsetof(EshiTransformView, entity);
    info.transform_view_x_offset = (uint32_t)offsetof(EshiTransformView, x);
    info.transform_view_y_offset = (uint32_t)offsetof(EshiTransformView, y);
    info.transform_view_count_offset = (uint32_t)offsetof(EshiTransformView, count);

    info.velocity_view_size = (uint32_t)sizeof(EshiVelocityView);
    info.velocity_view_alignment = (uint32_t)alignof(EshiVelocityView);
    info.velocity_view_entity_offset = (uint32_t)offsetof(EshiVelocityView, entity);
    info.velocity_view_vx_offset = (uint32_t)offsetof(EshiVelocityView, vx);
    info.velocity_view_vy_offset = (uint32_t)offsetof(EshiVelocityView, vy);
    info.velocity_view_count_offset = (uint32_t)offsetof(EshiVelocityView, count);

    info.collision_event_size = (uint32_t)sizeof(EshiCollisionEvent);
    info.collision_event_alignment = (uint32_t)alignof(EshiCollisionEvent);
    info.collision_event_a_offset = (uint32_t)offsetof(EshiCollisionEvent, a);
    info.collision_event_b_offset = (uint32_t)offsetof(EshiCollisionEvent, b);
    info.collision_event_nx_offset = (uint32_t)offsetof(EshiCollisionEvent, nx);
    info.collision_event_ny_offset = (uint32_t)offsetof(EshiCollisionEvent, ny);
    info.collision_event_penetration_offset =
        (uint32_t)offsetof(EshiCollisionEvent, penetration);

    info.material_size = (uint32_t)sizeof(EshiMaterial);
    info.material_alignment = (uint32_t)alignof(EshiMaterial);
    info.material_cpu_shader_offset = (uint32_t)offsetof(EshiMaterial, cpu_shader);
    info.material_source_path_offset = (uint32_t)offsetof(EshiMaterial, source_path);
    info.material_package_path_offset = (uint32_t)offsetof(EshiMaterial, package_path);
    info.material_uniform_data_offset = (uint32_t)offsetof(EshiMaterial, uniform_data);
    info.material_uniform_size_offset = (uint32_t)offsetof(EshiMaterial, uniform_size);
    return info;
}

} /* namespace */

extern "C" EshiResult eshi_abi_check(uint32_t requested_abi_version,
                                      uint32_t requested_command_protocol,
                                      uint32_t requested_event_protocol) {
    const uint32_t requested_major = ESHI_ABI_VERSION_MAJOR_OF(requested_abi_version);
    const uint32_t requested_minor = ESHI_ABI_VERSION_MINOR_OF(requested_abi_version);
    const uint32_t current_major = ESHI_ABI_VERSION_MAJOR_OF(ESHI_ABI_VERSION);
    const uint32_t current_minor = ESHI_ABI_VERSION_MINOR_OF(ESHI_ABI_VERSION);

    if (requested_major != current_major || requested_minor > current_minor ||
        requested_command_protocol != ESHI_COMMAND_PROTOCOL_VERSION ||
        requested_event_protocol != ESHI_EVENT_PROTOCOL_VERSION) {
        return ESHI_ERR_VERSION;
    }
    return ESHI_OK;
}

extern "C" EshiResult eshi_abi_query(EshiAbiInfo* out_info, uint32_t out_size) {
    if (!out_info || out_size < sizeof(uint32_t)) return ESHI_ERR_INVALID;

    const EshiAbiInfo info = current_abi();
    const size_t copy_size = std::min((size_t)out_size, sizeof info);
    std::memcpy(out_info, &info, copy_size);
    return out_size < sizeof info ? ESHI_ERR_LIMIT : ESHI_OK;
}
