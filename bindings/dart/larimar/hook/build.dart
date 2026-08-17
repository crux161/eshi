import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:logging/logging.dart';
import 'package:native_toolchain_c/native_toolchain_c.dart';

const _core = '../../../core';

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;
    final isMacOS = input.config.code.targetOS == OS.macOS;
    final builder = CBuilder.library(
      name: 'larimar_core',
      assetName: 'src/generated/larimar_bindings_generated.dart',
      // The macOS build mixes .cpp and .mm. Leaving language selection to
      // clang by extension preserves Objective-C++ for metal.mm; libc++ is
      // linked explicitly below. Other hosts retain the ordinary C++ path.
      language: isMacOS ? Language.objectiveC : Language.cpp,
      std: 'c++11',
      includes: const ['$_core/include'],
      sources: [
        '$_core/src/abi.cpp',
        '$_core/src/world.cpp',
        '$_core/src/scene.cpp',
        '$_core/src/render/registry.cpp',
        '$_core/src/render/ink.cpp',
        if (isMacOS) ...[
          '$_core/src/render/transpile.cpp',
          '$_core/src/render/metal.mm',
        ],
      ],
      frameworks: isMacOS ? const ['Foundation', 'Metal'] : const [],
      libraries: isMacOS ? const ['c++'] : const [],
      defines: isMacOS ? const {'ESHI_HAVE_METAL': null} : const {},
      flags: [
        '-Wall',
        '-Wextra',
        if (isMacOS) ...['-fobjc-arc', '-Wno-nullability-completeness'],
      ],
    );
    await builder.run(
      input: input,
      output: output,
      logger: Logger('')
        ..level = Level.ALL
        // ignore: avoid_print
        ..onRecord.listen((record) => print(record.message)),
    );
  });
}
