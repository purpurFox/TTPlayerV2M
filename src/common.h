#pragma once
#include "ttp_player_abi.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ttp {

using Bytes = std::vector<BYTE>;

struct Failure {
    HRESULT code;
};

inline void require(bool condition,
                    HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA)) {
    if (!condition)
        throw Failure{hr};
}

inline void check(HRESULT hr) {
    if (FAILED(hr))
        throw Failure{hr};
}

template <class F>
HRESULT protect(F &&f) noexcept {
    try {
        return f();
    } catch (const Failure &e) {
        return e.code;
    } catch (const std::bad_alloc &) {
        return E_OUTOFMEMORY;
    } catch (...) {
        return E_FAIL;
    }
}

template <class T>
struct ComPtr {
    T *p{};

    ComPtr() = default;
    explicit ComPtr(T *value, bool retain = false) : p(value) {
        if (p && retain)
            p->AddRef();
    }
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr &) = delete;
    ComPtr &operator=(const ComPtr &) = delete;
    ComPtr(ComPtr &&other) noexcept : p(std::exchange(other.p, nullptr)) {}
    ComPtr &operator=(ComPtr &&other) noexcept {
        if (this != &other) {
            reset();
            p = std::exchange(other.p, nullptr);
        }
        return *this;
    }
    void reset(T *value = nullptr) {
        if (p)
            p->Release();
        p = value;
    }
    T *operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
    T **put() {
        reset();
        return &p;
    }
};

inline bool same(REFGUID a, REFGUID b) {
    return !!InlineIsEqualGUID(a, b);
}

inline HRESULT text(std::wstring_view s, wchar_t **out) {
    if (!out)
        return E_POINTER;
    *out = nullptr;
    auto *p = static_cast<wchar_t *>(
        CoTaskMemAlloc((s.size() + 1) * sizeof(wchar_t)));
    if (!p)
        return E_OUTOFMEMORY;
    if (!s.empty())
        std::memcpy(p, s.data(), s.size() * sizeof(wchar_t));
    p[s.size()] = L'\0';
    *out = p;
    return S_OK;
}

inline DWORD dword(std::uint64_t x) {
    return DWORD(std::min<std::uint64_t>(x, MAXDWORD));
}

inline std::wstring basename_without_extension(const std::wstring &path) {
    const size_t slash = path.find_last_of(L"\\/");
    std::wstring name =
        slash == std::wstring::npos ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos && dot != 0)
        name.erase(dot);
    return name;
}

inline std::wstring stream_title(IStream *stream) {
    if (!stream)
        return {};
    STATSTG st{};
    if (FAILED(stream->Stat(&st, STATFLAG_DEFAULT)))
        return {};
    std::wstring result;
    if (st.pwcsName)
        result = basename_without_extension(st.pwcsName);
    if (st.pwcsName)
        CoTaskMemFree(st.pwcsName);
    return result;
}

inline Bytes read_all(IStream *stream, std::uint64_t maximum) {
    require(stream != nullptr, E_POINTER);
    Bytes out;
    STATSTG st{};
    if (SUCCEEDED(stream->Stat(&st, STATFLAG_NONAME)) &&
        st.cbSize.QuadPart > 0 &&
        std::uint64_t(st.cbSize.QuadPart) <= maximum)
        out.reserve(size_t(st.cbSize.QuadPart));

    BYTE chunk[65536];
    for (;;) {
        ULONG got = 0;
        const HRESULT hr = stream->Read(chunk, sizeof(chunk), &got);
        if (got) {
            require(out.size() + got <= maximum,
                    HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
            out.insert(out.end(), chunk, chunk + got);
        }
        if (hr == S_FALSE)
            break;
        check(hr);
        if (!got)
            break;
    }
    require(!out.empty(), HRESULT_FROM_WIN32(ERROR_HANDLE_EOF));
    return out;
}

class LockGuard {
    CRITICAL_SECTION *cs_;

public:
    explicit LockGuard(CRITICAL_SECTION *cs) : cs_(cs) {
        EnterCriticalSection(cs_);
    }
    ~LockGuard() { LeaveCriticalSection(cs_); }
    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;
};

} // namespace ttp
