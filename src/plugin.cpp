#include "common.h"
#include "v2m/v2init.h"
#include "ttpcomm/audio_format_probe.h"

#include <cstring>

namespace ttp {
HRESULT make_reader(void **out);
} // namespace ttp

namespace {

template <size_t N>
void set_text(char (&dst)[N], const char *src) noexcept {
    if (!src)
        return;
    size_t i = 0;
    for (; i + 1 < N && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = 0;
}

template <size_t N>
void set_text(wchar_t (&dst)[N], const wchar_t *src) noexcept {
    if (!src)
        return;
    size_t i = 0;
    for (; i + 1 < N && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = 0;
}

class Factory final : public ttp::ReaderCreator, public ttp::format::Probe {
    LONG refs_{1};

public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (ttp::same(iid, ttp::format::iid_probe)) {
            *p = static_cast<ttp::format::Probe *>(this);
            AddRef();
            return S_OK;
        }
        if (ttp::same(iid, IID_IUnknown) || ttp::same(iid, ttp::cat_reader)) {
            *p = static_cast<ttp::ReaderCreator *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ULONG(InterlockedIncrement(&refs_));
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const LONG n = InterlockedDecrement(&refs_);
        if (!n)
            delete this;
        return ULONG(n);
    }
    HRESULT STDMETHODCALLTYPE Create(void **p) override {
        return ttp::make_reader(p);
    }
    HRESULT STDMETHODCALLTYPE Name(wchar_t **p) override {
        return ttp::text(L"Farbrausch V2 Reader", p);
    }
    HRESULT STDMETHODCALLTYPE Extensions(wchar_t **p) override {
        return ttp::text(L"Farbrausch V2(*.v2m)", p);
    }

    HRESULT STDMETHODCALLTYPE ProbeFormat(
        const ttp::format::Request *request,
        ttp::format::Result *result) override {
        if (!request || !result)
            return E_POINTER;
        if (request->size != sizeof(ttp::format::Request) ||
            request->version != ttp::format::abi_version ||
            result->size != sizeof(ttp::format::Result) ||
            result->version != ttp::format::abi_version)
            return E_INVALIDARG;
        if (request->bytes > ttp::format::prefix_limit ||
            (!request->data && request->bytes))
            return E_INVALIDARG;

        *result = ttp::format::Result{};
        result->size = sizeof(ttp::format::Result);
        result->version = ttp::format::abi_version;

        if (request->bytes < 12) {
            result->confidence = ttp::format::NeedMore;
            result->required_bytes = 12;
            return S_OK;
        }

        uint32_t timediv = 0, maxtime = 0, gdnum = 0;
        std::memcpy(&timediv, request->data, 4);
        std::memcpy(&maxtime, request->data + 4, 4);
        std::memcpy(&gdnum, request->data + 8, 4);
        if (timediv != 128 || !maxtime || !gdnum || gdnum > 1024)
            return S_FALSE;

        result->family = ttp::format::Module;
        result->confidence = ttp::format::Strong;
        set_text(result->container, "v2m");
        set_text(result->codec, "v2m");
        set_text(result->extension, L".v2m");
        set_text(result->aliases, L"");
        set_text(result->display, L"Farbrausch V2 Module");
        return S_OK;
    }
};

class Plugin final : public ttp::AddIn {
    LONG refs_{1};

public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (ttp::same(iid, IID_IUnknown) || ttp::same(iid, ttp::iid_addin)) {
            *p = static_cast<ttp::AddIn *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ULONG(InterlockedIncrement(&refs_));
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const LONG n = InterlockedDecrement(&refs_);
        if (!n)
            delete this;
        return ULONG(n);
    }
    HRESULT STDMETHODCALLTYPE Enum(DWORD index, GUID *category,
                                   void **out) override {
        if (!out || !category)
            return E_POINTER;
        *out = nullptr;
        if (index)
            return E_INVALIDARG;
        return ttp::protect([&] {
            *category = ttp::cat_reader;
            *out = static_cast<ttp::ReaderCreator *>(new Factory);
            return S_OK;
        });
    }
};

} // namespace

extern "C" HRESULT WINAPI ttpGetSoundAddIn(void **out) {
    if (!out)
        return E_POINTER;
    *out = nullptr;
    return ttp::protect([&] {
        *out = static_cast<ttp::AddIn *>(new Plugin);
        return S_OK;
    });
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        v2_init_version_tables();
    }
    return TRUE;
}
