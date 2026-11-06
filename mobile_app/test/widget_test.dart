import 'package:flutter_test/flutter_test.dart';

import 'package:riverclean_mobile/main.dart';

void main() {
  testWidgets('app renders Monitor tab', (WidgetTester tester) async {
    await tester.pumpWidget(const RiverCleanApp());
    expect(find.text('Monitor'), findsWidgets);
    expect(find.text('CONNECTION'), findsOneWidget);
  });
}
