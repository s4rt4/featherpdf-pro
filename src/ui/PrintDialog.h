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

#include <QDialog>
#include <QList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QWidget;
class QPdfDocument;

// A custom print dialog. Unlike Qt's native QPrintDialog - which enumerates CUPS
// printers synchronously on the UI thread as it opens (freezing the app) - this
// opens instantly: "Print to File (PDF)" is available immediately and system
// printers are discovered on a background thread and added when ready. It also
// shows a live preview of exactly what will print (the current edit arrangement:
// order, rotation, deletions).
//
// The dialog only collects the user's choices; the caller does the rendering.
class PrintDialog : public QDialog {
    Q_OBJECT

public:
    PrintDialog(QPdfDocument* pdf, QVector<int> order, QVector<int> rotations,
                const QString& docName, int currentSlot, QWidget* parent = nullptr);

    QString selectedPrinter() const; // empty ⇒ print to file
    QString outputFile() const;
    QList<int> selectedSlots() const; // 0-based display slots, in print order
    int copies() const;
    bool grayscale() const;

private:
    void rebuildSelection(); // recompute which pages print, refresh the preview
    void showPreviewAt(int positionInSelection);
    QPixmap renderSlot(int slot, int maxW, int maxH) const;
    QList<int> parseRange(const QString& text) const;
    void updateOutputRowVisibility();
    QWidget* buildOptionsPane(const QString& docName); // left column
    QWidget* buildPreviewPane();                       // right column

    QPdfDocument* m_pdf = nullptr;
    QVector<int> m_order;
    QVector<int> m_rot;
    int m_pageCount = 0;
    int m_currentSlot = 0;

    QComboBox* m_printer = nullptr;
    QWidget* m_outputRow = nullptr;
    QLineEdit* m_outputFile = nullptr;
    QRadioButton* m_rangeAll = nullptr;
    QRadioButton* m_rangeCurrent = nullptr;
    QRadioButton* m_rangeCustom = nullptr;
    QLineEdit* m_rangeText = nullptr;
    QComboBox* m_subset = nullptr;   // all / odd / even pages in the range
    QSpinBox* m_copies = nullptr;
    QCheckBox* m_grayscale = nullptr;

    QLabel* m_preview = nullptr;
    QLabel* m_previewCounter = nullptr;
    QPushButton* m_prevPreview = nullptr;
    QPushButton* m_nextPreview = nullptr;

    QList<int> m_selection; // slots to print
    int m_previewPos = 0;   // index into m_selection
};
