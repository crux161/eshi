import 'package:hooks/hooks.dart';
import 'package:logging/logging.dart';
import 'package:native_toolchain_c/native_toolchain_c.dart';

const _core = '../../../core';

void main(List<String> args) async {
  await build(args, (input, output) async {
    final builder = CBuilder.library(
      name: 'larimar_core',
      assetName: 'src/generated/larimar_bindings_generated.dart',
      language: Language.cpp,
      std: 'c++11',
      includes: const ['$_core/include'],
      sources: const [
        '$_core/src/abi.cpp',
        '$_core/src/world.cpp',
        '$_core/src/scene.cpp',
        '$_core/src/render/registry.cpp',
        '$_core/src/render/ink.cpp',
      ],
      flags: const ['-Wall', '-Wextra'],
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
