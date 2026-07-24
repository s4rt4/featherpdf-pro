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

#include "backends/Scanner.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QObject>

#include <windows.h>

#include <objbase.h>
#include <shlwapi.h>
#include <sti.h>
#include <wia_lh.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

// WIA wants an apartment; the GUI thread already has one (Qt initializes OLE),
// in which case CoInitializeEx returns S_FALSE and this is a cheap no-op.
struct ComApartment {
    HRESULT hr;
    ComApartment() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComApartment() {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
    bool ok() const { return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE; }
};

QString fromBstr(BSTR b) {
    return b ? QString::fromWCharArray(b) : QString();
}

QString hrMessage(HRESULT hr) {
    return QStringLiteral("0x%1").arg(quint32(hr), 8, 16, QLatin1Char('0'));
}

QString readString(IWiaPropertyStorage* store, PROPID id) {
    PROPSPEC spec{PRSPEC_PROPID, {id}};
    PROPVARIANT v;
    PropVariantInit(&v);
    QString out;
    if (store && SUCCEEDED(store->ReadMultiple(1, &spec, &v)) && v.vt == VT_BSTR)
        out = fromBstr(v.bstrVal);
    PropVariantClear(&v);
    return out;
}

int readInt(IWiaPropertyStorage* store, PROPID id, int fallback) {
    PROPSPEC spec{PRSPEC_PROPID, {id}};
    PROPVARIANT v;
    PropVariantInit(&v);
    int out = fallback;
    if (store && SUCCEEDED(store->ReadMultiple(1, &spec, &v)) && v.vt == VT_I4)
        out = v.lVal;
    PropVariantClear(&v);
    return out;
}

GUID readGuid(IWiaPropertyStorage* store, PROPID id) {
    PROPSPEC spec{PRSPEC_PROPID, {id}};
    PROPVARIANT v;
    PropVariantInit(&v);
    GUID out = GUID_NULL;
    if (store && SUCCEEDED(store->ReadMultiple(1, &spec, &v)) && v.vt == VT_CLSID && v.puuid)
        out = *v.puuid;
    PropVariantClear(&v);
    return out;
}

bool writeInt(IWiaPropertyStorage* store, PROPID id, LONG value) {
    PROPSPEC spec{PRSPEC_PROPID, {id}};
    PROPVARIANT v;
    PropVariantInit(&v);
    v.vt = VT_I4;
    v.lVal = value;
    return store && SUCCEEDED(store->WriteMultiple(1, &spec, &v, WIA_IPA_FIRST));
}

bool writeGuid(IWiaPropertyStorage* store, PROPID id, const GUID& guid) {
    PROPSPEC spec{PRSPEC_PROPID, {id}};
    GUID copy = guid;
    PROPVARIANT v;
    PropVariantInit(&v);
    v.vt = VT_CLSID;
    v.puuid = &copy;
    return store && SUCCEEDED(store->WriteMultiple(1, &spec, &v, WIA_IPA_FIRST));
}

ComPtr<IWiaDevMgr2> deviceManager() {
    ComPtr<IWiaDevMgr2> mgr;
    CoCreateInstance(CLSID_WiaDevMgr2, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&mgr));
    return mgr;
}

// Receives the pages of one Download call as BMP files scan-N.bmp in `dir`.
class FileTransferCallback : public IWiaTransferCallback {
public:
    FileTransferCallback(const QString& dir, int startIndex)
        : m_dir(dir), m_index(startIndex) {}

    QStringList files() const { return m_files; }
    int nextIndex() const { return m_index; }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
        if (!out)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IWiaTransferCallback) {
            *out = static_cast<IWiaTransferCallback*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ULONG(InterlockedIncrement(&m_ref)); }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG n = ULONG(InterlockedDecrement(&m_ref));
        if (n == 0)
            delete this;
        return n;
    }

    // IWiaTransferCallback
    HRESULT STDMETHODCALLTYPE TransferCallback(LONG, WiaTransferParams*) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetNextStream(LONG, BSTR, BSTR, IStream** stream) override {
        if (!stream)
            return E_POINTER;
        const QString path =
            QDir(m_dir).filePath(QStringLiteral("scan-%1.bmp").arg(++m_index));
        const HRESULT hr = SHCreateStreamOnFileEx(
            reinterpret_cast<const wchar_t*>(path.utf16()),
            STGM_CREATE | STGM_READWRITE | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, TRUE,
            nullptr, stream);
        if (SUCCEEDED(hr))
            m_files << path;
        return hr;
    }

