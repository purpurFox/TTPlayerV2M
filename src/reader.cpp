#define V2MPLAYER_SYNC_FUNCTIONS 1

#include "common.h"
#include "ttpcomm/audio_format_probe.h"
#include "v2m/types.h"
#include "v2m/v2mplayer.h"
#include "v2m/v2mconv.h"
#include "v2m/v2init.h"

#include <cmath>
#include <cstdint>
#include <cwchar>

namespace ttp {
namespace {

constexpr DWORD kSampleRate = 44100;
constexpr DWORD kChannels = 2;
constexpr DWORD kBits = 16;
constexpr DWORD kBlockAlign = kChannels * (kBits / 8);
constexpr DWORD kBytesPerSecond = kSampleRate * kBlockAlign;
constexpr std::uint64_t kMaximumFile = 256ull * 1024 * 1024;
constexpr DWORD kMaximumFramesPerRead = 65536;
inline constexpr GUID stream_callback_id{
    0x1717a4a7, 0xa3dc, 0x416b, {0xa2, 0x97, 0x72, 0x16, 0x74, 0x9b, 0xd7, 0xbf}};

inline int16_t pcm16(float value) {
    if (!(value == value))
        return 0;
    float scaled = value * 32767.0f;
    if (scaled > 32767.0f)
        scaled = 32767.0f;
    else if (scaled < -32768.0f)
        scaled = -32768.0f;
    return int16_t(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

} // namespace

class V2Reader final : public Reader, public Metadata, public ttp::format::SourceInfo {
    LONG refs_{1};
    CRITICAL_SECTION lock_{};
    Bytes file_;
    Bytes converted_;
    std::unique_ptr<V2MPlayer> player_;
    WAVEFORMATEX format_{WAVE_FORMAT_PCM, WORD(kChannels), kSampleRate,
                         kBytesPerSecond, WORD(kBlockAlign), WORD(kBits), 0};
    DWORD duration_{};
    DWORD position_ms_{};
    DWORD tail_frames_{};
    bool tail_done_{};
    bool started_{};
    bool abort_{};
    std::wstring title_;
    std::vector<float> floats_;
    ComPtr<IStream> stream_;
    ComPtr<IUnknown> callback_;
    ttp::format::Result info_{};
    bool info_ready_{};

    HRESULT apply_callback(IUnknown *p) {
        if (!stream_ || !p)
            return S_OK;
        ComPtr<IUnknown> ext;
        if (FAILED(stream_->QueryInterface(
                stream_callback_id, reinterpret_cast<void **>(ext.put()))))
            return S_OK;
        using Fn = HRESULT(STDMETHODCALLTYPE *)(IUnknown *, IUnknown *);
        auto **v = *reinterpret_cast<void ***>(ext.p);
        return reinterpret_cast<Fn>(v[4])(ext.p, p);
    }

    void reset_locked() {
        if (player_) {
            player_->Close();
            player_.reset();
        }
        converted_.clear();
        file_.clear();
        floats_.clear();
        stream_.reset();
        callback_.reset();
        info_ready_ = false;
        info_ = ttp::format::Result{};
        duration_ = position_ms_ = tail_frames_ = 0;
        tail_done_ = false;
        started_ = abort_ = false;
        title_.clear();
    }

    HRESULT read_locked(Buffer *buffer) {
        require(buffer != nullptr, E_POINTER);
        check(buffer->SetLength(0));
        require(player_ != nullptr, E_UNEXPECTED);
        require(!abort_, E_ABORT);

        BYTE *dst = nullptr;
        DWORD length = 0;
        DWORD capacity = 0;
        check(buffer->Data(&dst, &length));
        check(buffer->Capacity(&capacity));
        require(dst != nullptr && capacity >= kBlockAlign, E_INVALIDARG);

        if (!started_) {
            player_->Play(position_ms_);
            started_ = true;
            tail_frames_ = 0;
        }

        // V2M files may loop forever; the host relies on the reported
        // duration, but enforce it here as well.
        if (position_ms_ >= duration_) {
            check(buffer->SetLength(0));
            return S_FALSE;
        }

        const bool playing = !!player_->IsPlaying();
        if (!playing) {
            if (tail_done_) {
                check(buffer->SetLength(0));
                return S_FALSE;
            }
            if (tail_frames_ == 0)
                tail_frames_ = 2 * kSampleRate;
        }

        bool final_chunk = false;
        DWORD frames = std::min<DWORD>(capacity / kBlockAlign,
                                       kMaximumFramesPerRead);
        if (tail_frames_ && frames > tail_frames_)
            frames = tail_frames_;
        else if (!tail_frames_) {
            const std::uint64_t remaining =
                (std::uint64_t(duration_ - position_ms_) * kSampleRate) / 1000;
            if (frames > remaining)
                frames = DWORD(remaining);
            final_chunk = frames == remaining;
        }
        require(frames > 0, E_INVALIDARG);

        floats_.resize(size_t(frames) * 2);
        player_->Render(floats_.data(), frames);


        for (DWORD i = 0; i < frames; ++i) {
            const int16_t left = pcm16(floats_[2 * i]);
            const int16_t right = pcm16(floats_[2 * i + 1]);
            std::memcpy(dst + 4 * i, &left, sizeof(left));
            std::memcpy(dst + 4 * i + 2, &right, sizeof(right));
        }
        check(buffer->SetLength(frames * kBlockAlign));

        if (playing) {
            position_ms_ += DWORD((std::uint64_t(frames) * 1000) / kSampleRate);
            if (final_chunk)
                position_ms_ = duration_;
        }
        if (tail_frames_) {
            tail_frames_ = frames >= tail_frames_ ? 0 : tail_frames_ - frames;
            if (tail_frames_ == 0)
                tail_done_ = true;
        }
        return S_OK;
    }

public:
    V2Reader() {
        InitializeCriticalSectionAndSpinCount(&lock_, 4000);
    }
    ~V2Reader() {
        {
            LockGuard guard(&lock_);
            reset_locked();
        }
        DeleteCriticalSection(&lock_);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (same(iid, IID_IUnknown) || same(iid, iid_reader))
            *out = static_cast<Reader *>(this);
        else if (same(iid, iid_metadata))
            *out = static_cast<Metadata *>(this);
        else if (same(iid, ttp::format::iid_info))
            *out = static_cast<ttp::format::SourceInfo *>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
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

    HRESULT STDMETHODCALLTYPE Open(IStream *stream, DWORD) override {
        const HRESULT hr = protect([&] {
            require(stream != nullptr, E_POINTER);
            stream_.reset(stream);
            stream->AddRef();
            LockGuard guard(&lock_);
            require(player_ == nullptr, E_UNEXPECTED);

            file_ = read_all(stream, kMaximumFile);
            require(file_.size() <= size_t(INT_MAX), E_INVALIDARG);

            v2_init_version_tables();
            const int delta =
                CheckV2MVersion(file_.data(), int(file_.size()));
            require(delta >= 0, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

            unsigned char *converted = nullptr;
            int converted_length = 0;
            ConvertV2M(file_.data(), int(file_.size()), &converted,
                       &converted_length);
            require(converted != nullptr && converted_length > 0, E_FAIL);
            converted_.assign(converted, converted + converted_length);
            delete[] converted;

            player_ = std::make_unique<V2MPlayer>();
            player_->Init(1000);
            require(player_->Open(converted_.data(), kSampleRate) != 0,
                    HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

            sS32 *timeline = nullptr;
            const sU32 positions = player_->CalcPositions(&timeline);
            if (positions && timeline) {
                const sS32 last = timeline[2 * (positions - 1)];
                if (last > 0)
                    duration_ = dword(std::uint64_t(last) + 2000);
            }
            delete[] timeline;

            if (!duration_ && file_.size() >= 8) {
                sU32 timediv = 0, maxtime = 0;
                std::memcpy(&timediv, file_.data(), 4);
                std::memcpy(&maxtime, file_.data() + 4, 4);
                if (timediv)
                    duration_ =
                        dword(std::uint64_t(maxtime) * 1000 / timediv + 2000);
            }
            if (!duration_)
                duration_ = 1;

            title_ = stream_title(stream);
            if (title_.empty())
                title_ = L"Farbrausch V2 Module";

            info_ = ttp::format::Result{};
            info_.size = sizeof(info_);
            info_.version = ttp::format::abi_version;
            info_.family = ttp::format::Module;
            info_.confidence = ttp::format::Opened;
            strcpy_s(info_.container, "v2m");
            strcpy_s(info_.codec, "v2m");
            wcscpy_s(info_.extension, L".v2m");
            wcscpy_s(info_.aliases, L"");
            wcscpy_s(info_.display, L"Farbrausch V2 Module");
            info_ready_ = true;
            if (callback_)
                apply_callback(callback_.p);

            position_ms_ = 0;
            tail_frames_ = 0;
            tail_done_ = false;
            started_ = abort_ = false;
            return S_OK;
        });
        if (FAILED(hr)) {
            LockGuard guard(&lock_);
            reset_locked();
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE Capabilities(DWORD *p) override {
        if (!p)
            return E_POINTER;
        *p = 2; // seekable; the whole stream is buffered at Open
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Duration(DWORD *p) override {
        if (!p)
            return E_POINTER;
        *p = player_ ? duration_ : 0;
        return player_ ? S_OK : E_UNEXPECTED;
    }
    HRESULT STDMETHODCALLTYPE Format(WAVEFORMATEX **p) override {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (!player_)
            return E_UNEXPECTED;
        auto *format =
            static_cast<WAVEFORMATEX *>(CoTaskMemAlloc(sizeof(format_)));
        if (!format)
            return E_OUTOFMEMORY;
        *format = format_;
        *p = format;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE BufferSize(DWORD *p) override {
        if (!p)
            return E_POINTER;
        *p = 4096;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CodecName(wchar_t **p) override {
        return text(L"V2|Farbrausch V2 Module", p);
    }
    HRESULT STDMETHODCALLTYPE Bitrate(DWORD *p) override {
        if (!p)
            return E_POINTER;
        *p = (player_ && duration_)
                 ? dword(std::uint64_t(file_.size()) * 8000 / duration_)
                 : 0;
        return player_ ? S_OK : E_UNEXPECTED;
    }
    HRESULT STDMETHODCALLTYPE BitDepth(WORD *p) override {
        if (!p)
            return E_POINTER;
        *p = WORD(kBits);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCallback(IUnknown *p) override {
        LockGuard guard(&lock_);
        callback_.reset(p);
        if (p)
            p->AddRef();
        return apply_callback(p);
    }

    HRESULT STDMETHODCALLTYPE GetFormatInfo(ttp::format::Result *out) override {
        if (!out)
            return E_POINTER;
        if (out->size != sizeof(*out) || out->version != ttp::format::abi_version)
            return E_INVALIDARG;
        *out = ttp::format::Result{};
        if (!player_)
            return E_UNEXPECTED;
        if (!info_ready_)
            return S_FALSE;
        *out = info_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Start() override {
        return protect([&] {
            LockGuard guard(&lock_);
            require(player_ != nullptr, E_UNEXPECTED);
            abort_ = false;
            if (!started_) {
                player_->Play(position_ms_);
                started_ = true;
                tail_frames_ = 0;
                tail_done_ = false;
            }
            return S_OK;
        });
    }
    HRESULT STDMETHODCALLTYPE Stop() override {
        LockGuard guard(&lock_);
        abort_ = true;
        started_ = false;
        tail_frames_ = 0;
        tail_done_ = false;
        if (player_)
            player_->Stop(0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Read(Buffer *buffer) override {
        return protect([&] {
            LockGuard guard(&lock_);
            return read_locked(buffer);
        });
    }
    HRESULT STDMETHODCALLTYPE Seek(DWORD *p) override {
        return protect([&] {
            require(p != nullptr, E_POINTER);
            LockGuard guard(&lock_);
            require(player_ != nullptr, E_UNEXPECTED);
            DWORD target = std::min(*p, duration_);
            *p = target;
            position_ms_ = target;
            tail_frames_ = 0;
            tail_done_ = false;
            abort_ = false;
            player_->Play(target);
            started_ = true;
            return S_OK;
        });
    }

    HRESULT STDMETHODCALLTYPE Count(DWORD *p) override {
        if (!p)
            return E_POINTER;
        *p = 1;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE At(DWORD index, wchar_t **key,
                                 wchar_t **value) override {
        if (!key || !value)
            return E_POINTER;
        *key = *value = nullptr;
        if (index)
            return E_INVALIDARG;
        const HRESULT hr = text(L"Title", key);
        if (FAILED(hr))
            return hr;
        const HRESULT vr = text(title_, value);
        if (FAILED(vr)) {
            CoTaskMemFree(*key);
            *key = nullptr;
        }
        return vr;
    }
    HRESULT STDMETHODCALLTYPE Get(const char *key, wchar_t **value) override {
        if (!key || !value)
            return E_POINTER;
        *value = nullptr;
        if (_stricmp(key, "Title") != 0)
            return E_INVALIDARG;
        return text(title_, value);
    }
    HRESULT STDMETHODCALLTYPE Set(const char *, const wchar_t *) override {
        return E_NOTIMPL;
    }
};

HRESULT make_reader(void **out) {
    if (!out)
        return E_POINTER;
    *out = nullptr;
    return protect([&] {
        *out = static_cast<Reader *>(new V2Reader);
        return S_OK;
    });
}

} // namespace ttp
