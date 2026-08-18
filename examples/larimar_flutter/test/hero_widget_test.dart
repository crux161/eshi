import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:larimar_flutter/hero.dart';

void main() {
  testWidgets('composes the HarmonyOS Sans Larimar lockup', (tester) async {
    await tester.pumpWidget(const LarimarBrandApp(testMode: true));

    expect(find.text('larimar'), findsOneWidget);
    expect(find.text('A GAME ENGINE IN MOTION'), findsOneWidget);
    expect(find.byTooltip('Mute'), findsOneWidget);

    // No spinner: the intro opens on darkness and the stone arrives out of it.
    // A progress indicator in the middle of a brand sequence is the one thing
    // guaranteed to read as software rather than as a product.
    expect(find.byType(CircularProgressIndicator), findsNothing);
  });

  testWidgets('holds the name and the line back until the stone has turned', (
    tester,
  ) async {
    await tester.pumpWidget(const LarimarBrandApp(testMode: true, phase: 0));
    // Everything is built at every phase; what changes is whether it is
    // visible, so opacity is what a test has to read.
    expect(_opacityOf(tester, 'larimar'), lessThan(0.02));

    await tester.pumpWidget(const LarimarBrandApp(testMode: true, phase: 0.55));
    await tester.pump();
    expect(_opacityOf(tester, 'larimar'), greaterThan(0.9));
    expect(_opacityOf(tester, 'A GAME ENGINE IN MOTION'), lessThan(0.02));

    await tester.pumpWidget(const LarimarBrandApp(testMode: true, phase: 0.97));
    await tester.pump();
    expect(_opacityOf(tester, 'A GAME ENGINE IN MOTION'), greaterThan(0.9));
  });
}

double _opacityOf(WidgetTester tester, String text) {
  final opacity = tester.widget<Opacity>(
    find
        .ancestor(of: find.text(text), matching: find.byType(Opacity))
        .first,
  );
  return opacity.opacity;
}
