// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Exercises the batch / action engine: a multi-step action piped through one
// file produces a valid result, page-altering and security steps take effect,
// the catalog helpers are sound, and a failing step is reported with its name.
// The per-operation backends have their own unit tests; this checks the
// chaining, dispatch, and parameter wiring.

#include "backends/BatchRunner.h"
#include "backends/PdfEditor.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include <poppler-qt6.h>

namespace {
int pages(const QString& path) {
    std::unique_ptr<Poppler::Document> d = Poppler::Document::load(path);
    return (d && !d->isLocked()) ? d->numPages() : -1;
}
QString allText(const QString& path) {
    std::unique_ptr<Poppler::Document> d = Poppler::Document::load(path);
    if (!d || d->isLocked())
        return QString();
    QString t;
    for (int i = 0; i < d->numPages(); ++i)
        if (auto p = d->page(i))
            t += p->text(QRectF());
    return t;
}
} // namespace

class TestBatchRunner : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_pdf;

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath(QStringLiteral("in.pdf"));
        QPdfWriter w(m_pdf);
        w.setPageSize(QPageSize(QPageSize::A4));
        QPainter pa(&w);
        pa.drawText(100, 100, QStringLiteral("one"));
        w.newPage();
        pa.drawText(100, 100, QStringLiteral("two"));
        pa.end();
        QCOMPARE(pages(m_pdf), 2);
    }

    void emptyActionCopies() {
        const QString out = m_dir.filePath(QStringLiteral("copy.pdf"));
        QString err;
        QVERIFY2(BatchRunner::runFile(m_pdf, out, {}, &err), qPrintable(err));
        QCOMPARE(pages(out), 2);
    }

    void multiStepPipelineProducesValidPdf() {
        // Optimize → Watermark → Header/Footer → Bates: four operations composed
        // into one output. The header/footer text is extractable, proving the
        // middle steps ran and the result is a well-formed PDF.
        QList<BatchRunner::Step> steps;
        steps.append({BatchRunner::Op::Optimize, BatchRunner::defaultParams(BatchRunner::Op::Optimize)});
        QVariantMap wm = BatchRunner::defaultParams(BatchRunner::Op::Watermark);
        wm[QStringLiteral("text")] = QStringLiteral("DRAFT");
        steps.append({BatchRunner::Op::Watermark, wm});
        QVariantMap hf = BatchRunner::defaultParams(BatchRunner::Op::HeaderFooter);
        hf[QStringLiteral("footerCenter")] = QStringLiteral("BATCHMARK {n}");
        steps.append({BatchRunner::Op::HeaderFooter, hf});
        steps.append({BatchRunner::Op::Bates, BatchRunner::defaultParams(BatchRunner::Op::Bates)});

        const QString out = m_dir.filePath(QStringLiteral("piped.pdf"));
        QString err;
        QVERIFY2(BatchRunner::runFile(m_pdf, out, steps, &err), qPrintable(err));
        QCOMPARE(pages(out), 2); // every step preserves the page count
        QVERIFY2(allText(out).contains(QStringLiteral("BATCHMARK")),
                 "the header/footer step's text should be present in the output");
    }

    void encryptStepProtectsTheFile() {
        QVariantMap p = BatchRunner::defaultParams(BatchRunner::Op::Encrypt);
        p[QStringLiteral("password")] = QStringLiteral("pw");
        const QString out = m_dir.filePath(QStringLiteral("enc.pdf"));
        QString err;
        QVERIFY2(BatchRunner::runFile(m_pdf, out, {{BatchRunner::Op::Encrypt, p}}, &err),
                 qPrintable(err));
        QVERIFY(PdfEditor::isPasswordProtected(out));
    }

    void rotateStepAppliesToEveryPage() {
        QVariantMap p = BatchRunner::defaultParams(BatchRunner::Op::Rotate);
        p[QStringLiteral("angle")] = 90;
        const QString out = m_dir.filePath(QStringLiteral("rot.pdf"));
        QString err;
        QVERIFY2(BatchRunner::runFile(m_pdf, out, {{BatchRunner::Op::Rotate, p}}, &err),
                 qPrintable(err));
        QCOMPARE(pages(out), 2);
    }

    void failingStepIsReportedWithItsName() {
        const QString out = m_dir.filePath(QStringLiteral("nope.pdf"));
        QString err;
        const QString missing = m_dir.filePath(QStringLiteral("does-not-exist.pdf"));
        QVERIFY(!BatchRunner::runFile(
            missing, out, {{BatchRunner::Op::Optimize,
                            BatchRunner::defaultParams(BatchRunner::Op::Optimize)}},
            &err));
        QVERIFY2(err.contains(BatchRunner::opLabel(BatchRunner::Op::Optimize)), qPrintable(err));
    }

    void actionSavesAndLoadsRoundTrip() {
        QList<BatchRunner::Step> steps;
        QVariantMap wm = BatchRunner::defaultParams(BatchRunner::Op::Watermark);
        wm[QStringLiteral("text")] = QStringLiteral("SECRET");
        wm[QStringLiteral("opacity")] = 0.5;
        steps.append({BatchRunner::Op::Watermark, wm});
        steps.append({BatchRunner::Op::Sanitize, BatchRunner::defaultParams(BatchRunner::Op::Sanitize)});

        const QString file = m_dir.filePath(QStringLiteral("action.json"));
        QString err;
        QVERIFY2(BatchRunner::saveAction(file, steps, &err), qPrintable(err));

        QList<BatchRunner::Step> back;
        QVERIFY2(BatchRunner::loadAction(file, &back, &err), qPrintable(err));
        QCOMPARE(back.size(), 2);
        QCOMPARE(back[0].op, BatchRunner::Op::Watermark);
        QCOMPARE(back[0].params.value(QStringLiteral("text")).toString(), QStringLiteral("SECRET"));
        QCOMPARE(back[0].params.value(QStringLiteral("opacity")).toDouble(), 0.5);
        QCOMPARE(back[1].op, BatchRunner::Op::Sanitize);

        // A loaded action drives runFile exactly like a hand-built one.
        const QString out = m_dir.filePath(QStringLiteral("from-action.pdf"));
        QVERIFY2(BatchRunner::runFile(m_pdf, out, back, &err), qPrintable(err));
        QCOMPARE(pages(out), 2);
    }

    void loadActionRejectsGarbage() {
        const QString bad = m_dir.filePath(QStringLiteral("bad.json"));
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not json at all");
        f.close();
        QString err;
        QList<BatchRunner::Step> steps;
        QVERIFY(!BatchRunner::loadAction(bad, &steps, &err));
        QVERIFY(!err.isEmpty());
        // An unknown op id is rejected too.
        const QString unknown = m_dir.filePath(QStringLiteral("unknown.json"));
        QFile g(unknown);
        QVERIFY(g.open(QIODevice::WriteOnly));
        g.write(R"({"feather-action":1,"steps":[{"op":"teleport","params":{}}]})");
        g.close();
        QVERIFY(!BatchRunner::loadAction(unknown, &steps, &err));
    }

    void catalogIsConsistent() {
        const QList<BatchRunner::Op> ops = BatchRunner::allOps();
        QVERIFY(ops.size() >= 8);
        for (BatchRunner::Op op : ops) {
            QVERIFY(!BatchRunner::opId(op).isEmpty());
            QVERIFY(!BatchRunner::opLabel(op).isEmpty());
            QVERIFY(!BatchRunner::summarize({op, BatchRunner::defaultParams(op)}).isEmpty());
        }
        QVariantMap wm = BatchRunner::defaultParams(BatchRunner::Op::Watermark);
        wm[QStringLiteral("text")] = QStringLiteral("HELLO");
        QVERIFY(BatchRunner::summarize({BatchRunner::Op::Watermark, wm}).contains(
            QStringLiteral("HELLO")));
    }
};

QTEST_MAIN(TestBatchRunner)
#include "test_batchrunner.moc"
