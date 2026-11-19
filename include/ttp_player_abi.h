#pragma once
// Minimal TTPlayer sound add-in ABI, recovered from the original player and
// the TTPlayerRebuild reference plugins. Strings returned to the host are
// allocated with CoTaskMemAlloc. This header is interface-only; see
// docs/ABI.md for the provenance notes.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmreg.h>
#include <objidl.h>

namespace ttp {

inline constexpr GUID iid_addin{
    0xeec6c534, 0xfeba, 0x421e, {0xaa, 0x5d, 0x10, 0xac, 0x66, 0xf9, 0x87, 0x84}};
inline constexpr GUID iid_reader{
    0x30c7c165, 0xc0a9, 0x4204, {0x99, 0x5d, 0x45, 0x6a, 0x76, 0x49, 0x98, 0xfb}};
inline constexpr GUID iid_metadata{
    0x7ad84e00, 0x5fef, 0x4481, {0xb5, 0x32, 0xfb, 0xbd, 0x67, 0x7e, 0x67, 0xc2}};
inline constexpr GUID cat_reader{
    0x476d15a5, 0xd863, 0x416a, {0x8a, 0x59, 0xa9, 0xc7, 0xd7, 0x2c, 0xe0, 0x4e}};

struct Buffer : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetLength(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE Capacity(DWORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE Data(BYTE **, DWORD *) = 0;
};

struct Reader : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Open(IStream *, DWORD flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE Capabilities(DWORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE Duration(DWORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE Format(WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferSize(DWORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE CodecName(wchar_t **) = 0;
    virtual HRESULT STDMETHODCALLTYPE Bitrate(DWORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE BitDepth(WORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(IUnknown *) = 0;
    virtual HRESULT STDMETHODCALLTYPE Start() = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE Read(Buffer *) = 0;
    virtual HRESULT STDMETHODCALLTYPE Seek(DWORD *) = 0;
};

struct Metadata : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Count(DWORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE At(DWORD, wchar_t **, wchar_t **) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get(const char *, wchar_t **) = 0;
    virtual HRESULT STDMETHODCALLTYPE Set(const char *, const wchar_t *) = 0;
};

struct Creator : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Create(void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE Name(wchar_t **) = 0;
};

struct ReaderCreator : Creator {
    virtual HRESULT STDMETHODCALLTYPE Extensions(wchar_t **) = 0;
};

struct AddIn : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Enum(DWORD, GUID *, void **) = 0;
};

} // namespace ttp