private:
    virtual ~FileTransferCallback() = default;
    LONG m_ref = 1;
    QString m_dir;
    int m_index = 0;
    QStringList m_files;
};

struct EnumeratedDevice {
    Scanner::Device info;
    int stiType = 0;
};

QList<EnumeratedDevice> enumerate(IWiaDevMgr2* mgr, QString* error) {
    QList<EnumeratedDevice> out;
    ComPtr<IEnumWIA_DEV_INFO> it;
    const HRESULT hr = mgr->EnumDeviceInfo(WIA_DEVINFO_ENUM_LOCAL, &it);
    if (FAILED(hr)) {
        if (error)
            *error = QObject::tr("Listing scanners failed (%1).").arg(hrMessage(hr));
        return out;
    }
    for (;;) {
        ComPtr<IWiaPropertyStorage> props;
        ULONG fetched = 0;
        if (it->Next(1, &props, &fetched) != S_OK || fetched == 0)
            break;
        EnumeratedDevice d;
        d.info.name = readString(props.Get(), WIA_DIP_DEV_ID);
        d.info.vendor = readString(props.Get(), WIA_DIP_VEND_DESC);
        d.info.model = readString(props.Get(), WIA_DIP_DEV_NAME);
        d.stiType = int(GET_STIDEVICE_TYPE(readInt(props.Get(), WIA_DIP_DEV_TYPE, 0)));
        d.info.type = Scanner::typeLabel(d.stiType);
        if (!d.info.name.isEmpty())
            out.append(d);
    }
    return out;
}

} // namespace

QString Scanner::Device::label() const {
    QString human = QStringLiteral("%1 %2").arg(vendor, model).trimmed();
    if (human.isEmpty())
        human = name;
    if (!type.isEmpty())
        human += QStringLiteral(" (%1)").arg(type);
    return human;
}

QString Scanner::typeLabel(int stiDeviceType) {
    switch (stiDeviceType) {
    case StiDeviceTypeScanner: return QStringLiteral("scanner");
    case StiDeviceTypeDigitalCamera: return QStringLiteral("camera");
    case StiDeviceTypeStreamingVideo: return QStringLiteral("video");
    default: return QString();
    }
}

bool Scanner::isAvailable() {
    ComApartment com;
    if (!com.ok())
        return false;
    return deviceManager() != nullptr;
}

QList<Scanner::Device> Scanner::devices(QString* error) {
    ComApartment com;
    if (!com.ok()) {
        if (error)
            *error = QObject::tr("The Windows Image Acquisition (WIA) service isn't available.");
        return {};
    }
    const ComPtr<IWiaDevMgr2> mgr = deviceManager();
    if (!mgr) {
        if (error)
            *error = QObject::tr("The Windows Image Acquisition (WIA) service isn't available.");
        return {};
    }
    QList<Device> out;
    for (const EnumeratedDevice& d : enumerate(mgr.Get(), error))
        if (d.stiType == StiDeviceTypeScanner)
            out.append(d.info);
    return out;
}

