// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Headless tests for the WIA scanner backend (Scanner). The actual scan needs
// hardware, so the unit-testable surface is the STI type mapping, the human
// device label, and that probing WIA availability is safe without a scanner.

#include "backends/Scanner.h"

#include <QtTest>

class TestScanner : public QObject {
    Q_OBJECT

private slots:
    void typeLabelMapsKnownStiTypes() {
        QCOMPARE(Scanner::typeLabel(1), QStringLiteral("scanner"));   // StiDeviceTypeScanner
        QCOMPARE(Scanner::typeLabel(2), QStringLiteral("camera"));    // StiDeviceTypeDigitalCamera
        QCOMPARE(Scanner::typeLabel(3), QStringLiteral("video"));     // StiDeviceTypeStreamingVideo
        QCOMPARE(Scanner::typeLabel(0), QString());                   // StiDeviceTypeDefault
        QCOMPARE(Scanner::typeLabel(99), QString());
    }

    void labelIsHumanReadable() {
        Scanner::Device d;
        d.name = QStringLiteral("{6BDD1FC6-810F-11D0-BEC7-08002BE2092F}\\0001");
        d.vendor = QStringLiteral("Epson");
        d.model = QStringLiteral("Perfection V39");
        d.type = QStringLiteral("scanner");
        QCOMPARE(d.label(), QStringLiteral("Epson Perfection V39 (scanner)"));
    }

    void labelFallsBackToDeviceName() {
        // A driver that reports no vendor/model still gets a usable label.
        Scanner::Device d;
        d.name = QStringLiteral("{6BDD1FC6-810F-11D0-BEC7-08002BE2092F}\\0002");
        QCOMPARE(d.label(), d.name);
    }

    void probingWiaIsSafeWithoutHardware() {
        // No assertion on the value (CI runners vary); it must simply not
        // crash or leak COM state, and repeated calls must agree.
        const bool first = Scanner::isAvailable();
        QCOMPARE(Scanner::isAvailable(), first);

        QString error;
        const QList<Scanner::Device> devices = Scanner::devices(&error);
        // With no scanner attached the list is empty; that is not an error
        // unless WIA itself is unreachable.
        if (!devices.isEmpty())
            QVERIFY(!devices.first().name.isEmpty());
    }
};

QTEST_MAIN(TestScanner)
#include "test_scanner.moc"
