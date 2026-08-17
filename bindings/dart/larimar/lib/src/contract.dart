part of '../larimar.dart';

/// The native ABI and packed-protocol versions loaded by this process.
final class LarimarContract {
  LarimarContract._({
    required this.abiVersion,
    required this.commandProtocolVersion,
    required this.eventProtocolVersion,
  });

  static LarimarContract? _cached;

  /// Checks compatibility before any native world or shared buffer is used.
  static LarimarContract get current => _cached ??= _load();

  final int abiVersion;
  final int commandProtocolVersion;
  final int eventProtocolVersion;

  static LarimarContract _load() {
    final compatibility = native.eshi_abi_check(
      native.ESHI_ABI_VERSION,
      native.ESHI_COMMAND_PROTOCOL_VERSION,
      native.ESHI_EVENT_PROTOCOL_VERSION,
    );
    if (compatibility.value != LarimarResult.ok.code) {
      throw LarimarVersionException(
        'Loaded Larimar core rejected ABI ${native.ESHI_ABI_VERSION}, '
        'command protocol ${native.ESHI_COMMAND_PROTOCOL_VERSION}, and '
        'event protocol ${native.ESHI_EVENT_PROTOCOL_VERSION}.',
      );
    }

    return using((arena) {
      final pointer = arena<native.EshiAbiInfo>();
      _checkNative(
        native.eshi_abi_query(pointer, ffi.sizeOf<native.EshiAbiInfo>()),
        'eshi_abi_query',
      );
      final info = pointer.ref;

      void expectLayout(String label, int reported, int generated) {
        if (reported != generated) {
          throw LarimarVersionException(
            '$label layout mismatch: native=$reported, Dart=$generated.',
          );
        }
      }

      expectLayout(
        'EshiAbiInfo size',
        info.struct_size,
        ffi.sizeOf<native.EshiAbiInfo>(),
      );
      expectLayout(
        'EshiConfig size',
        info.config_size,
        ffi.sizeOf<native.EshiConfig>(),
      );
      expectLayout(
        'EshiTransformView size',
        info.transform_view_size,
        ffi.sizeOf<native.EshiTransformView>(),
      );
      expectLayout(
        'EshiVelocityView size',
        info.velocity_view_size,
        ffi.sizeOf<native.EshiVelocityView>(),
      );
      expectLayout(
        'EshiCollisionEvent size',
        info.collision_event_size,
        ffi.sizeOf<native.EshiCollisionEvent>(),
      );
      expectLayout(
        'EshiMaterial size',
        info.material_size,
        ffi.sizeOf<native.EshiMaterial>(),
      );
      expectLayout(
        'pointer size',
        info.pointer_size,
        ffi.sizeOf<ffi.Pointer<ffi.Void>>(),
      );
      expectLayout(
        'function pointer size',
        info.function_pointer_size,
        ffi.sizeOf<ffi.Pointer<ffi.NativeFunction<ffi.Void Function()>>>(),
      );
      expectLayout('size_t size', info.size_t_size, ffi.sizeOf<ffi.Size>());
      expectLayout('EshiGrade size', info.grade_size, ffi.sizeOf<ffi.Int>());
      expectLayout('EshiResult size', info.result_size, ffi.sizeOf<ffi.Int>());
      expectLayout(
        'EshiEntity size',
        info.entity_size,
        ffi.sizeOf<ffi.Uint32>(),
      );

      if (ffi.sizeOf<ffi.Pointer>() == 8) {
        expectLayout('EshiAbiInfo alignment', info.abi_info_alignment, 4);
        expectLayout('EshiConfig alignment', info.config_alignment, 8);
        expectLayout(
          'EshiTransformView alignment',
          info.transform_view_alignment,
          8,
        );
        expectLayout(
          'EshiVelocityView alignment',
          info.velocity_view_alignment,
          8,
        );
        expectLayout(
          'EshiCollisionEvent alignment',
          info.collision_event_alignment,
          4,
        );
        expectLayout('EshiMaterial alignment', info.material_alignment, 8);
        final offsets = <String, ({int actual, int expected})>{
          'config.width': (actual: info.config_width_offset, expected: 0),
          'config.height': (actual: info.config_height_offset, expected: 4),
          'config.grade': (actual: info.config_grade_offset, expected: 8),
          'config.fixed_dt': (
            actual: info.config_fixed_dt_offset,
            expected: 12,
          ),
          'config.seed': (actual: info.config_seed_offset, expected: 16),
          'config.max_entities': (
            actual: info.config_max_entities_offset,
            expected: 24,
          ),
          'transform.entity': (
            actual: info.transform_view_entity_offset,
            expected: 0,
          ),
          'transform.x': (actual: info.transform_view_x_offset, expected: 8),
          'transform.y': (actual: info.transform_view_y_offset, expected: 16),
          'transform.count': (
            actual: info.transform_view_count_offset,
            expected: 24,
          ),
          'velocity.entity': (
            actual: info.velocity_view_entity_offset,
            expected: 0,
          ),
          'velocity.vx': (actual: info.velocity_view_vx_offset, expected: 8),
          'velocity.vy': (actual: info.velocity_view_vy_offset, expected: 16),
          'velocity.count': (
            actual: info.velocity_view_count_offset,
            expected: 24,
          ),
          'collision.a': (actual: info.collision_event_a_offset, expected: 0),
          'collision.b': (actual: info.collision_event_b_offset, expected: 4),
          'collision.nx': (actual: info.collision_event_nx_offset, expected: 8),
          'collision.ny': (
            actual: info.collision_event_ny_offset,
            expected: 12,
          ),
          'collision.penetration': (
            actual: info.collision_event_penetration_offset,
            expected: 16,
          ),
          'material.cpu_shader': (
            actual: info.material_cpu_shader_offset,
            expected: 0,
          ),
          'material.source_path': (
            actual: info.material_source_path_offset,
            expected: 8,
          ),
          'material.package_path': (
            actual: info.material_package_path_offset,
            expected: 16,
          ),
          'material.uniform_data': (
            actual: info.material_uniform_data_offset,
            expected: 24,
          ),
          'material.uniform_size': (
            actual: info.material_uniform_size_offset,
            expected: 32,
          ),
        };
        for (final entry in offsets.entries) {
          expectLayout(entry.key, entry.value.actual, entry.value.expected);
        }
      }

      expectLayout('ABI version', info.abi_version, native.ESHI_ABI_VERSION);
      expectLayout(
        'command protocol',
        info.command_protocol_version,
        native.ESHI_COMMAND_PROTOCOL_VERSION,
      );
      expectLayout(
        'event protocol',
        info.event_protocol_version,
        native.ESHI_EVENT_PROTOCOL_VERSION,
      );

      return LarimarContract._(
        abiVersion: info.abi_version,
        commandProtocolVersion: info.command_protocol_version,
        eventProtocolVersion: info.event_protocol_version,
      );
    });
  }

  @override
  String toString() =>
      'LarimarContract(abi: $abiVersion, commands: '
      '$commandProtocolVersion, events: $eventProtocolVersion)';
}
