import 'package:flutter_test/flutter_test.dart';
import 'package:larimar_flutter/main.dart';

void main() {
  testWidgets('composes Larimar status with ordinary Flutter UI', (
    tester,
  ) async {
    await tester.pumpWidget(const LarimarApp(testStatus: 'test native ready'));

    expect(find.text('Larimar First Light'), findsOneWidget);
    expect(find.text('Native core online'), findsOneWidget);
    expect(find.text('test native ready'), findsOneWidget);
    expect(find.text('Advance one simulation tick'), findsOneWidget);
  });
}
