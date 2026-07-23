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

#include "ui/CompareReportDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
QString joinWords(const QStringList& words, const QString& colour) {
    QStringList esc;
    esc.reserve(words.size());
    for (const QString& w : words)
        esc << w.toHtmlEscaped();
    return QStringLiteral("<span style=\"color:%1\">%2</span>").arg(colour, esc.join(QLatin1Char(' ')));
}
}

CompareReportDialog::CompareReportDialog(const TextComparer::Result& result, const QString& oldName,
                                         const QString& newName, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Text Comparison"));

    const Theme::Palette& p = Theme::instance().palette();
    const auto css = [](const QColor& c) {
        return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1,%2,%3,%4)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                      .arg(QString::number(c.alphaF(), 'f', 3));
    };
    QColor border = p.text;
    border.setAlpha(46);
    setStyleSheet(QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                                 "QLabel { color:%2; }"
                                 "QTextBrowser { background:%3; border:1px solid %4;"
                                 " border-radius:8px; color:%2; }")
                      .arg(css(p.dim), css(p.text), css(p.surface), css(border)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* summary = new QLabel(this);
    if (!result.changed())
        summary->setText(tr("No text differences found."));
    else
        summary->setText(tr("%n page(s) changed", "", result.pages.size()) +
                         tr(": %1 added, %2 removed")
                             .arg(result.addedWords)
                             .arg(result.removedWords));
    root->addWidget(summary);

    auto* legend = new QLabel(
        tr("Comparing %1 (old) → %2 (new).").arg(oldName.toHtmlEscaped(), newName.toHtmlEscaped()),
        this);
    legend->setObjectName(QStringLiteral("Hint"));
    legend->setWordWrap(true);
    root->addWidget(legend);

    // Pull readable red/green that work on both themes.
    const QString red = QStringLiteral("#e1554c");
    const QString green = QStringLiteral("#3fa45b");

    auto* view = new QTextBrowser(this);
    QString html;
    for (const TextComparer::PageDiff& d : result.pages) {
        html += QStringLiteral("<p style=\"margin:8px 0 2px 0\"><b>%1</b></p>")
                    .arg(tr("Page %1").arg(d.page + 1));
        if (!d.removed.isEmpty())
            html += QStringLiteral("<p style=\"margin:0\">− %1</p>").arg(joinWords(d.removed, red));
        if (!d.added.isEmpty())
            html += QStringLiteral("<p style=\"margin:0\">+ %1</p>").arg(joinWords(d.added, green));
    }
    if (html.isEmpty())
        html = QStringLiteral("<p>%1</p>").arg(tr("The two documents have the same text."));
    view->setHtml(html);
    root->addWidget(view, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);

    resize(640, 520);
}
