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

#include <QColor>
#include <QDialog>
#include <QVector>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QRadioButton;
class QWidget;

// Collects the settings for a centred, rotated text watermark.
class WatermarkDialog : public QDialog {
    Q_OBJECT

public:
    explicit WatermarkDialog(QWidget* parent = nullptr);

    bool useImage() const;      // image watermark vs text
    QString imagePath() const;
    double scale() const;       // image: fraction of page width

    QString text() const;
    QColor color() const { return m_color; }
    double opacity() const;     // 0..1
    double fontSize() const;
    double rotation() const;

private:
    void selectSwatch(int i);
    void updateMode();

    QRadioButton* m_modeText = nullptr;
    QRadioButton* m_modeImage = nullptr;
    QWidget* m_textRows = nullptr;
    QWidget* m_imageRows = nullptr;
    QLineEdit* m_text = nullptr;
    QLineEdit* m_imagePath = nullptr;
    QSpinBox* m_scale = nullptr;
    QSpinBox* m_opacity = nullptr;
    QSpinBox* m_size = nullptr;
    QSpinBox* m_rotation = nullptr;
    QPushButton* m_apply = nullptr;
    QVector<QPushButton*> m_swatches;
    QVector<QColor> m_colors;
    QColor m_color;
};
