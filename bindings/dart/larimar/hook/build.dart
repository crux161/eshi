import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:logging/logging.dart';
import 'package:native_toolchain_c/native_toolchain_c.dart';

const _core = '../../../core';
const _filament = '../../../third_party/filament/v1.75.0';

const _filamentLibraries = <String>[
  'gltfio',
  'gltfio_core',
  'filament',
  'backend',
  'filabridge',
  'filaflat',
  'geometry',
  'dracodec',
  'ktxreader',
  'image',
  'meshoptimizer',
  'stb',
  'uberarchive',
  'uberzlib',
  'basis_transcoder',
  'bluegl',
  'bluevk',
  'smol-v',
  'utils',
  'zstd',
];

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;
    final isMacOS = input.config.code.targetOS == OS.macOS;
    final filamentArch =
        input.config.code.targetArchitecture == Architecture.arm64
        ? 'arm64'
        : 'x86_64';
    final filamentLibraryDirectory = input.packageRoot
        .resolve('$_filament/lib/$filamentArch')
        .toFilePath();
    // The pinned macOS distribution ships arm64 only, and a release build of a
    // macOS app is universal — so the x86_64 slice has no Filament to link and
    // used to fail the whole build with `library 'gltfio' not found`. Build
    // that slice without the Filament backend instead: it still has Ink and
    // Metal, and a Mac that runs it falls back the way any tier-less host does.
    final hasFilament =
        isMacOS && Directory(filamentLibraryDirectory).existsSync();
    final builder = CBuilder.library(
      name: 'larimar_core',
      assetName: 'src/generated/larimar_bindings_generated.dart',
      // The macOS build mixes .cpp and .mm. Leaving language selection to
      // clang by extension preserves Objective-C++ for metal.mm; libc++ is
      // linked explicitly below. Other hosts retain the ordinary C++ path.
      language: isMacOS ? Language.objectiveC : Language.cpp,
      std: 'c++20',
      includes: ['$_core/include', if (hasFilament) '$_filament/include'],
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
        if (hasFilament) '$_core/src/render/filament.cpp',
      ],
      frameworks: isMacOS
          ? const ['Cocoa', 'CoreVideo', 'Foundation', 'Metal', 'QuartzCore']
          : const [],
      libraries: [
        if (hasFilament) ..._filamentLibraries,
        if (isMacOS) 'c++',
      ],
      libraryDirectories: hasFilament ? [filamentLibraryDirectory] : const [],
      defines: {
        if (isMacOS) 'ESHI_HAVE_METAL': null,
        if (hasFilament) 'ESHI_HAVE_FILAMENT': null,
      },
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
