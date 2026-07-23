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

#pragma once

#include <QImage>
#include <QTransform>

// Pure, GUI-free image operations that clean up a rendered page before it is
// handed to Tesseract. Each function takes a QImage and returns a new QImage so
// they can be unit-tested without any GUI or PDF/OCR machinery. QImage-only -
// no extra system dependencies.
namespace OcrPreprocess {

// Convert to single-channel 8-bit grayscale (Format_Grayscale8).
QImage grayscale(const QImage& src);

// Otsu's method: the 0..255 threshold that maximizes the between-class variance
// of a grayscale image's histogram. `gray` is converted to grayscale if needed.
int otsuThreshold(const QImage& gray);

// Grayscale + Otsu binarization. The result is Format_Grayscale8 with exactly
// two levels: 0 (ink/black) and 255 (paper/white).
QImage binarize(const QImage& src);

// Drop isolated specks with a 3x3 median filter (a morphological clean-up on the
// grayscale image). Edges are handled by clamping. Returns Format_Grayscale8.
QImage despeckle(const QImage& src);

// Estimate the rotation (in degrees) that straightens the page: the angle within
// [-maxAngleDeg, +maxAngleDeg] (stepped by stepDeg) whose rotated horizontal
// projection profile (per-row ink sums) has the greatest variance. The returned
// value is the *correction* angle - rotating the image by it straightens the
// text - so an image skewed clockwise by +5deg yields about -5deg.
double estimateSkew(const QImage& src, double maxAngleDeg = 8.0, double stepDeg = 0.5);

// Rotate `src` by `angleDeg` (counter-clockwise positive), expanding the canvas
// and filling exposed corners with white. If `srcToDst` is non-null it receives
// the transform mapping source pixel coordinates to the returned image's
// coordinates (its inverse maps back).
QImage rotated(const QImage& src, double angleDeg, QTransform* srcToDst = nullptr);

// Estimate the skew and rotate to straighten it.
QImage deskew(const QImage& src);

// Convenience: which clean-up steps to run, in order grayscale/deskew ->
// binarize -> despeckle.
struct Options {
    bool deskew = true;
    bool binarize = true;
    bool despeckle = true;
};

// Apply the enabled steps and return the cleaned image.
QImage process(const QImage& src, const Options& opt);

} // namespace OcrPreprocess
