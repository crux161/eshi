import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:larimar/larimar.dart';
import 'package:larimar_flutter/main.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('animates Pong through a real macOS texture across lifecycle', (
    tester,
  ) async {
    try {
      LarimarWorld(
        config: const LarimarWorldConfig(grade: LarimarGrade.brush),
      ).dispose();
    } on LarimarNativeException catch (error) {
      if (error.result == LarimarResult.unsupported) return;
      rethrow;
    }
    Future<void> expectLiveView() async {
      for (var attempt = 0; attempt < 30; attempt += 1) {
        await tester.pump(const Duration(milliseconds: 100));
        if (find.byType(Texture).evaluate().isNotEmpty) break;
      }
      expect(find.byType(Texture), findsOneWidget);
      expect(find.text('Flutter UI over animated Pong'), findsOneWidget);
      expect(find.byKey(const ValueKey<String>('larimar-error')), findsNothing);
    }

    for (var iteration = 0; iteration < 5; iteration += 1) {
      final key = ValueKey<int>(iteration);
      await tester.pumpWidget(
        LarimarApp(key: key, testViewSize: const Size(640, 360)),
      );
      await expectLiveView();

      await tester.pumpWidget(
        LarimarApp(
          key: key,
          testViewSize: Size(672 + iteration * 8, 384 + iteration * 8),
        ),
      );
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.byType(Texture), findsOneWidget);

      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
      await tester.pump(const Duration(milliseconds: 50));
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.byType(Texture), findsOneWidget);

      await tester.pumpWidget(const SizedBox());
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.byType(Texture), findsNothing);
    }
  });
}
