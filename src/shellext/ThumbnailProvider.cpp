// Feather PDF Pro — light on the system, full-featured on PDF.
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

// Explorer thumbnail provider (feather-thumb.dll): renders page one of a PDF
// so folders show real previews when Feather owns the .pdf association.
//
// Explorer loads thumbnail providers into its DllHost surrogate, so this DLL
// must work without the app beside it: pages are rendered with the
// Windows.Data.Pdf runtime classes that ship with Windows 10+, not with
// Qt/Poppler — no Feather DLL is ever pulled into the surrogate.
//
// The installer writes the registration (CLSID + shellex under the
// FeatherPDF.Document ProgID); for development an elevated
// `regsvr32 feather-thumb.dll` does the same.

#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <propsys.h>
#include <shcore.h>
#include <shlwapi.h>
#include <thumbcache.h>
#include <windows.data.pdf.interop.h>

#include <winrt/base.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {

// {C6DD57D7-9B9D-45BE-9881-AACF7F842E7A}
constexpr CLSID kClsid = {
    0xC6DD57D7, 0x9B9D, 0x45BE, {0x98, 0x81, 0xAA, 0xCF, 0x7F, 0x84, 0x2E, 0x7A}};
constexpr wchar_t kClsidText[] = L"{C6DD57D7-9B9D-45BE-9881-AACF7F842E7A}";
constexpr wchar_t kFriendlyName[] = L"Feather PDF Thumbnail Provider";
// The shell's fixed category GUID for thumbnail handlers.
constexpr wchar_t kThumbnailHandlerCategory[] = L"{E357FCCD-A995-4576-B01F-234630154E96}";

HINSTANCE g_module = nullptr;
std::atomic<LONG> g_objects{0}; // live COM objects + factory locks

// PdfCreateRenderer, resolved at run time so this DLL still loads (and can
// answer "not available") on hosts without the Windows.Data.Pdf runtime.
HRESULT createPdfRenderer(IDXGIDevice* device, IPdfRendererNative** renderer) {
    using CreateFn = HRESULT(WINAPI*)(IDXGIDevice*, IPdfRendererNative**);
    static const CreateFn create = [] {
        const HMODULE dll = LoadLibraryExW(L"Windows.Data.Pdf.dll", nullptr,
                                           LOAD_LIBRARY_SEARCH_SYSTEM32);
        return dll ? reinterpret_cast<CreateFn>(GetProcAddress(dll, "PdfCreateRenderer"))
                   : nullptr;
    }();
    if (!create)
        return REGDB_E_CLASSNOTREG;
    return create(device, renderer);
}

