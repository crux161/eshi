import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:larimar_flutter/hero.dart';

void main() {
  testWidgets('composes the HarmonyOS Sans Larimar lockup', (tester) async {
    await tester.pumpWidget(const LarimarBrandApp(testMode: true));

    expect(find.text('larimar'), findsOneWidget);
    expect(find.text('A GAME ENGINE IN MOTION'), findsOneWidget);
    expect(find.byTooltip('Mute'), findsOneWidget);
    expect(find.byType(CircularProgressIndicator), findsOneWidget);
  });
}
