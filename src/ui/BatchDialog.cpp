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

#include "ui/BatchDialog.h"

#include "ui/StepConfigDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {
QString cssColor(const QColor& c) {
    return c.alpha() == 255 ? c.name(QColor::HexRgb)
                            : QStringLiteral("rgba(%1,%2,%3,%4)")
                                  .arg(c.red())
                                  .arg(c.green())
                                  .arg(c.blue())
                                  .arg(QString::number(c.alphaF(), 'f', 3));
}
} // namespace

BatchDialog::BatchDialog(const QStringList& initialFiles, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Batch / Action"));

    const Theme::Palette& pal = Theme::instance().palette();
    QColor ctlBorder = pal.text;
    ctlBorder.setAlpha(46);
    setStyleSheet(
        QStringLiteral(
            "QLabel#Hint { color:%2; font-size:12px; }"
            "QLabel#Section { color:%4; font-weight:600; }"
            "QListWidget { background:%1; border:1px solid %3; border-radius:8px; padding:4px;"
            " color:%4; outline:0; }"
            "QListWidget::item { padding:5px 6px; border-radius:6px; }"
            "QListWidget::item:selected { background:%5; color:%4; }"
            "QLineEdit { background:%1; border:1px solid %3; border-radius:8px; padding:6px 9px;"
            " color:%4; selection-background-color:%6; selection-color:#FFFFFF; }"
            "QLineEdit:focus { border:1px solid %6; }"
            "QPushButton#Ghost { background:transparent; border:1px solid %3; border-radius:8px;"
            " padding:5px 12px; color:%4; }"
            "QPushButton#Ghost:hover { border:1px solid %6; color:%6; }"
            "QPushButton#Ghost:disabled { color:%2; border-color:%3; }"
            "QProgressBar { background:%1; border:1px solid %3; border-radius:7px; height:8px;"
            " text-align:center; color:%4; }"
            "QProgressBar::chunk { background:%6; border-radius:6px; }")
            .arg(cssColor(pal.surface), cssColor(pal.dim), cssColor(ctlBorder), cssColor(pal.text),
                 cssColor(pal.accentTint), cssColor(pal.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(10);

    auto* hint = new QLabel(
        tr("Build an action once, then run it over many files. Each file is piped through the "
           "steps in order and written to the output folder."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(16);

    // Files column.
    auto* filesCol = new QVBoxLayout;
    filesCol->setSpacing(6);
    auto* filesLabel = new QLabel(tr("Files"), this);
    filesLabel->setObjectName(QStringLiteral("Section"));
    filesCol->addWidget(filesLabel);
    m_files = new QListWidget(this);
    m_files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_files->setMinimumSize(240, 200);
    filesCol->addWidget(m_files, 1);
    auto* filesButtons = new QHBoxLayout;
    filesButtons->setSpacing(8);
    auto* addFilesBtn = new QPushButton(tr("Add files…"), this);
    addFilesBtn->setObjectName(QStringLiteral("Ghost"));
    addFilesBtn->setCursor(Qt::PointingHandCursor);
    auto* removeFileBtn = new QPushButton(tr("Remove"), this);
    removeFileBtn->setObjectName(QStringLiteral("Ghost"));
    removeFileBtn->setCursor(Qt::PointingHandCursor);
    filesButtons->addWidget(addFilesBtn);
    filesButtons->addWidget(removeFileBtn);
    filesButtons->addStretch(1);
    filesCol->addLayout(filesButtons);
    columns->addLayout(filesCol, 1);

    // Steps column.
    auto* stepsCol = new QVBoxLayout;
    stepsCol->setSpacing(6);
    auto* stepsLabel = new QLabel(tr("Action steps"), this);
    stepsLabel->setObjectName(QStringLiteral("Section"));
    stepsCol->addWidget(stepsLabel);
    m_steps = new QListWidget(this);
    m_steps->setMinimumSize(240, 200);
    stepsCol->addWidget(m_steps, 1);
    auto* stepsButtons = new QHBoxLayout;
    stepsButtons->setSpacing(8);
    auto* addStepBtn = new QPushButton(tr("Add step…"), this);
    addStepBtn->setObjectName(QStringLiteral("Ghost"));
    addStepBtn->setCursor(Qt::PointingHandCursor);
    auto* editStepBtn = new QPushButton(tr("Edit"), this);
    editStepBtn->setObjectName(QStringLiteral("Ghost"));
    editStepBtn->setCursor(Qt::PointingHandCursor);
    auto* removeStepBtn = new QPushButton(tr("Remove"), this);
    removeStepBtn->setObjectName(QStringLiteral("Ghost"));
    removeStepBtn->setCursor(Qt::PointingHandCursor);
    auto* upBtn = new QPushButton(QStringLiteral("↑"), this);
    upBtn->setObjectName(QStringLiteral("Ghost"));
    upBtn->setCursor(Qt::PointingHandCursor);
    upBtn->setFixedWidth(36);
    auto* downBtn = new QPushButton(QStringLiteral("↓"), this);
    downBtn->setObjectName(QStringLiteral("Ghost"));
    downBtn->setCursor(Qt::PointingHandCursor);
    downBtn->setFixedWidth(36);
    stepsButtons->addWidget(addStepBtn);
    stepsButtons->addWidget(editStepBtn);
    stepsButtons->addWidget(removeStepBtn);
    stepsButtons->addStretch(1);
    stepsButtons->addWidget(upBtn);
    stepsButtons->addWidget(downBtn);
    stepsCol->addLayout(stepsButtons);

    // Reusable actions: save the assembled steps to a file, or load one back
    // (the same file the `feather-pdf watch` daemon consumes).
    auto* actionButtons = new QHBoxLayout;
    auto* saveActionBtn = new QPushButton(tr("Save action…"), this);
    saveActionBtn->setObjectName(QStringLiteral("Ghost"));
    saveActionBtn->setCursor(Qt::PointingHandCursor);
    auto* loadActionBtn = new QPushButton(tr("Load action…"), this);
    loadActionBtn->setObjectName(QStringLiteral("Ghost"));
    loadActionBtn->setCursor(Qt::PointingHandCursor);
    actionButtons->addWidget(saveActionBtn);
    actionButtons->addWidget(loadActionBtn);
    actionButtons->addStretch(1);
    stepsCol->addLayout(actionButtons);
    columns->addLayout(stepsCol, 1);

    root->addLayout(columns, 1);

    // Output row.
    auto* outGrid = new QGridLayout;
    outGrid->setHorizontalSpacing(10);
    outGrid->setVerticalSpacing(8);
    auto* folderCaption = new QLabel(tr("Output folder"), this);
    folderCaption->setObjectName(QStringLiteral("Section"));
    m_folderLabel = new QLabel(this);
    m_folderLabel->setObjectName(QStringLiteral("Hint"));
    auto* changeFolder = new QPushButton(tr("Change…"), this);
    changeFolder->setObjectName(QStringLiteral("Ghost"));
    changeFolder->setCursor(Qt::PointingHandCursor);
    outGrid->addWidget(folderCaption, 0, 0);
    outGrid->addWidget(m_folderLabel, 0, 1);
    outGrid->addWidget(changeFolder, 0, 2);
    auto* suffixCaption = new QLabel(tr("Filename suffix"), this);
    suffixCaption->setObjectName(QStringLiteral("Section"));
    m_suffix = new QLineEdit(QStringLiteral("-batch"), this);
    m_suffix->setMinimumHeight(fontMetrics().height() + 14);
    outGrid->addWidget(suffixCaption, 1, 0);
    outGrid->addWidget(m_suffix, 1, 1, 1, 2);
    outGrid->setColumnStretch(1, 1);
    root->addLayout(outGrid);

    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->hide();
    root->addWidget(m_progress);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("Hint"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(this);
    m_run = qobject_cast<QPushButton*>(buttons->addButton(tr("Run"), QDialogButtonBox::ActionRole));
    m_run->setObjectName(QStringLiteral("Share"));
    m_run->setCursor(Qt::PointingHandCursor);
    buttons->addButton(QDialogButtonBox::Close);
    root->addWidget(buttons);

    connect(addFilesBtn, &QPushButton::clicked, this, &BatchDialog::addFiles);
    connect(removeFileBtn, &QPushButton::clicked, this, &BatchDialog::removeSelectedFile);
    connect(addStepBtn, &QPushButton::clicked, this, &BatchDialog::addStep);
    connect(editStepBtn, &QPushButton::clicked, this, &BatchDialog::editSelectedStep);
    connect(removeStepBtn, &QPushButton::clicked, this, &BatchDialog::removeSelectedStep);
    connect(upBtn, &QPushButton::clicked, this, [this] { moveStep(-1); });
    connect(downBtn, &QPushButton::clicked, this, [this] { moveStep(1); });
    connect(saveActionBtn, &QPushButton::clicked, this, &BatchDialog::saveActionToFile);
    connect(loadActionBtn, &QPushButton::clicked, this, &BatchDialog::loadActionFromFile);
    connect(changeFolder, &QPushButton::clicked, this, &BatchDialog::chooseOutputFolder);
    connect(m_steps, &QListWidget::itemDoubleClicked, this, &BatchDialog::editSelectedStep);
    connect(m_run, &QPushButton::clicked, this, &BatchDialog::run);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_files->model(), &QAbstractItemModel::rowsInserted, this,
            &BatchDialog::updateRunState);
    connect(m_files->model(), &QAbstractItemModel::rowsRemoved, this,
            &BatchDialog::updateRunState);

    for (const QString& f : initialFiles)
        if (!f.isEmpty())
            m_files->addItem(f);
    m_outputFolder = defaultOutputFolder();
    m_folderLabel->setText(m_outputFolder);
    updateRunState();

    resize(720, 540);
}

QString BatchDialog::defaultOutputFolder() const {
    if (m_files->count() > 0)
        return QFileInfo(m_files->item(0)->text()).absolutePath();
    return QDir::homePath();
}

void BatchDialog::addFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Add PDF files"), m_outputFolder, tr("PDF documents (*.pdf)"));
    for (const QString& f : files) {
        if (m_files->findItems(f, Qt::MatchExactly).isEmpty())
            m_files->addItem(f);
    }
    if (m_outputFolder.isEmpty() || m_outputFolder == QDir::homePath()) {
        m_outputFolder = defaultOutputFolder();
        m_folderLabel->setText(m_outputFolder);
    }
}

void BatchDialog::removeSelectedFile() {
    qDeleteAll(m_files->selectedItems());
    updateRunState();
}

void BatchDialog::addStep() {
    QMenu menu(this);
    for (BatchRunner::Op op : BatchRunner::allOps()) {
        QAction* a = menu.addAction(BatchRunner::opLabel(op));
        a->setData(static_cast<int>(op));
    }
    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen)
        return;
    const auto op = static_cast<BatchRunner::Op>(chosen->data().toInt());
    StepConfigDialog dialog(op, BatchRunner::defaultParams(op), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_stepData.append({op, dialog.params()});
    refreshSteps();
    m_steps->setCurrentRow(m_steps->count() - 1);
}

void BatchDialog::editSelectedStep() {
    const int row = m_steps->currentRow();
    if (row < 0 || row >= m_stepData.size())
        return;
    BatchRunner::Step& step = m_stepData[row];
    StepConfigDialog dialog(step.op, step.params, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    step.params = dialog.params();
    refreshSteps();
    m_steps->setCurrentRow(row);
}

void BatchDialog::removeSelectedStep() {
    const int row = m_steps->currentRow();
    if (row < 0 || row >= m_stepData.size())
        return;
    m_stepData.removeAt(row);
    refreshSteps();
    m_steps->setCurrentRow(qMin(row, m_steps->count() - 1));
}

void BatchDialog::moveStep(int delta) {
    const int row = m_steps->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= m_stepData.size())
        return;
    m_stepData.move(row, target);
    refreshSteps();
    m_steps->setCurrentRow(target);
}

void BatchDialog::saveActionToFile() {
    if (m_stepData.isEmpty()) {
        m_status->setText(tr("Add a step before saving an action."));
        return;
    }
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + QStringLiteral("/actions");
    QDir().mkpath(dir);
    QString path = QFileDialog::getSaveFileName(this, tr("Save action"),
                                                dir + QStringLiteral("/action.json"),
                                                tr("Feather actions (*.json)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        path += QStringLiteral(".json");
    QString err;
    if (BatchRunner::saveAction(path, m_stepData, &err))
        m_status->setText(tr("Saved action to %1").arg(QFileInfo(path).fileName()));
    else
        m_status->setText(err);
}

void BatchDialog::loadActionFromFile() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/actions");
    const QString path = QFileDialog::getOpenFileName(this, tr("Load action"), dir,
                                                      tr("Feather actions (*.json)"));
    if (path.isEmpty())
        return;
    QList<BatchRunner::Step> steps;
    QString err;
    if (!BatchRunner::loadAction(path, &steps, &err)) {
        m_status->setText(err);
        return;
    }
    m_stepData = steps;
    refreshSteps();
    m_status->setText(
        tr("Loaded %n step(s) from %1", "", int(steps.size())).arg(QFileInfo(path).fileName()));
}

void BatchDialog::chooseOutputFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Output folder"), m_outputFolder);
    if (dir.isEmpty())
        return;
    m_outputFolder = dir;
    m_folderLabel->setText(dir);
}

void BatchDialog::refreshSteps() {
    m_steps->clear();
    int i = 1;
    for (const BatchRunner::Step& step : m_stepData)
        m_steps->addItem(QStringLiteral("%1. %2").arg(i++).arg(BatchRunner::summarize(step)));
    updateRunState();
}

void BatchDialog::updateRunState() {
    if (m_run)
        m_run->setEnabled(!m_running && m_files->count() > 0 && !m_stepData.isEmpty());
}

void BatchDialog::run() {
    if (m_running || m_files->count() == 0 || m_stepData.isEmpty())
        return;
    if (m_outputFolder.isEmpty())
        m_outputFolder = defaultOutputFolder();

    const QString suffix = m_suffix->text();
    QDir outDir(m_outputFolder);

    m_running = true;
    updateRunState();
    m_progress->setRange(0, m_files->count());
    m_progress->setValue(0);
    m_progress->show();

    int ok = 0;
    QStringList failures;
    for (int i = 0; i < m_files->count(); ++i) {
        const QString input = m_files->item(i)->text();
        const QFileInfo info(input);
        const QString output =
            outDir.filePath(info.completeBaseName() + suffix + QStringLiteral(".pdf"));
        m_status->setText(tr("Processing %1…").arg(info.fileName()));
        QApplication::processEvents();

        QString error;
        if (BatchRunner::runFile(input, output, m_stepData, &error)) {
            ++ok;
        } else {
            failures.append(QStringLiteral("%1: %2").arg(info.fileName(), error));
        }
        m_progress->setValue(i + 1);
        QApplication::processEvents();
    }

    m_running = false;
    updateRunState();

    if (failures.isEmpty()) {
        m_status->setText(tr("Done, processed %n file(s) into %1.", "", ok)
                              .arg(QDir::toNativeSeparators(m_outputFolder)));
    } else {
        m_status->setText(tr("Done, %1 succeeded, %n failed:\n%2", "", failures.size())
                              .arg(ok)
                              .arg(failures.join(QStringLiteral("\n"))));
    }
}
