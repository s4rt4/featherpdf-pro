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

#include <QList>
#include <QVector>
#include <QWidget>

#include "backends/ReadAloud.h"

class QComboBox;
class QLabel;
class QProcess;
class QSlider;
class QToolButton;

// The Read-aloud bar — a slim strip beneath the command toolbar that speaks the
// document one sentence at a time via speech-dispatcher (the `spd-say` command).
// Play/Pause/Stop, a speed slider, and a language picker; a live "sentence n of m"
// status. Each sentence is one `spd-say -w` process, so playback advances when a
// process finishes; pause/resume and stop work between sentences. It emits
// pageReached() so the viewport can follow along. Esc (or ✕) closes it.
class ReadAloudBar : public QWidget {
    Q_OBJECT

public:
    explicit ReadAloudBar(QWidget* parent = nullptr);

    // True if speech-dispatcher's `spd-say` is on PATH (the only runtime need).
    static bool isAvailable();

    // Load `docPath`, show the bar, and start speaking from the top. `startPage`
    // (0-based) begins at the first sentence on or after that page. Returns the
    // number of utterances found (0 → nothing speakable; caller may toast).
    int start(const QString& docPath, int startPage = 0);

    void stopAndHide(); // stop speech, hide the bar (no closed() emitted)

signals:
    void pageReached(int page); // the page of the sentence now being spoken
    void closed();              // ✕ or Esc

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class State { Stopped, Playing, Paused };

    QToolButton* addButton(const QString& iconName, const QString& tip);
    void refreshIcons();
    void speakCurrent();      // launch spd-say for m_index
    void cancelSpeech();      // kill our process + cancel any queued speech
    void onProcessFinished(); // advance to the next sentence
    void setState(State s);
    void updateStatus();
    void togglePlayPause();
    void restart(); // back to the first sentence and play

    QList<ReadAloud::Utterance> m_utterances;
    int m_index = 0;
    State m_state = State::Stopped;
    QProcess* m_proc = nullptr;

    QToolButton* m_playPause = nullptr;
    QToolButton* m_stop = nullptr;
    QSlider* m_speed = nullptr;
    QComboBox* m_language = nullptr;
    QLabel* m_status = nullptr;
    QVector<QPair<QToolButton*, QString>> m_iconButtons;
};
