/// Dart ownership and packed-stream API for the Larimar native core.
library;

import 'dart:ffi' as ffi;
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'src/generated/larimar_bindings_generated.dart' as native;

part 'src/commands.dart';
part 'src/contract.dart';
part 'src/events.dart';
part 'src/exceptions.dart';
part 'src/world.dart';
