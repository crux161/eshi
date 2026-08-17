/**
 * Loads against the release-mode shared core and verifies the C99 view of every
 * public layout against the descriptor compiled into that library.
 */
#include <eshi/eshi.h>

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct AlignAbiInfo { char byte; EshiAbiInfo value; };
struct AlignConfig { char byte; EshiConfig value; };
struct AlignTransformView { char byte; EshiTransformView value; };
struct AlignVelocityView { char byte; EshiVelocityView value; };
struct AlignCollisionEvent { char byte; EshiCollisionEvent value; };
struct AlignMaterial { char byte; EshiMaterial value; };

static int failures = 0;
static int checks = 0;

#define CHECK_EQ(actual, expected, label)                                      \
    do {                                                                        \
        const unsigned long long a_ = (unsigned long long)(actual);             \
        const unsigned long long e_ = (unsigned long long)(expected);           \
        ++checks;                                                               \
        if (a_ != e_) {                                                         \
            ++failures;                                                        \
            fprintf(stderr, "FAIL %s: native=%llu C99=%llu\n", label, a_, e_); \
        }                                                                       \
    } while (0)

#define CHECK_LAYOUT_FIELD(info, type, member, reported) \
    CHECK_EQ((info).reported, offsetof(type, member), #type "." #member " offset")

static void check_public_layouts(const EshiAbiInfo* info) {
    CHECK_EQ(info->struct_size, sizeof(EshiAbiInfo), "EshiAbiInfo size");
    CHECK_EQ(info->abi_info_alignment, offsetof(struct AlignAbiInfo, value),
             "EshiAbiInfo alignment");
    CHECK_EQ(info->pointer_size, sizeof(void*), "data pointer size");
    CHECK_EQ(info->size_t_size, sizeof(size_t), "size_t size");
    CHECK_EQ(info->function_pointer_size, sizeof(EshiShaderFn), "function pointer size");
    CHECK_EQ(info->grade_size, sizeof(EshiGrade), "EshiGrade size");
    CHECK_EQ(info->result_size, sizeof(EshiResult), "EshiResult size");
    CHECK_EQ(info->entity_size, sizeof(EshiEntity), "EshiEntity size");

    CHECK_EQ(info->config_size, sizeof(EshiConfig), "EshiConfig size");
    CHECK_EQ(info->config_alignment, offsetof(struct AlignConfig, value),
             "EshiConfig alignment");
    CHECK_LAYOUT_FIELD((*info), EshiConfig, width, config_width_offset);
    CHECK_LAYOUT_FIELD((*info), EshiConfig, height, config_height_offset);
    CHECK_LAYOUT_FIELD((*info), EshiConfig, grade, config_grade_offset);
    CHECK_LAYOUT_FIELD((*info), EshiConfig, fixed_dt, config_fixed_dt_offset);
    CHECK_LAYOUT_FIELD((*info), EshiConfig, seed, config_seed_offset);
    CHECK_LAYOUT_FIELD((*info), EshiConfig, max_entities, config_max_entities_offset);

    CHECK_EQ(info->transform_view_size, sizeof(EshiTransformView),
             "EshiTransformView size");
    CHECK_EQ(info->transform_view_alignment, offsetof(struct AlignTransformView, value),
             "EshiTransformView alignment");
    CHECK_LAYOUT_FIELD((*info), EshiTransformView, entity, transform_view_entity_offset);
    CHECK_LAYOUT_FIELD((*info), EshiTransformView, x, transform_view_x_offset);
    CHECK_LAYOUT_FIELD((*info), EshiTransformView, y, transform_view_y_offset);
    CHECK_LAYOUT_FIELD((*info), EshiTransformView, count, transform_view_count_offset);

    CHECK_EQ(info->velocity_view_size, sizeof(EshiVelocityView), "EshiVelocityView size");
    CHECK_EQ(info->velocity_view_alignment, offsetof(struct AlignVelocityView, value),
             "EshiVelocityView alignment");
    CHECK_LAYOUT_FIELD((*info), EshiVelocityView, entity, velocity_view_entity_offset);
    CHECK_LAYOUT_FIELD((*info), EshiVelocityView, vx, velocity_view_vx_offset);
    CHECK_LAYOUT_FIELD((*info), EshiVelocityView, vy, velocity_view_vy_offset);
    CHECK_LAYOUT_FIELD((*info), EshiVelocityView, count, velocity_view_count_offset);

    CHECK_EQ(info->collision_event_size, sizeof(EshiCollisionEvent),
             "EshiCollisionEvent size");
    CHECK_EQ(info->collision_event_alignment, offsetof(struct AlignCollisionEvent, value),
             "EshiCollisionEvent alignment");
    CHECK_LAYOUT_FIELD((*info), EshiCollisionEvent, a, collision_event_a_offset);
    CHECK_LAYOUT_FIELD((*info), EshiCollisionEvent, b, collision_event_b_offset);
    CHECK_LAYOUT_FIELD((*info), EshiCollisionEvent, nx, collision_event_nx_offset);
    CHECK_LAYOUT_FIELD((*info), EshiCollisionEvent, ny, collision_event_ny_offset);
    CHECK_LAYOUT_FIELD((*info), EshiCollisionEvent, penetration,
                       collision_event_penetration_offset);

    CHECK_EQ(info->material_size, sizeof(EshiMaterial), "EshiMaterial size");
    CHECK_EQ(info->material_alignment, offsetof(struct AlignMaterial, value),
             "EshiMaterial alignment");
    CHECK_LAYOUT_FIELD((*info), EshiMaterial, cpu_shader, material_cpu_shader_offset);
    CHECK_LAYOUT_FIELD((*info), EshiMaterial, source_path, material_source_path_offset);
    CHECK_LAYOUT_FIELD((*info), EshiMaterial, package_path, material_package_path_offset);
    CHECK_LAYOUT_FIELD((*info), EshiMaterial, uniform_data, material_uniform_data_offset);
    CHECK_LAYOUT_FIELD((*info), EshiMaterial, uniform_size, material_uniform_size_offset);
}

int main(void) {
    EshiAbiInfo info;
    uint32_t short_query[2];
    int i;

    memset(&info, 0, sizeof info);
    CHECK_EQ(CHAR_BIT, 8, "eight-bit bytes");
    CHECK_EQ(sizeof(float), 4, "32-bit float");
    CHECK_EQ(FLT_RADIX, 2, "binary float radix");
    CHECK_EQ(FLT_MANT_DIG, 24, "IEEE-754 binary32 mantissa");
    CHECK_EQ(sizeof(EshiGrade), 4, "fixed-width public enums");
    CHECK_EQ(sizeof(EshiResult), 4, "fixed-width result enum");

    CHECK_EQ(eshi_abi_check(ESHI_ABI_VERSION,
                            ESHI_COMMAND_PROTOCOL_VERSION,
                            ESHI_EVENT_PROTOCOL_VERSION),
             ESHI_OK, "current contract accepted");
    CHECK_EQ(eshi_abi_check(ESHI_ABI_VERSION_PACK(2, 0, 0),
                            ESHI_COMMAND_PROTOCOL_VERSION,
                            ESHI_EVENT_PROTOCOL_VERSION),
             ESHI_ERR_VERSION, "different ABI major rejected");
    CHECK_EQ(eshi_abi_check(ESHI_ABI_VERSION,
                            ESHI_COMMAND_PROTOCOL_VERSION + 1u,
                            ESHI_EVENT_PROTOCOL_VERSION),
             ESHI_ERR_VERSION, "different command protocol rejected");
    CHECK_EQ(eshi_abi_check(ESHI_ABI_VERSION,
                            ESHI_COMMAND_PROTOCOL_VERSION,
                            ESHI_EVENT_PROTOCOL_VERSION + 1u),
             ESHI_ERR_VERSION, "different event protocol rejected");

    CHECK_EQ(eshi_abi_query(&info, (uint32_t)sizeof info), ESHI_OK,
             "full ABI query succeeds");
    CHECK_EQ(info.abi_version, ESHI_ABI_VERSION, "ABI version matches header");
    CHECK_EQ(info.command_protocol_version, ESHI_COMMAND_PROTOCOL_VERSION,
             "command protocol matches header");
    CHECK_EQ(info.event_protocol_version, ESHI_EVENT_PROTOCOL_VERSION,
             "event protocol matches header");
    check_public_layouts(&info);

    short_query[0] = 0;
    short_query[1] = 0xC0DEF00Du;
    CHECK_EQ(eshi_abi_query((EshiAbiInfo*)short_query, sizeof(uint32_t)),
             ESHI_ERR_LIMIT, "short ABI query reports required capacity");
    CHECK_EQ(short_query[0], sizeof(EshiAbiInfo), "short ABI query returns native size");
    CHECK_EQ(short_query[1], 0xC0DEF00Du, "short ABI query does not overrun caller memory");

    /* Lifecycle symmetry is part of the boundary contract, including NULL. */
    eshi_world_destroy(NULL);
    for (i = 0; i < 128; ++i) {
        EshiWorld* world = eshi_world_create(NULL);
        ++checks;
        if (world == NULL) {
            ++failures;
            fprintf(stderr, "FAIL lifecycle create at iteration %d\n", i);
            break;
        }
        eshi_world_destroy(world);
    }

    printf("Larimar release ABI: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