// Renders `bytes` (a complete PDF) and returns a top-down 32bpp premultiplied
// BGRA DIB section whose longest edge is `cx` pixels. Throws on failure.
//
// Rendering goes through the IPdfRendererNative interop path with a D3D11
// device created and torn down right here, not RenderToStreamAsync: the
// convenience API parks a D3D device in Windows.Data.Pdf's globals forever,
// and a device that is still alive when the process exits can crash inside
// the display driver's DLL_PROCESS_DETACH.
HBITMAP renderThumbnail(const BYTE* bytes, ULONG size, UINT cx) {
    using namespace winrt;
    using namespace winrt::Windows::Data::Pdf;
    using namespace winrt::Windows::Storage::Streams;

    // The PDF bytes → an IRandomAccessStream the WinRT parser accepts.
    com_ptr<IStream> mem;
    mem.attach(SHCreateMemStream(bytes, size));
    if (!mem)
        throw std::bad_alloc();
    IRandomAccessStream pdfStream{nullptr};
    check_hresult(CreateRandomAccessStreamOverStream(
        mem.get(), BSOS_DEFAULT, guid_of<IRandomAccessStream>(), put_abi(pdfStream)));

    PdfDocument doc = PdfDocument::LoadFromStreamAsync(pdfStream).get();
    if (doc.PageCount() == 0)
        throw hresult_error(E_FAIL);
    PdfPage page = doc.GetPage(0);

    const auto pageSize = page.Size();
    if (pageSize.Width <= 0 || pageSize.Height <= 0)
        throw hresult_error(E_FAIL);
    const float scale = (pageSize.Width >= pageSize.Height) ? cx / pageSize.Width
                                                            : cx / pageSize.Height;
    const UINT w = static_cast<UINT>(std::max(1L, std::lround(pageSize.Width * scale)));
    const UINT h = static_cast<UINT>(std::max(1L, std::lround(pageSize.Height * scale)));

    // D2D interop needs BGRA support; fall back to WARP on GPU-less hosts.
    com_ptr<ID3D11Device> device;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                   D3D11_SDK_VERSION, device.put(), nullptr, nullptr);
    if (FAILED(hr))
        check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                        D3D11_SDK_VERSION, device.put(), nullptr, nullptr));
    com_ptr<IPdfRendererNative> renderer;
    check_hresult(createPdfRenderer(device.as<IDXGIDevice>().get(), renderer.put()));

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    com_ptr<ID3D11Texture2D> target;
    check_hresult(device->CreateTexture2D(&desc, nullptr, target.put()));

    PDF_RENDER_PARAMS params = {};
    params.SourceRect = {0.0f, 0.0f, pageSize.Width, pageSize.Height};
    params.DestinationWidth = w;
    params.DestinationHeight = h;
    // PDF pages have no background of their own; flatten onto opaque white
    // like every viewer does.
    params.BackgroundColor = {1.0f, 1.0f, 1.0f, 1.0f};
    check_hresult(renderer->RenderPageToSurface(winrt::get_unknown(page),
                                                target.as<IDXGISurface>().get(),
                                                POINT{0, 0}, &params));
    page.Close();

    // Read the pixels back through a staging copy into a DIB section.
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    com_ptr<ID3D11Texture2D> staging;
    check_hresult(device->CreateTexture2D(&desc, nullptr, staging.put()));
    com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    context->CopyResource(staging.get(), target.get());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    check_hresult(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap) {
        context->Unmap(staging.get(), 0);
        throw std::bad_alloc();
    }
    for (UINT row = 0; row < h; ++row)
        memcpy(static_cast<BYTE*>(bits) + row * (w * 4),
               static_cast<const BYTE*>(mapped.pData) + row * mapped.RowPitch, w * 4);
    context->Unmap(staging.get(), 0);

    // Make sure the device has finished (and can be torn down cleanly) before
    // the com_ptrs release it.
    context->ClearState();
    context->Flush();
    return bitmap;
}

class ThumbnailProvider : public IInitializeWithStream, public IThumbnailProvider {
public:
    ThumbnailProvider() { ++g_objects; }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        static const QITAB qit[] = {
            QITABENT(ThumbnailProvider, IInitializeWithStream),
            QITABENT(ThumbnailProvider, IThumbnailProvider),
            {nullptr, 0},
        };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++m_ref; }
    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG left = --m_ref;
        if (left == 0)
            delete this;
        return left;
    }

    // IInitializeWithStream. The whole PDF is read here, on the shell's own
    // thread: the IStream may be apartment-bound, so it must not be touched
    // from the render thread later.
    IFACEMETHODIMP Initialize(IStream* stream, DWORD) override {
        if (!stream)
            return E_POINTER;
        if (!m_bytes.empty())
            return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        STATSTG st = {};
        HRESULT hr = stream->Stat(&st, STATFLAG_NONAME);
        if (FAILED(hr))
            return hr;
        // Don't balloon the surrogate process for absurd files; Explorer just
        // falls back to the generic icon.
        if (st.cbSize.QuadPart == 0 || st.cbSize.QuadPart > 256ull * 1024 * 1024)
            return E_FAIL;
        try {
            m_bytes.resize(static_cast<size_t>(st.cbSize.QuadPart));
        } catch (const std::bad_alloc&) {
            return E_OUTOFMEMORY;
        }
        LARGE_INTEGER zero = {};
        stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        size_t total = 0;
        while (total < m_bytes.size()) {
            ULONG got = 0;
            hr = stream->Read(m_bytes.data() + total,
                              static_cast<ULONG>(m_bytes.size() - total), &got);
            if (FAILED(hr) || got == 0)
                break;
            total += got;
        }
        if (total != m_bytes.size()) {
            m_bytes.clear();
            return FAILED(hr) ? hr : E_FAIL;
        }
        return S_OK;
    }

    // IThumbnailProvider. Explorer calls this on an STA thread, where
    // C++/WinRT refuses blocking .get() waits — so the render runs on a short
    // MTA worker thread instead and this thread just joins it.
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override {
        if (!phbmp || !pdwAlpha)
            return E_POINTER;
        *phbmp = nullptr;
        *pdwAlpha = WTSAT_UNKNOWN;
        if (cx == 0)
            return E_INVALIDARG;
        if (m_bytes.empty())
            return E_UNEXPECTED;

        HRESULT hr = E_FAIL;
        HBITMAP bitmap = nullptr;
        try {
            std::thread worker([&]() noexcept {
                winrt::init_apartment(); // MTA
                try {
                    bitmap = renderThumbnail(m_bytes.data(),
                                             static_cast<ULONG>(m_bytes.size()), cx);
                    hr = S_OK;
                } catch (...) {
                    hr = winrt::to_hresult();
                }
                winrt::uninit_apartment();
            });
            worker.join();
        } catch (...) {
            return E_FAIL;
        }
        if (FAILED(hr))
            return hr;
        *phbmp = bitmap;
        *pdwAlpha = WTSAT_ARGB;
        return S_OK;
    }

