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
#include <QList>
#include <QString>

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;

// The batch / action wizard. The user assembles an ordered "action" (a list of
// steps, each one of the document operations) and a set of input files, then
// runs the action over every file, writing the results into an output folder.
// The heavy lifting is BatchRunner; this dialog is the catalog + queue UI.
class BatchDialog : public QDialog {
    Q_OBJECT

public:
    explicit BatchDialog(const QStringList& initialFiles, QWidget* parent = nullptr);

private:
    void addFiles();
    void removeSelectedFile();
    void addStep();
    void editSelectedStep();
    void removeSelectedStep();
    void moveStep(int delta);
    void saveActionToFile();
    void loadActionFromFile();
    void chooseOutputFolder();
    void refreshSteps();
    void updateRunState();
    void run();
    QString defaultOutputFolder() const;

    QListWidget* m_files = nullptr;
    QListWidget* m_steps = nullptr;
    QList<BatchRunner::Step> m_stepData;
    QLineEdit* m_suffix = nullptr;
    QLabel* m_folderLabel = nullptr;
    QString m_outputFolder;
    QProgressBar* m_progress = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_run = nullptr;
    bool m_running = false;
};
