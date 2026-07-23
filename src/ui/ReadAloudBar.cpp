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

#include "ui/ReadAloudBar.h"

#include "ui/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QProcess>
#include <QSlider>
#include <QStandardPaths>
#include <QToolButton>

namespace {
constexpr char kSpdSay[] = "spd-say";
} // namespace

ReadAloudBar::ReadAloudBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("ReadAloudBar"));
    setFixedHeight(44);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(12, 6, 12, 6);
    row->setSpacing(6);

    auto* icon = new QLabel(this);
    icon->setObjectName(QStringLiteral("ReadAloudIcon"));
    icon->setFixedSize(18, 18);
    row->addWidget(icon);

    m_playPause = addButton(QStringLiteral("pause"), tr("Pause"));
    connect(m_playPause, &QToolButton::clicked, this, &ReadAloudBar::togglePlayPause);
    row->addWidget(m_playPause);

    m_stop = addButton(QStringLiteral("stop"), tr("Stop"));
    connect(m_stop, &QToolButton::clicked, this, [this] {
        cancelSpeech();
        m_index = 0;
        setState(State::Stopped);
        updateStatus();
    });
    row->addWidget(m_stop);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("ReadAloudStatus"));
    m_status->setMinimumWidth(140);
    row->addWidget(m_status);

    row->addStretch(1);

    auto* speedLabel = new QLabel(tr("Speed"), this);
    speedLabel->setObjectName(QStringLiteral("ReadAloudCaption"));
    row->addWidget(speedLabel);
    m_speed = new QSlider(Qt::Horizontal, this);
    m_speed->setObjectName(QStringLiteral("ReadAloudSpeed"));
    m_speed->setRange(-90, 90); // spd-say --rate
    m_speed->setValue(0);
    m_speed->setFixedWidth(120);
    m_speed->setToolTip(tr("Reading speed"));
    row->addWidget(m_speed);

    m_language = new QComboBox(this);
    m_language->setObjectName(QStringLiteral("ReadAloudLang"));
    m_language->addItem(tr("Auto"), QString());
    m_language->addItem(tr("English"), QStringLiteral("en"));
    m_language->addItem(tr("Indonesian"), QStringLiteral("id"));
    m_language->setToolTip(tr("Voice language"));
    row->addWidget(m_language);

    row->addSpacing(8);
    auto* close = addButton(QStringLiteral("x"), tr("Close"));
    connect(close, &QToolButton::clicked, this, [this] {
        cancelSpeech();
        setState(State::Stopped);
        hide();
        emit closed();
    });
    row->addWidget(close);

    refreshIcons();
    connect(&Theme::instance(), &Theme::changed, this, &ReadAloudBar::refreshIcons);
    updateStatus();
}

bool ReadAloudBar::isAvailable() {
    return !QStandardPaths::findExecutable(QString::fromLatin1(kSpdSay)).isEmpty();
}

QToolButton* ReadAloudBar::addButton(const QString& iconName, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(28, 28);
    b->setIconSize(QSize(16, 16));
    b->setAutoRaise(true);
    m_iconButtons.append({b, iconName});
    return b;
}

void ReadAloudBar::refreshIcons() {
    const auto& pal = Theme::instance().palette();
    for (const auto& [btn, name] : m_iconButtons)
        btn->setIcon(Theme::instance().icon(name, pal.text));
    if (auto* icon = findChild<QLabel*>(QStringLiteral("ReadAloudIcon")))
        icon->setPixmap(Theme::instance().icon(QStringLiteral("volume-2"), pal.dim).pixmap(16, 16));
}

int ReadAloudBar::start(const QString& docPath, int startPage) {
    cancelSpeech();

    QString error;
    m_utterances = ReadAloud::utterances(docPath, &error);
    if (m_utterances.isEmpty()) {
        m_index = 0;
        setState(State::Stopped);
        updateStatus();
        return 0;
    }

    // Begin at the first sentence on or after the page the user is viewing.
    m_index = 0;
    for (int i = 0; i < m_utterances.size(); ++i) {
        if (m_utterances[i].page >= startPage) {
            m_index = i;
            break;
        }
    }

    show();
    setState(State::Playing);
    speakCurrent();
    return m_utterances.size();
}