private:
    virtual ~ThumbnailProvider() { --g_objects; }

    std::atomic<ULONG> m_ref{1};
    std::vector<BYTE> m_bytes;
};

class ClassFactory : public IClassFactory {
public:
    ClassFactory() { ++g_objects; }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        static const QITAB qit[] = {
            QITABENT(ClassFactory, IClassFactory),
            {nullptr, 0},
        };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++m_ref; }
    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG left = --m_ref;
        if (left == 0)
            delete this;
        return left;
    }

    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv)
            return E_POINTER;
        *ppv = nullptr;
        if (outer)
            return CLASS_E_NOAGGREGATION;
        ThumbnailProvider* provider = new (std::nothrow) ThumbnailProvider();
        if (!provider)
            return E_OUTOFMEMORY;
        const HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock)
            ++g_objects;
        else
            --g_objects;
        return S_OK;
    }

private:
    virtual ~ClassFactory() { --g_objects; }

    std::atomic<ULONG> m_ref{1};
};

HRESULT setRegValue(const std::wstring& subkey, PCWSTR name, PCWSTR data) {
    const LSTATUS st =
        RegSetKeyValueW(HKEY_CLASSES_ROOT, subkey.c_str(), name, REG_SZ, data,
                        static_cast<DWORD>((lstrlenW(data) + 1) * sizeof(wchar_t)));
    return HRESULT_FROM_WIN32(st);
}

} // namespace

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    if (!ppv)
        return E_POINTER;
    *ppv = nullptr;
    if (!IsEqualCLSID(clsid, kClsid))
        return CLASS_E_CLASSNOTAVAILABLE;
    ClassFactory* factory = new (std::nothrow) ClassFactory();
    if (!factory)
        return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return g_objects.load() == 0 ? S_OK : S_FALSE;
}

// regsvr32 support for development; the installer writes the same keys (plus
// the shellex entry under Applications\feather-pdf-pro.exe, which matters when
// the user picked Feather through the Open With dialog).
STDAPI DllRegisterServer() {
    wchar_t module[MAX_PATH];
    if (!GetModuleFileNameW(g_module, module, MAX_PATH))
        return HRESULT_FROM_WIN32(GetLastError());
    const std::wstring clsidKey = std::wstring(L"CLSID\\") + kClsidText;
    HRESULT hr = setRegValue(clsidKey, nullptr, kFriendlyName);
    if (SUCCEEDED(hr))
        hr = setRegValue(clsidKey + L"\\InprocServer32", nullptr, module);
    if (SUCCEEDED(hr))
        hr = setRegValue(clsidKey + L"\\InprocServer32", L"ThreadingModel", L"Both");
    if (SUCCEEDED(hr))
        hr = setRegValue(std::wstring(L"FeatherPDF.Document\\shellex\\") +
                             kThumbnailHandlerCategory,
                         nullptr, kClsidText);
    return hr;
}

STDAPI DllUnregisterServer() {
    RegDeleteTreeW(HKEY_CLASSES_ROOT, (std::wstring(L"CLSID\\") + kClsidText).c_str());
    RegDeleteTreeW(HKEY_CLASSES_ROOT, (std::wstring(L"FeatherPDF.Document\\shellex\\") +
                                       kThumbnailHandlerCategory)
                                          .c_str());
    return S_OK;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
