#include "common.h"
#include "v2m/v2init.h"

namespace ttp {
HMODULE module{};
HRESULT make_reader(void **out);
} // namespace ttp

namespace {

class Factory final : public ttp::ReaderCreator {
    LONG refs_{1};

public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
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

extern "C" __declspec(dllexport) HRESULT WINAPI ttpGetSoundAddIn(void **out) {
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
        ttp::module = instance;
        DisableThreadLibraryCalls(instance);
        v2_init_version_tables();
    }
    return TRUE;
}
