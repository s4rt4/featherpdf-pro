// Feather PDF — tests.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).

#include "backends/OcrPreprocess.h"

#include <QImage>
#include <QSet>
#include <QtTest>

#include <cmath>

namespace {

// Distinct luminance levels present in an image.
QSet<int> levels(const QImage& img) {
    QSet<int> s;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            s.insert(qGray(img.pixel(x, y)));
    return s;
}

// A page-like image of dark horizontal text bands on a white background.
QImage textBands(int w = 400, int h = 400) {
    QImage img(w, h, QImage::Format_Grayscale8);
    img.fill(255);
    for (int y = 0; y < h; ++y) {
        // A band of ~6px every 20px: clear horizontal structure.
        const bool band = (y % 20) < 6 && y > 30 && y < h - 30;
        if (!band)
            continue;
        uchar* row = img.scanLine(y);
        for (int x = 40; x < w - 40; ++x)
            row[x] = 0;
    }
    return img;
}

} // namespace

class TestOcrPreprocess : public QObject {
    Q_OBJECT

private slots:
    // Otsu binarization of a smooth gray gradient must collapse to exactly two
    // luminance levels: pure black and pure white.
    void binarizeGivesTwoLevels() {
        QImage gradient(256, 64, QImage::Format_RGB888);
        for (int x = 0; x < gradient.width(); ++x) {
            const QRgb c = qRgb(x, x, x);
            for (int y = 0; y < gradient.height(); ++y)
                gradient.setPixel(x, y, c);
        }

        const QImage bin = OcrPreprocess::binarize(gradient);
        const QSet<int> lv = levels(bin);
        QCOMPARE(lv.size(), 2);
        QVERIFY(lv.contains(0));
        QVERIFY(lv.contains(255));
    }

    // Isolated specks on a blank page must be wiped out by despeckle.
    void despeckleRemovesSpecks() {
        QImage img(80, 80, QImage::Format_Grayscale8);
        img.fill(255);
        const QVector<QPoint> specks{{10, 10}, {40, 25}, {60, 60}, {20, 70}, {70, 15}};
        for (const QPoint& p : specks)
            img.scanLine(p.y())[p.x()] = 0;
        // Sanity: the specks are really there before cleaning.
        QVERIFY(levels(img).contains(0));

        const QImage clean = OcrPreprocess::despeckle(img);
        for (const QPoint& p : specks)
            QCOMPARE(int(clean.scanLine(p.y())[p.x()]), 255);
        // Nothing dark survives - the page is blank again.
        QCOMPARE(levels(clean).size(), 1);
        QVERIFY(levels(clean).contains(255));
    }

    // A straight page should be detected as having (almost) no skew.
    void deskewDetectsStraight() {
        const double a = OcrPreprocess::estimateSkew(textBands());
        QVERIFY2(std::abs(a) <= 1.0, qPrintable(QString::number(a)));
    }

    // Rotating the bands by a known angle and estimating skew should recover the
    // correction angle (the negative of the applied rotation), and applying
    // deskew should substantially reduce the residual skew.
    void deskewRecoversKnownAngle() {
        const double applied = 5.0; // rotate the content clockwise by 5 degrees
        const QImage skewed = OcrPreprocess::rotated(textBands(), applied);

        const double est = OcrPreprocess::estimateSkew(skewed);
        // The correction angle should be about -5 degrees, within ~1 degree.
        QVERIFY2(std::abs(est - (-applied)) <= 1.0, qPrintable(QString::number(est)));

        // And straightening should leave far less residual skew than we started with.
        const QImage fixed = OcrPreprocess::deskew(skewed);
        const double residual = OcrPreprocess::estimateSkew(fixed);
        QVERIFY2(std::abs(residual) < std::abs(est),
                 qPrintable(QString("residual=%1 est=%2").arg(residual).arg(est)));
        QVERIFY2(std::abs(residual) <= 1.5, qPrintable(QString::number(residual)));
    }
};

QTEST_GUILESS_MAIN(TestOcrPreprocess)
#include "test_ocrpreprocess.moc"
