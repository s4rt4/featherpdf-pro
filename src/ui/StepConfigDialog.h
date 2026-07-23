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

#include "backends/BatchRunner.h"

#include <QDialog>
#include <QHash>
#include <QString>
#include <QVariantMap>

class QFormLayout;
class QWidget;

// Edits one batch step's parameters. The form is built from the step's operation
// (text fields, spin boxes, and check boxes as appropriate); params() reads the
// editors back into a QVariantMap with the same keys BatchRunner expects.
class StepConfigDialog : public QDialog {
    Q_OBJECT

public:
    StepConfigDialog(BatchRunner::Op op, const QVariantMap& params, QWidget* parent = nullptr);

    QVariantMap params() const;

private:
    void addText(QFormLayout* form, const QString& key, const QString& label,
                 const QString& value, const QString& placeholder = QString());
    void addInt(QFormLayout* form, const QString& key, const QString& label, int value, int min,
                int max);
    void addDouble(QFormLayout* form, const QString& key, const QString& label, double value,
                   double min, double max, double step);
    void addBool(QFormLayout* form, const QString& key, const QString& label, bool value);
    void addAngle(QFormLayout* form, const QString& key, const QString& label, int value);

    BatchRunner::Op m_op;
    QHash<QString, QWidget*> m_editors;
};
