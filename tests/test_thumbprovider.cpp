// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Drives the Explorer thumbnail provider (feather-thumb.dll) the way the
// shell does: DllGetClassObject → IClassFactory → IInitializeWithStream on a
// file stream → IThumbnailProvider::GetThumbnail, all from an STA thread like
// Explorer's. The DLL renders with Windows.Data.Pdf; on stripped-down hosts
// where that runtime class is missing the test skips instead of failing.

#include <QFileInfo>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include <objbase.h>
#include <propsys.h>
#include <shlwapi.h>
#include <thumbcache.h>

class TestThumbProvider : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_pdf;
    HMODULE m_dll = nullptr;

    using GetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);

    // Must match kClsid in src/shellext/ThumbnailProvider.cpp.
    static constexpr CLSID kClsid = {
        0xC6DD57D7, 0x9B9D, 0x45BE, {0x98, 0x81, 0xAA, 0xCF, 0x7F, 0x84, 0x2E, 0x7A}};

    IThumbnailProvider* makeProvider() {
        auto getClassObject = reinterpret_cast<GetClassObjectFn>(
            GetProcAddress(m_dll, "DllGetClassObject"));
        if (!getClassObject)
            return nullptr;
        IClassFactory* factory = nullptr;
        if (FAILED(getClassObject(kClsid, IID_PPV_ARGS(&factory))))
            return nullptr;
        IThumbnailProvider* provider = nullptr;
        const HRESULT hr = factory->CreateInstance(nullptr, IID_PPV_ARGS(&provider));
        factory->Release();
        return SUCCEEDED(hr) ? provider : nullptr;
    }

    bool initializeFromFile(IThumbnailProvider* provider, const QString& path,
                            HRESULT* outHr = nullptr) {
        IInitializeWithStream* init = nullptr;
        if (FAILED(provider->QueryInterface(IID_PPV_ARGS(&init))))
            return false;
        IStream* stream = nullptr;
        HRESULT hr = SHCreateStreamOnFileEx(reinterpret_cast<const wchar_t*>(path.utf16()),
                                            STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE,
                                            nullptr, &stream);
        if (SUCCEEDED(hr)) {
            hr = init->Initialize(stream, STGM_READ);
            stream->Release();
        }
        init->Release();
        if (outHr)
            *outHr = hr;
        return SUCCEEDED(hr);
    }

private slots:
    void initTestCase() {
        // Explorer calls thumbnail providers from an STA; do the same so the
        // provider's worker-thread design is what's actually under test.
        QCOMPARE(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), S_OK);

        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath(QStringLiteral("sample.pdf"));
        QPdfWriter w(m_pdf);
        w.setPageSize(QPageSize(QPageSize::A4));
        QPainter pa(&w);
        pa.fillRect(QRect(0, 0, w.width(), w.height()), Qt::darkBlue);
        pa.setPen(Qt::white);
        pa.drawText(100, 1000, QStringLiteral("thumbnail me"));
        pa.end();
        QVERIFY(QFileInfo::exists(m_pdf));

        m_dll = LoadLibraryW(reinterpret_cast<const wchar_t*>(
            QStringLiteral(FEATHER_THUMB_DLL).utf16()));
        QVERIFY2(m_dll, "feather-thumb.dll failed to load");
    }

    void cleanupTestCase() {
        if (m_dll)
            FreeLibrary(m_dll);
        CoUninitialize();
    }

    void rendersPageOneAtRequestedSize() {
        IThumbnailProvider* provider = makeProvider();
        QVERIFY(provider);
        QVERIFY(initializeFromFile(provider, m_pdf));

        HBITMAP bmp = nullptr;
        WTS_ALPHATYPE alpha = WTSAT_UNKNOWN;
        const HRESULT hr = provider->GetThumbnail(128, &bmp, &alpha);
        provider->Release();
        if (hr == REGDB_E_CLASSNOTREG || hr == CLASS_E_CLASSNOTAVAILABLE)
            QSKIP("Windows.Data.Pdf is not available on this host");
        QVERIFY2(SUCCEEDED(hr), qPrintable(QStringLiteral("GetThumbnail: 0x%1")
                                               .arg(ulong(hr), 8, 16, QLatin1Char('0'))));
        QVERIFY(bmp);
        QCOMPARE(alpha, WTSAT_ARGB);

        // A4 portrait: the long edge gets the requested 128px, the short edge
        // follows the 210:297 aspect ratio.
        BITMAP info = {};
        QVERIFY(GetObjectW(bmp, sizeof(info), &info) > 0);
        QCOMPARE(int(info.bmHeight), 128);
        QVERIFY2(info.bmWidth >= 88 && info.bmWidth <= 93,
                 qPrintable(QStringLiteral("width %1").arg(info.bmWidth)));
        QCOMPARE(int(info.bmBitsPixel), 32);

        // The page was painted dark blue on white — the DIB must not be blank
        // or transparent.
        QVERIFY(info.bmBits);
        const quint32 center = reinterpret_cast<const quint32*>(
            info.bmBits)[(info.bmHeight / 2) * info.bmWidth + info.bmWidth / 2];
        QVERIFY2((center >> 24) == 0xFF, "center pixel should be opaque");
        QVERIFY2(center != 0xFFFFFFFF, "center pixel should not be white");

        DeleteObject(bmp);
    }

    void rejectsSecondInitialize() {
        IThumbnailProvider* provider = makeProvider();
        QVERIFY(provider);
        QVERIFY(initializeFromFile(provider, m_pdf));
        HRESULT hr = S_OK;
        QVERIFY(!initializeFromFile(provider, m_pdf, &hr));
        QCOMPARE(hr, HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED));
        provider->Release();
    }

    void failsCleanlyOnGarbage() {
        const QString junk = m_dir.filePath(QStringLiteral("junk.pdf"));
        {
            QFile f(junk);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("this is not a pdf at all");
        }
        IThumbnailProvider* provider = makeProvider();
        QVERIFY(provider);
        QVERIFY(initializeFromFile(provider, junk));
        HBITMAP bmp = nullptr;
        WTS_ALPHATYPE alpha = WTSAT_UNKNOWN;
        const HRESULT hr = provider->GetThumbnail(128, &bmp, &alpha);
        provider->Release();
        if (hr == REGDB_E_CLASSNOTREG || hr == CLASS_E_CLASSNOTAVAILABLE)
            QSKIP("Windows.Data.Pdf is not available on this host");
        QVERIFY(FAILED(hr));
        QVERIFY(!bmp);
    }
};

QTEST_MAIN(TestThumbProvider)
#include "test_thumbprovider.moc"