QStringList Scanner::scanPages(const QString& device, int dpi, Mode mode, int pageCount,
                               const QString& outDir, QString* error) {
    ComApartment com;
    if (!com.ok()) {
        if (error)
            *error = QObject::tr("The Windows Image Acquisition (WIA) service isn't available.");
        return {};
    }
    const ComPtr<IWiaDevMgr2> mgr = deviceManager();
    if (!mgr) {
        if (error)
            *error = QObject::tr("The Windows Image Acquisition (WIA) service isn't available.");
        return {};
    }

    QString devId = device;
    if (devId.isEmpty()) {
        for (const EnumeratedDevice& d : enumerate(mgr.Get(), error))
            if (d.stiType == StiDeviceTypeScanner) {
                devId = d.info.name;
                break;
            }
        if (devId.isEmpty()) {
            if (error && error->isEmpty())
                *error = QObject::tr("No scanner was found.");
            return {};
        }
    }

    BSTR id = SysAllocString(reinterpret_cast<const wchar_t*>(devId.utf16()));
    ComPtr<IWiaItem2> root;
    HRESULT hr = mgr->CreateDevice(0, id, &root);
    SysFreeString(id);
    if (FAILED(hr)) {
        if (error)
            *error = QObject::tr("Couldn't open the scanner (%1).").arg(hrMessage(hr));
        return {};
    }

    // Pick the input to scan from: the feeder for multi-page jobs when the
    // scanner has one, else the flatbed, else whatever single input exists.
    ComPtr<IWiaItem2> flatbed, feeder, fallback;
    ComPtr<IEnumWiaItem2> children;
    if (SUCCEEDED(root->EnumChildItems(nullptr, &children))) {
        for (;;) {
            ComPtr<IWiaItem2> item;
            ULONG fetched = 0;
            if (children->Next(1, &item, &fetched) != S_OK || fetched == 0)
                break;
            ComPtr<IWiaPropertyStorage> props;
            item.As(&props);
            const GUID category = readGuid(props.Get(), WIA_IPA_ITEM_CATEGORY);
            if (category == WIA_CATEGORY_FLATBED && !flatbed)
                flatbed = item;
            else if (category == WIA_CATEGORY_FEEDER && !feeder)
                feeder = item;
            if (!fallback)
                fallback = item;
        }
    }

    const int pages = qMax(1, pageCount);
    const bool useFeeder = feeder && (pages > 1 || !flatbed);
    ComPtr<IWiaItem2> input = useFeeder ? feeder : (flatbed ? flatbed : fallback);
    if (!input) {
        if (error)
            *error = QObject::tr("The scanner reports no flatbed or feeder to scan from.");
        return {};
    }

    // Configure the scan. Individual drivers reject properties they don't
    // support; that's fine — the scan then runs with the driver's defaults.
    ComPtr<IWiaPropertyStorage> props;
    input.As(&props);
    const int boundDpi = qBound(50, dpi, 1200);
    writeInt(props.Get(), WIA_IPS_XRES, boundDpi);
    writeInt(props.Get(), WIA_IPS_YRES, boundDpi);
    writeInt(props.Get(), WIA_IPA_DATATYPE,
             mode == Mode::Color ? WIA_DATA_COLOR : WIA_DATA_GRAYSCALE);
    writeGuid(props.Get(), WIA_IPA_FORMAT, WiaImgFmt_BMP);
    if (useFeeder)
        writeInt(props.Get(), WIA_IPS_PAGES, pages);

    ComPtr<IWiaTransfer> transfer;
    if (FAILED(input.As(&transfer)) || !transfer) {
        if (error)
            *error = QObject::tr("The scanner doesn't support image transfer.");
        return {};
    }

    QStringList bmps;
    HRESULT lastHr = S_OK;
    if (useFeeder) {
        // One download; the feeder delivers every page as its own stream.
        FileTransferCallback* cb = new FileTransferCallback(outDir, 0);
        lastHr = transfer->Download(0, cb);
        bmps = cb->files();
        cb->Release();
    } else {
        // Flatbed: each download captures one page.
        int index = 0;
        for (int i = 0; i < pages; ++i) {
            FileTransferCallback* cb = new FileTransferCallback(outDir, index);
            lastHr = transfer->Download(0, cb);
            const QStringList produced = cb->files();
            index = cb->nextIndex();
            cb->Release();
            if (FAILED(lastHr))
                break;
            bmps << produced;
        }
    }

    // An empty feeder mid-batch is not an error: keep the pages that arrived.
    if (bmps.isEmpty()) {
        if (error) {
            *error = lastHr == WIA_ERROR_PAPER_EMPTY
                         ? QObject::tr("The document feeder is empty.")
                         : QObject::tr("Scanning failed (%1).").arg(hrMessage(lastHr));
        }
        return {};
    }

    // The pipeline expects PNGs (and BMPs are huge); convert and drop the BMPs.
    QStringList pngs;
    for (const QString& bmp : bmps) {
        const QImage image(bmp);
        if (image.isNull()) {
            if (error)
                *error = QObject::tr("A scanned page couldn't be read.");
            return {};
        }
        QString png = bmp;
        png.replace(QStringLiteral(".bmp"), QStringLiteral(".png"));
        if (!image.save(png)) {
            if (error)
                *error = QObject::tr("A scanned page couldn't be saved.");
            return {};
        }
        QFile::remove(bmp);
        pngs << png;
    }
    return pngs;
}
