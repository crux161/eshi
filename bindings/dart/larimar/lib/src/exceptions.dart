part of '../larimar.dart';

/// Stable result values exposed by Larimar's native ABI.
enum LarimarResult {
  ok(0),
  invalidArgument(-1),
  unsupported(-2),
  outOfMemory(-3),
  limit(-4),
  stale(-5),
  incompatibleVersion(-6);

  const LarimarResult(this.code);

  factory LarimarResult.fromCode(int code) => switch (code) {
    0 => ok,
    -1 => invalidArgument,
    -2 => unsupported,
    -3 => outOfMemory,
    -4 => limit,
    -5 => stale,
    -6 => incompatibleVersion,
    _ => throw LarimarProtocolException('Unknown native result code: $code'),
  };

  final int code;
}

/// Base class for failures surfaced by the Larimar Dart package.
sealed class LarimarException implements Exception {
  const LarimarException(this.message);

  final String message;

  @override
  String toString() => '$runtimeType: $message';
}

/// A native ABI operation returned an error.
final class LarimarNativeException extends LarimarException {
  const LarimarNativeException(this.result, String message) : super(message);

  final LarimarResult result;
}

/// The generated Dart contract does not match the loaded native library.
final class LarimarVersionException extends LarimarException {
  const LarimarVersionException(super.message);
}

/// An operation was attempted after its owner was disposed.
final class LarimarDisposedException extends LarimarException {
  const LarimarDisposedException(super.message);
}

/// A world was accessed from a different isolate than the one that created it.
final class LarimarIsolateException extends LarimarException {
  const LarimarIsolateException(super.message);
}

/// A command did not fit in the reserved shared buffer.
final class LarimarBufferOverflowException extends LarimarException {
  const LarimarBufferOverflowException(super.message);
}

/// A command or event stream violated the versioned wire contract.
final class LarimarProtocolException extends LarimarException {
  const LarimarProtocolException(super.message);
}

Never _throwNative(native.EshiResult result, String operation) {
  final code = LarimarResult.fromCode(result.value);
  final messagePointer = native.eshi_result_string(result);
  final nativeMessage = messagePointer == ffi.nullptr
      ? 'native operation failed'
      : messagePointer.cast<Utf8>().toDartString();
  throw LarimarNativeException(code, '$operation: $nativeMessage');
}

void _checkNative(native.EshiResult result, String operation) {
  if (result.value != LarimarResult.ok.code) {
    _throwNative(result, operation);
  }
}