void ReadAloudBar::speakCurrent() {
    if (m_index < 0 || m_index >= m_utterances.size()) {
        setState(State::Stopped);
        updateStatus();
        return;
    }

    const ReadAloud::Utterance& u = m_utterances[m_index];

    QStringList args{QStringLiteral("-w"), QStringLiteral("-r"),
                     QString::number(m_speed->value())};
    const QString lang = m_language->currentData().toString();
    if (!lang.isEmpty())
        args << QStringLiteral("-l") << lang;
    args << QStringLiteral("--") << u.text;

    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished, this, &ReadAloudBar::onProcessFinished);
    m_proc->start(QString::fromLatin1(kSpdSay), args);

    emit pageReached(u.page);
    updateStatus();
}

void ReadAloudBar::onProcessFinished() {
    if (auto* p = qobject_cast<QProcess*>(sender()))
        p->deleteLater();
    if (m_proc == sender())
        m_proc = nullptr;

    if (m_state != State::Playing)
        return; // paused or stopped between sentences

    ++m_index;
    if (m_index >= m_utterances.size()) {
        setState(State::Stopped);
        m_index = m_utterances.size(); // park at the end → next play restarts
        updateStatus();
        return;
    }
    speakCurrent();
}

void ReadAloudBar::cancelSpeech() {
    if (m_proc) {
        disconnect(m_proc, nullptr, this, nullptr); // don't let kill() advance us
        m_proc->kill();
        m_proc->waitForFinished(200);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    // Tell the daemon to drop anything still queued/speaking.
    if (isAvailable())
        QProcess::startDetached(QString::fromLatin1(kSpdSay), {QStringLiteral("-C")});
}

void ReadAloudBar::togglePlayPause() {
    if (m_utterances.isEmpty())
        return;
    if (m_state == State::Playing) {
        setState(State::Paused);
        cancelSpeech(); // stop the current sentence; we'll re-speak it on resume
        return;
    }
    // Paused or finished → (re)start.
    if (m_index >= m_utterances.size())
        m_index = 0;
    setState(State::Playing);
    speakCurrent();
}

void ReadAloudBar::restart() {
    cancelSpeech();
    m_index = 0;
    setState(State::Playing);
    speakCurrent();
}

void ReadAloudBar::stopAndHide() {
    cancelSpeech();
    setState(State::Stopped);
    hide();
}

void ReadAloudBar::setState(State s) {
    m_state = s;
    const bool playing = s == State::Playing;
    // The play/pause button is the inverse of the state: shows Pause while playing.
    if (!m_iconButtons.isEmpty() && m_iconButtons.first().first == m_playPause) {
        m_iconButtons.first().second = playing ? QStringLiteral("pause") : QStringLiteral("play");
        m_playPause->setToolTip(playing ? tr("Pause") : tr("Play"));
        m_playPause->setAccessibleName(m_playPause->toolTip());
        refreshIcons();
    }
    m_stop->setEnabled(s != State::Stopped);
}

void ReadAloudBar::updateStatus() {
    if (m_utterances.isEmpty()) {
        m_status->setText(tr("Nothing to read"));
        return;
    }
    const int n = m_utterances.size();
    const int shown = qMin(m_index + 1, n);
    const int page = m_utterances[qBound(0, m_index, n - 1)].page + 1;
    if (m_state == State::Stopped && m_index >= n)
        m_status->setText(tr("Finished"));
    else
        m_status->setText(tr("Sentence %1 of %2  ·  page %3").arg(shown).arg(n).arg(page));
}

void ReadAloudBar::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        cancelSpeech();
        setState(State::Stopped);
        hide();
        emit closed();
        return;
    }
    QWidget::keyPressEvent(event);
}
