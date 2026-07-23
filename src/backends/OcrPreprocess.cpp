// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "backends/OcrPreprocess.h"

#include <QPainter>
#include <QRect>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace OcrPreprocess {
namespace {

// Build the 256-bin histogram of an 8-bit grayscale image.
std::array<long, 256> histogram(const QImage& gray) {
    std::array<long, 256> h{};
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* s = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x)
            ++h[s[x]];
    }
    return h;
}

// An ink mask: Format_Grayscale8 where dark (<= threshold) pixels become 255
// ("ink present") and everything else 0. Rotating such a mask keeps exposed
// corners at 0, so they read as no ink - exactly what the projection wants.
QImage inkMask(const QImage& gray, int threshold) {
    const int w = gray.width(), h = gray.height();
    QImage mask(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        const uchar* s = gray.constScanLine(y);
        uchar* d = mask.scanLine(y);
        for (int x = 0; x < w; ++x)
            d[x] = s[x] <= threshold ? 255 : 0;
    }
    return mask;
}

// Variance of the horizontal projection profile (per-row ink sums) after
// rotating the ink mask by `angle` degrees. A larger value means the ink packs
// into sharp horizontal bands - i.e. the lines are level.
double profileVariance(const QImage& ink, double angle) {
    QImage r = ink;
    if (std::abs(angle) > 1e-3)
        r = ink.transformed(QTransform().rotate(angle), Qt::FastTransformation);
    const int rw = r.width(), rh = r.height();
    if (rw == 0 || rh == 0)
        return 0.0;

    std::vector<double> rowSum(rh, 0.0);
    for (int y = 0; y < rh; ++y) {
        const uchar* s = r.constScanLine(y);
        long sum = 0;
        for (int x = 0; x < rw; ++x)
            sum += s[x];
        rowSum[y] = static_cast<double>(sum) / 255.0; // count of ink pixels
    }

    double mean = 0.0;
    for (double v : rowSum)
        mean += v;
    mean /= rh;
    double var = 0.0;
    for (double v : rowSum) {
        const double d = v - mean;
        var += d * d;
    }
    return var / rh;
}

} // namespace

QImage grayscale(const QImage& src) {
    if (src.format() == QImage::Format_Grayscale8)
        return src;
    return src.convertToFormat(QImage::Format_Grayscale8);
}

int otsuThreshold(const QImage& grayIn) {
    const QImage gray = grayscale(grayIn);
    const std::array<long, 256> hist = histogram(gray);

    const long total = static_cast<long>(gray.width()) * gray.height();
    if (total == 0)
        return 127;

    double sumAll = 0.0;
    for (int i = 0; i < 256; ++i)
        sumAll += static_cast<double>(i) * hist[i];

    double sumB = 0.0;   // weighted sum of the background class
    long weightB = 0;    // pixel count of the background class
    double bestVar = -1.0;
    int bestT = 127;
    for (int t = 0; t < 256; ++t) {
        weightB += hist[t];
        if (weightB == 0)
            continue;
        const long weightF = total - weightB;
        if (weightF == 0)
            break;
        sumB += static_cast<double>(t) * hist[t];
        const double meanB = sumB / weightB;
        const double meanF = (sumAll - sumB) / weightF;
        const double diff = meanB - meanF;
        const double between = static_cast<double>(weightB) * weightF * diff * diff;
        if (between > bestVar) {
            bestVar = between;
            bestT = t;
        }
    }
    return bestT;
}

QImage binarize(const QImage& src) {
    const QImage gray = grayscale(src);
    const int t = otsuThreshold(gray);
    const int w = gray.width(), h = gray.height();
    QImage out(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        const uchar* s = gray.constScanLine(y);
        uchar* d = out.scanLine(y);
        for (int x = 0; x < w; ++x)
            d[x] = s[x] <= t ? 0 : 255; // ink -> black, paper -> white
    }
    return out;
}

QImage despeckle(const QImage& src) {
    const QImage gray = grayscale(src);
    const int w = gray.width(), h = gray.height();
    if (w < 3 || h < 3)
        return gray;

    QImage out(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        uchar* d = out.scanLine(y);
        const int y0 = std::max(0, y - 1);
        const int y1 = y;
        const int y2 = std::min(h - 1, y + 1);
        const uchar* r0 = gray.constScanLine(y0);
        const uchar* r1 = gray.constScanLine(y1);
        const uchar* r2 = gray.constScanLine(y2);
        for (int x = 0; x < w; ++x) {
            const int x0 = std::max(0, x - 1);
            const int x2 = std::min(w - 1, x + 1);
            std::array<uchar, 9> n{r0[x0], r0[x], r0[x2], r1[x0], r1[x],
                                   r1[x2], r2[x0], r2[x], r2[x2]};
            std::nth_element(n.begin(), n.begin() + 4, n.end());
            d[x] = n[4]; // the median
        }
    }
    return out;
}

double estimateSkew(const QImage& src, double maxAngleDeg, double stepDeg) {
    QImage gray = grayscale(src);
    // Skew is a global property; a downscaled copy is plenty and far faster.
    if (gray.width() > 1200)
        gray = gray.scaledToWidth(1200, Qt::SmoothTransformation);

    const int t = otsuThreshold(gray);
    const QImage ink = inkMask(gray, t);

    if (stepDeg <= 0.0)
        stepDeg = 0.5;
    double best = 0.0;
    double bestScore = -1.0;
    for (double a = -maxAngleDeg; a <= maxAngleDeg + 1e-9; a += stepDeg) {
        const double score = profileVariance(ink, a);
        if (score > bestScore) {
            bestScore = score;
            best = a;
        }
    }
    return best;
}

QImage rotated(const QImage& src, double angleDeg, QTransform* srcToDst) {
    if (std::abs(angleDeg) < 1e-3) {
        if (srcToDst)
            *srcToDst = QTransform();
        return src;
    }
    QTransform t;
    t.rotate(angleDeg);
    const QRect bounds = t.mapRect(QRect(0, 0, src.width(), src.height()));

    QImage out(bounds.size(),
               src.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB888);
    out.fill(Qt::white);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.translate(-bounds.x(), -bounds.y());
    p.rotate(angleDeg);
    p.drawImage(0, 0, src);
    p.end();

    if (srcToDst) {
        QTransform full;
        full.translate(-bounds.x(), -bounds.y());
        full.rotate(angleDeg);
        *srcToDst = full;
    }
    return out;
}

QImage deskew(const QImage& src) {
    return rotated(src, estimateSkew(src));
}

QImage process(const QImage& src, const Options& opt) {
    QImage img = src;
    if (opt.deskew)
        img = deskew(img);
    if (opt.binarize)
        img = binarize(img);
    else
        img = grayscale(img);
    if (opt.despeckle)
        img = despeckle(img);
    return img;
}

} // namespace OcrPreprocess
