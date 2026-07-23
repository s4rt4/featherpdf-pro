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

#include "backends/SnapshotExporter.h"

#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>

namespace {
QPdfDocumentRenderOptions::Rotation rotationFor(int deg) {
    switch (((deg % 360) + 360) % 360) {
    case 90: return QPdfDocumentRenderOptions::Rotation::Clockwise90;
    case 180: return QPdfDocumentRenderOptions::Rotation::Clockwise180;
    case 270: return QPdfDocumentRenderOptions::Rotation::Clockwise270;
    default: return QPdfDocumentRenderOptions::Rotation::None;
    }
}
} // namespace

QImage SnapshotExporter::renderRegion(QPdfDocument* doc, int sourcePage, int rotationDeg,
                                      const QRectF& normRect, int dpi, QString* error) {
    if (!doc || doc->pageCount() <= 0) {
        if (error)
            *error = QStringLiteral("No document to snapshot.");
        return {};
    }
    if (sourcePage < 0 || sourcePage >= doc->pageCount()) {
        if (error)
            *error = QStringLiteral("That page is out of range.");
        return {};
    }

    // Clamp the region to the page and reject an empty selection.
    const QRectF nrm = normRect.normalized().intersected(QRectF(0, 0, 1, 1));
    if (nrm.width() <= 0.0 || nrm.height() <= 0.0) {
        if (error)
            *error = QStringLiteral("The selected region is empty.");
        return {};
    }

    dpi = qBound(36, dpi, 1200);

    QSizeF pt = doc->pagePointSize(sourcePage);
    const int rot = ((rotationDeg % 360) + 360) % 360;
    if (rot == 90 || rot == 270)
        pt.transpose(); // displayed page is the rotated one
    if (pt.width() <= 0 || pt.height() <= 0) {
        if (error)
            *error = QStringLiteral("That page has no size.");
        return {};
    }

    // points (1/72") -> device pixels at the requested DPI (mirrors ImageExporter).
    const QSize px(qRound(pt.width() * dpi / 72.0), qRound(pt.height() * dpi / 72.0));

    QPdfDocumentRenderOptions opts;
    opts.setRenderFlags(QPdfDocumentRenderOptions::RenderFlag::Annotations);
    opts.setRotation(rotationFor(rotationDeg));
    const QImage rendered = doc->render(sourcePage, px, opts);
    if (rendered.isNull()) {
        if (error)
            *error = QStringLiteral("The page couldn't be rendered.");
        return {};
    }

    // Crop the rendered page to the normalized region (in displayed-page fractions).
    QRect crop(qRound(nrm.x() * rendered.width()), qRound(nrm.y() * rendered.height()),
               qRound(nrm.width() * rendered.width()), qRound(nrm.height() * rendered.height()));
    crop = crop.intersected(rendered.rect());
    if (crop.isEmpty()) {
        if (error)
            *error = QStringLiteral("The selected region is empty.");
        return {};
    }
    const QImage cropped = rendered.copy(crop);

    // Flatten onto white - the render is transparent, and PNG/clipboard look best
    // on an opaque background (matches ImageExporter's flattening).
    QImage img(cropped.size(), QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter painter(&img);
    painter.drawImage(0, 0, cropped);
    painter.end();
    return img;
}
