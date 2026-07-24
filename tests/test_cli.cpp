// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Integration test for the headless command-line interface: drives the real
// feather-pdf binary (path injected as FEATHERPDF_BIN) with sub-commands and
// checks exit codes and output. Covers the dispatcher, argument parsing, and a
// couple of representative operations end to end; the per-operation backends
// have their own unit tests.

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPageSize>
#include <QProcessEnvironment>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

class TestCli : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_pdf;

    struct Result {
        int code = -1;
        QString out;
        QString err;
    };
    Result run(const QStringList& args) {
        QProcess p;
        p.start(QStringLiteral(FEATHERPDF_BIN), args);
        if (!p.waitForFinished(30000))
            return {};
        return {p.exitCode(), QString::fromUtf8(p.readAllStandardOutput()),
                QString::fromUtf8(p.readAllStandardError())};
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath(QStringLiteral("two.pdf"));
        QPdfWriter w(m_pdf);
        w.setPageSize(QPageSize(QPageSize::A4));
        QPainter pa(&w);
        pa.drawText(100, 100, QStringLiteral("page one"));
        w.newPage();
        pa.drawText(100, 100, QStringLiteral("page two"));
        pa.end();
        QVERIFY(QFileInfo::exists(m_pdf));
    }

    void versionPrints() {
        const Result r = run({QStringLiteral("--version")});
        QCOMPARE(r.code, 0);
        QVERIFY2(r.out.contains(QStringLiteral("Feather PDF")), qPrintable(r.out));
    }

    void helpListsCommands() {
        const Result r = run({QStringLiteral("--help")});
        QCOMPARE(r.code, 0);
        QVERIFY(r.out.contains(QStringLiteral("merge")));
        QVERIFY(r.out.contains(QStringLiteral("optimize")));
    }

    void infoReportsPageCount() {
        const Result r = run({QStringLiteral("info"), m_pdf});
        QCOMPARE(r.code, 0);
        QVERIFY2(r.out.contains(QStringLiteral("Pages:     2")), qPrintable(r.out));
        QVERIFY(r.out.contains(QStringLiteral("Encrypted: no")));
    }

    void extractWritesSubset() {
        const QString out = m_dir.filePath(QStringLiteral("one.pdf"));
        const Result r =
            run({QStringLiteral("extract"), m_pdf, out, QStringLiteral("--pages"), QStringLiteral("1")});
        QCOMPARE(r.code, 0);
        QVERIFY(QFileInfo::exists(out));
        const Result info = run({QStringLiteral("info"), out});
        QVERIFY2(info.out.contains(QStringLiteral("Pages:     1")), qPrintable(info.out));
    }

    void thumbnailRendersSizedPng() {
        const QString png = m_dir.filePath(QStringLiteral("thumb.png"));
        const Result r = run({QStringLiteral("thumbnail"), m_pdf, png, QStringLiteral("--size"),
                              QStringLiteral("128")});
        QCOMPARE(r.code, 0);
        QVERIFY(QFileInfo::exists(png));
        const QImage img(png);
        QVERIFY(!img.isNull());
        // A4 portrait -> the longest edge lands exactly on the requested size.
        QCOMPARE(qMax(img.width(), img.height()), 128);
    }

    void batchRunsSavedAction() {
        const QString action = m_dir.filePath(QStringLiteral("act.json"));
        QFile f(action);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"feather-action":1,"steps":[{"op":"sanitize","params":{}}]})");
        f.close();
        const QString out = m_dir.filePath(QStringLiteral("batched.pdf"));
        const Result r = run({QStringLiteral("batch"), m_pdf, out, QStringLiteral("--action"), action});
        QCOMPARE(r.code, 0);
        QVERIFY2(QFileInfo::exists(out), qPrintable(r.err));
    }

    void watchOnceProcessesAndRetires() {
        QTemporaryDir in, out, done;
        QVERIFY(in.isValid() && out.isValid() && done.isValid());
        QVERIFY(QFile::copy(m_pdf, in.filePath(QStringLiteral("a.pdf"))));
        QVERIFY(QFile::copy(m_pdf, in.filePath(QStringLiteral("b.pdf"))));
        const QString action = m_dir.filePath(QStringLiteral("watch-act.json"));
        QFile f(action);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"feather-action":1,"steps":[{"op":"sanitize","params":{}}]})");
        f.close();

        const Result r = run({QStringLiteral("watch"), QStringLiteral("--once"),
                              QStringLiteral("--action"), action, QStringLiteral("--in"), in.path(),
                              QStringLiteral("--out"), out.path(), QStringLiteral("--done"),
                              done.path()});
        QCOMPARE(r.code, 0);
        QVERIFY2(QFileInfo::exists(out.filePath(QStringLiteral("a.pdf"))), qPrintable(r.err));
        QVERIFY(QFileInfo::exists(out.filePath(QStringLiteral("b.pdf"))));
        // Sources are retired into done/, so they aren't seen on the next pass.
        QVERIFY(!QFileInfo::exists(in.filePath(QStringLiteral("a.pdf"))));
        QVERIFY(QFileInfo::exists(done.filePath(QStringLiteral("a.pdf"))));
    }

    void mergeConcatenates() {
        const QString out = m_dir.filePath(QStringLiteral("four.pdf"));
        const Result r = run({QStringLiteral("merge"), out, m_pdf, m_pdf});
        QCOMPARE(r.code, 0);
        const Result info = run({QStringLiteral("info"), out});
        QVERIFY2(info.out.contains(QStringLiteral("Pages:     4")), qPrintable(info.out));
    }

    void encryptThenDecryptRoundTrips() {
        const QString enc = m_dir.filePath(QStringLiteral("enc.pdf"));
        const QString dec = m_dir.filePath(QStringLiteral("dec.pdf"));
        QCOMPARE(run({QStringLiteral("encrypt"), m_pdf, enc, QStringLiteral("--password"),
                      QStringLiteral("pw")})
                     .code,
                 0);
        QVERIFY(run({QStringLiteral("info"), enc}).out.contains(QStringLiteral("Encrypted: yes")));
        QCOMPARE(run({QStringLiteral("decrypt"), enc, dec, QStringLiteral("--password"),
                      QStringLiteral("pw")})
                     .code,
                 0);
        QVERIFY(run({QStringLiteral("info"), dec}).out.contains(QStringLiteral("Encrypted: no")));
    }

    void usageErrorsExitTwo() {
        // Missing required option.
        QCOMPARE(run({QStringLiteral("extract"), m_pdf, m_dir.filePath(QStringLiteral("x.pdf"))}).code,
                 2);
        // Out-of-range page.
        QCOMPARE(run({QStringLiteral("extract"), m_pdf, m_dir.filePath(QStringLiteral("x.pdf")),
                      QStringLiteral("--pages"), QStringLiteral("9")})
                     .code,
                 2);
        // Unknown sub-command flag style.
        QCOMPARE(run({QStringLiteral("optimize"), m_pdf}).code, 2); // missing output
    }
};

QTEST_MAIN(TestCli)
#include "test_cli.moc"
