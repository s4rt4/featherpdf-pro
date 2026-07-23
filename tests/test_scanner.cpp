// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Headless tests for the SANE scanner backend (Scanner). The actual scan needs
// hardware, so the unit-testable surface is the `scanimage -f` device-list
// parsing and the human label it produces.

#include "backends/Scanner.h"

#include <QtTest>

class TestScanner : public QObject {
    Q_OBJECT

private slots:
    void parsesFormattedDeviceList() {
        // The format we ask scanimage for: device|vendor|model|type per line.
        const QString raw = QStringLiteral(
            "epson2:libusb:001:004|Epson|Perfection V39|flatbed scanner\n"
            "net:192.168.1.5:airscan|Brother|MFC-L2710DW|multi-function peripheral\n");
        const QList<Scanner::Device> d = Scanner::parseDeviceList(raw);
        QCOMPARE(d.size(), 2);
        QCOMPARE(d.at(0).name, QStringLiteral("epson2:libusb:001:004"));
        QCOMPARE(d.at(0).vendor, QStringLiteral("Epson"));
        QCOMPARE(d.at(0).model, QStringLiteral("Perfection V39"));
        QCOMPARE(d.at(0).type, QStringLiteral("flatbed scanner"));
        QCOMPARE(d.at(1).name, QStringLiteral("net:192.168.1.5:airscan"));
    }

    void labelIsHumanReadable() {
        Scanner::Device d;
        d.name = QStringLiteral("epson2:libusb:001:004");
        d.vendor = QStringLiteral("Epson");
        d.model = QStringLiteral("Perfection V39");
        d.type = QStringLiteral("flatbed scanner");
        QCOMPARE(d.label(), QStringLiteral("Epson Perfection V39 (flatbed scanner)"));
    }

    void labelFallsBackToDeviceName() {
        // A backend that reports no vendor/model still gets a usable label.
        Scanner::Device d;
        d.name = QStringLiteral("test:0");
        QCOMPARE(d.label(), QStringLiteral("test:0"));
    }

    void ignoresBlankAndPartialLines() {
        const QString raw = QStringLiteral(
            "\n"
            "   \n"
            "plustek:libusb:002:003|Canon|LiDE 220|flatbed scanner\n");
        const QList<Scanner::Device> d = Scanner::parseDeviceList(raw);
        QCOMPARE(d.size(), 1);
        QCOMPARE(d.at(0).vendor, QStringLiteral("Canon"));
        // A device line may have only the name (some backends omit metadata).
        const QList<Scanner::Device> bare =
            Scanner::parseDeviceList(QStringLiteral("dummy:0\n"));
        QCOMPARE(bare.size(), 1);
        QCOMPARE(bare.at(0).name, QStringLiteral("dummy:0"));
    }
};

QTEST_MAIN(TestScanner)
#include "test_scanner.moc"
