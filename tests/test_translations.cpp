// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Validates the Indonesian translation catalog: known strings are translated,
// coverage stays high, and no translation drops a %1/%2/%n placeholder that its
// source carries (which would crash or mis-format at runtime).

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QXmlStreamReader>
#include <QtTest>

namespace {
QSet<QString> placeholders(const QString& s) {
    QSet<QString> out;
    static const QRegularExpression re(QStringLiteral("%([0-9]|n)"));
    auto it = re.globalMatch(s);
    while (it.hasNext())
        out.insert(it.next().captured(0));
    return out;
}

struct Entry {
    QString translation;
    bool finished = false;
    bool numerus = false;
};
}

class TestTranslations : public QObject {
    Q_OBJECT

private:
    QHash<QString, Entry> m_map;

private slots:
    void initTestCase() {
        QFile f(QStringLiteral(TRANSLATIONS_TS));
        QVERIFY2(f.open(QIODevice::ReadOnly), TRANSLATIONS_TS);
        QXmlStreamReader xml(&f);
        QString source;
        Entry cur;
        bool inSource = false, inTranslation = false;
        QString translationText;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                const QStringView n = xml.name();
                if (n == QLatin1String("message")) {
                    source.clear();
                    cur = Entry{};
                    cur.numerus = xml.attributes().value("numerus") == QLatin1String("yes");
                    translationText.clear();
                } else if (n == QLatin1String("source")) {
                    inSource = true;
                } else if (n == QLatin1String("translation")) {
                    inTranslation = true;
                    cur.finished = xml.attributes().value("type") != QLatin1String("unfinished");
                } else if (n == QLatin1String("numerusform")) {
                    inTranslation = true;
                }
            } else if (xml.isCharacters()) {
                if (inSource)
                    source += xml.text().toString();
                else if (inTranslation)
                    translationText += xml.text().toString();
            } else if (xml.isEndElement()) {
                const QStringView n = xml.name();
                if (n == QLatin1String("source"))
                    inSource = false;
                else if (n == QLatin1String("translation"))
                    inTranslation = false;
                else if (n == QLatin1String("numerusform"))
                    inTranslation = false;
                else if (n == QLatin1String("message")) {
                    cur.translation = translationText;
                    if (!source.isEmpty())
                        m_map.insert(source, cur);
                }
            }
        }
        QVERIFY(!xml.hasError());
        QVERIFY(m_map.size() > 400);
    }

    void knownStringsAreTranslated() {
        const QHash<QString, QString> expect{
            {QStringLiteral("Save"), QStringLiteral("Simpan")},
            {QStringLiteral("Highlight"), QStringLiteral("Sorot")},
            {QStringLiteral("Find"), QStringLiteral("Cari")},
            {QStringLiteral("Preferences"), QStringLiteral("Preferensi")},
            {QStringLiteral("Language"), QStringLiteral("Bahasa")},
            {QStringLiteral("Remove Hidden Information"), QStringLiteral("Buang Info Tersembunyi")},
        };
        for (auto it = expect.constBegin(); it != expect.constEnd(); ++it) {
            QVERIFY2(m_map.contains(it.key()), qPrintable(it.key()));
            QCOMPARE(m_map.value(it.key()).translation, it.value());
        }
    }

    void coverageIsHigh() {
        int finished = 0;
        for (const Entry& e : m_map)
            if (e.finished && !e.translation.isEmpty())
                ++finished;
        // ~588 of 617 unique sources are translated; the rest are language-neutral
        // (file filters, %1/%2 formats, URLs). Guard against a big regression.
        QVERIFY2(finished >= 560, qPrintable(QStringLiteral("only %1 finished").arg(finished)));
    }

    void placeholdersArePreserved() {
        for (auto it = m_map.constBegin(); it != m_map.constEnd(); ++it) {
            const Entry& e = it.value();
            if (!e.finished || e.translation.isEmpty())
                continue;
            const QSet<QString> want = placeholders(it.key());
            const QSet<QString> got = placeholders(e.translation);
            QVERIFY2(want == got,
                     qPrintable(QStringLiteral("placeholder mismatch for \"%1\" -> \"%2\"")
                                    .arg(it.key(), e.translation)));
        }
    }
};

QTEST_GUILESS_MAIN(TestTranslations)
#include "test_translations.moc"
