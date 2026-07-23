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

    // Drive the file-manager helper script. It calls `feather-pdf` by name, so
    // put the freshly built binary's directory on PATH for the child process.
    Result runHelper(const QStringList& args) {
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString binDir = QFileInfo(QStringLiteral(FEATHERPDF_BIN)).absolutePath();
        env.insert(QStringLiteral("PATH"),
                   binDir + QLatin1Char(':') + env.value(QStringLiteral("PATH")));
        p.setProcessEnvironment(env);
        p.start(QStringLiteral(FEATHERPDF_ACTION), args);
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

    // Drive the CUPS backend script. FEATHER_PRINT_DIR redirects output to a
    // temp dir and FEATHER_PRINT_OPEN=0 skips the best-effort session launch.
    Result runBackend(const QStringList& args, const QString& dir,
                      const QString& stdinFile = QString()) {
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("FEATHER_PRINT_DIR"), dir);
        env.insert(QStringLiteral("FEATHER_PRINT_OPEN"), QStringLiteral("0"));
        p.setProcessEnvironment(env);
        if (!stdinFile.isEmpty())
            p.setStandardInputFile(stdinFile);
        p.start(QStringLiteral(FEATHERPDF_CUPS_BACKEND), args);
        if (!p.waitForFinished(30000))
            return {};
        return {p.exitCode(), QString::fromUtf8(p.readAllStandardOutput()),
                QString::fromUtf8(p.readAllStandardError())};
    }

    void printBackendAdvertisesDevice() {
        QProcess p;
        p.start(QStringLiteral(FEATHERPDF_CUPS_BACKEND), QStringList{});
        QVERIFY(p.waitForFinished(10000));
        QCOMPARE(p.exitCode(), 0);
        const QString out = QString::fromUtf8(p.readAllStandardOutput());
        QVERIFY2(out.contains(QStringLiteral("feather-pdf-print:/")), qPrintable(out));
    }

    void printBackendSavesJob() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // CUPS args: job-id user title copies options ; document on stdin.
        const Result r = runBackend({QStringLiteral("77"), QStringLiteral("tester"),
                                     QStringLiteral("Quarterly Report"), QStringLiteral("1"),
                                     QString()},
                                    dir.path(), m_pdf);
        QCOMPARE(r.code, 0);
        const QString saved = dir.filePath(QStringLiteral("Quarterly_Report-77.pdf"));
        QVERIFY2(QFileInfo::exists(saved), qPrintable(r.err));
        // The saved file is a byte-for-byte copy of what we "printed".
        QFile in(m_pdf), out(saved);
        QVERIFY(in.open(QIODevice::ReadOnly) && out.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), out.readAll());
    }

    void actionHelperWritesBesideSource() {
        // Work on a copy so the output name is predictable next to it.
        const QString src = m_dir.filePath(QStringLiteral("action-src.pdf"));
        QVERIFY(QFile::copy(m_pdf, src));
        const Result r = runHelper({QStringLiteral("compress"), src});
        QCOMPARE(r.code, 0);
        QVERIFY2(QFileInfo::exists(m_dir.filePath(QStringLiteral("action-src-compressed.pdf"))),
                 qPrintable(r.err));
    }

    void actionHelperRejectsBadArgs() {
        QCOMPARE(runHelper({}).code, 2);                                  // no action, no file
        QCOMPARE(runHelper({QStringLiteral("bogus"), m_pdf}).code, 2);    // unknown action
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
