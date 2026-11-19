#include "ttp_player_abi.h"

#include <shlwapi.h>
#include <shellapi.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

class MemoryBuffer final : public ttp::Buffer {
    LONG refs_{1};
    std::vector<BYTE> storage_;
    DWORD length_{};

public:
    MemoryBuffer() { storage_.resize(1 << 20); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
        if (!out)
            return E_POINTER;
        if (IsEqualIID(iid, IID_IUnknown)) {
            *out = static_cast<ttp::Buffer *>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
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
    HRESULT STDMETHODCALLTYPE SetLength(DWORD length) override {
        if (length > storage_.size())
            storage_.resize(length);
        length_ = length;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Capacity(DWORD *p) override {
        if (!p)
            return E_POINTER;
        *p = DWORD(storage_.size());
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Data(BYTE **p, DWORD *n) override {
        if (!p || !n)
            return E_POINTER;
        *p = storage_.data();
        *n = length_;
        return S_OK;
    }
};

#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    uint32_t riff_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t format;
    uint16_t channels;
    uint32_t rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits;
    char data[4];
    uint32_t data_size;
};
#pragma pack(pop)
static_assert(sizeof(WavHeader) == 44, "WAV header layout");

void print_wide(const char *label, wchar_t *value) {
    std::printf("%s: %ls\n", label, value ? value : L"(null)");
    if (value)
        CoTaskMemFree(value);
}

void patch_wav(FILE *f, uint64_t data_bytes) {
    const uint32_t data = uint32_t(data_bytes);
    const uint32_t riff = 36 + data;
    std::fseek(f, 4, SEEK_SET);
    std::fwrite(&riff, sizeof(riff), 1, f);
    std::fseek(f, 40, SEEK_SET);
    std::fwrite(&data, sizeof(data), 1, f);
}

} // namespace

int main() {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 4) {
        std::printf("usage: ttp_v2m_test <ttp_v2m.dll> <file.v2m> <out.wav> "
                    "[max_seconds]\n");
        return 2;
    }
    const double max_seconds = argc >= 5 ? _wtof(argv[4]) : 0.0;
    setvbuf(stdout, nullptr, _IONBF, 0);

    HMODULE library = LoadLibraryW(argv[1]);
    if (!library) {
        std::printf("LoadLibrary failed: %lu\n", GetLastError());
        return 3;
    }
    using GetAddIn = HRESULT(WINAPI *)(void **);
    auto get_addin =
        reinterpret_cast<GetAddIn>(GetProcAddress(library, "ttpGetSoundAddIn"));
    if (!get_addin) {
        std::printf("ttpGetSoundAddIn export missing\n");
        return 3;
    }

    ttp::AddIn *addin = nullptr;
    HRESULT hr = get_addin(reinterpret_cast<void **>(&addin));
    if (FAILED(hr) || !addin) {
        std::printf("ttpGetSoundAddIn failed: 0x%08lX\n", (unsigned long)hr);
        return 4;
    }

    GUID category{};
    void *created = nullptr;
    hr = addin->Enum(0, &category, &created);
    if (FAILED(hr) || !created || !IsEqualIID(category, ttp::cat_reader)) {
        std::printf("Enum failed: 0x%08lX\n", (unsigned long)hr);
        return 4;
    }
    auto *creator = static_cast<ttp::ReaderCreator *>(created);

    wchar_t *name = nullptr;
    wchar_t *extensions = nullptr;
    creator->Name(&name);
    creator->Extensions(&extensions);
    print_wide("reader", name);
    print_wide("extensions", extensions);

    void *reader_object = nullptr;
    hr = creator->Create(&reader_object);
    if (FAILED(hr) || !reader_object) {
        std::printf("Create failed: 0x%08lX\n", (unsigned long)hr);
        return 4;
    }
    auto *reader = static_cast<ttp::Reader *>(reader_object);

    IStream *stream = nullptr;
    hr = SHCreateStreamOnFileW(argv[2], STGM_READ | STGM_SHARE_DENY_WRITE,
                               &stream);
    if (FAILED(hr)) {
        std::printf("SHCreateStreamOnFileW failed: 0x%08lX\n",
                    (unsigned long)hr);
        return 4;
    }

    {
        STATSTG st{};
        const HRESULT stat_hr = stream->Stat(&st, STATFLAG_NONAME);
        LARGE_INTEGER zero{};
        ULARGE_INTEGER position{};
        const HRESULT seek_hr =
            stream->Seek(zero, STREAM_SEEK_SET, &position);
        BYTE probe[8]{};
        ULONG got = 0;
        const HRESULT read_hr = stream->Read(probe, sizeof(probe), &got);
        std::printf("stream: stat=0x%08lX size=%llu seek=0x%08lX pos=%llu "
                    "read=0x%08lX got=%lu\n",
                    (unsigned long)stat_hr,
                    (unsigned long long)st.cbSize.QuadPart,
                    (unsigned long)seek_hr,
                    (unsigned long long)position.QuadPart,
                    (unsigned long)read_hr, (unsigned long)got);
        stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    }

    hr = reader->Open(stream, 0);
    std::printf("Open: 0x%08lX\n", (unsigned long)hr);
    if (FAILED(hr))
        return 5;

    DWORD duration = 0;
    DWORD buffer_size = 0;
    DWORD bitrate = 0;
    WORD depth = 0;
    DWORD capabilities = 0;
    WAVEFORMATEX *format = nullptr;
    wchar_t *codec = nullptr;
    reader->Duration(&duration);
    reader->BufferSize(&buffer_size);
    reader->Bitrate(&bitrate);
    reader->BitDepth(&depth);
    reader->Capabilities(&capabilities);
    reader->Format(&format);
    reader->CodecName(&codec);
    print_wide("codec", codec);
    std::printf("duration: %lu ms\n", (unsigned long)duration);
    std::printf("buffer: %lu, bitrate: %lu, depth: %u, capabilities: %lu\n",
                (unsigned long)buffer_size, (unsigned long)bitrate, depth,
                (unsigned long)capabilities);
    if (format) {
        std::printf("format: tag=%u ch=%u rate=%lu align=%u bits=%u\n",
                    format->wFormatTag, format->nChannels,
                    (unsigned long)format->nSamplesPerSec,
                    format->nBlockAlign, format->wBitsPerSample);
        CoTaskMemFree(format);
    }

    DWORD metadata_count = 0;
    reader->QueryInterface(ttp::iid_metadata,
                           reinterpret_cast<void **>(&created));
    auto *metadata = static_cast<ttp::Metadata *>(created);
    if (metadata) {
        metadata->Count(&metadata_count);
        for (DWORD i = 0; i < metadata_count; ++i) {
            wchar_t *key = nullptr;
            wchar_t *value = nullptr;
            if (SUCCEEDED(metadata->At(i, &key, &value))) {
                std::printf("metadata[%lu]: %ls = %ls\n", (unsigned long)i,
                            key, value);
                CoTaskMemFree(key);
                CoTaskMemFree(value);
            }
        }
    }

    reader->Start();

    FILE *wav = _wfopen(argv[3], L"wb");
    if (!wav) {
        std::printf("cannot open output WAV\n");
        return 6;
    }
    WavHeader header{};
    std::memcpy(header.riff, "RIFF", 4);
    std::memcpy(header.wave, "WAVE", 4);
    std::memcpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.format = 1;
    header.channels = 2;
    header.rate = 44100;
    header.byte_rate = 44100 * 2 * 2;
    header.block_align = 4;
    header.bits = 16;
    std::memcpy(header.data, "data", 4);
    std::fwrite(&header, sizeof(header), 1, wav);

    MemoryBuffer buffer;
    uint64_t decoded = 0;
    long double sum_squares = 0.0;
    double peak = 0.0;
    uint64_t samples = 0;
    bool aborted = false;
    bool truncated = false;
    const uint64_t limit =
        max_seconds > 0 ? uint64_t(max_seconds * 44100.0) * 4 : UINT64_MAX;

    for (;;) {
        buffer.SetLength(0);
        hr = reader->Read(&buffer);
        if (hr == S_FALSE)
            break;
        if (FAILED(hr)) {
            std::printf("Read failed: 0x%08lX\n", (unsigned long)hr);
            aborted = true;
            break;
        }
        BYTE *data = nullptr;
        DWORD length = 0;
        buffer.Data(&data, &length);
        std::fwrite(data, 1, length, wav);
        for (DWORD i = 0; i + 1 < length; i += 2) {
            int16_t sample = 0;
            std::memcpy(&sample, data + i, 2);
            const double value = sample / 32768.0;
            sum_squares += value * value;
            peak = std::max(peak, std::fabs(value));
            ++samples;
        }
        decoded += length;
        if (decoded >= limit) {
            truncated = true;
            break;
        }
    }
    patch_wav(wav, decoded);
    std::fclose(wav);

    std::printf("decoded: %llu bytes, %.2f s, peak=%.4f, rms=%.4f\n",
                (unsigned long long)decoded,
                double(decoded) / double(44100 * 2 * 2), peak,
                samples ? std::sqrt(double(sum_squares / samples)) : 0.0);

    if (!aborted && !truncated) {
        DWORD seek = duration > 12000 ? 10000 : duration / 2;
        const HRESULT seek_hr = reader->Seek(&seek);
        BYTE *data = nullptr;
        DWORD length = 0;
        buffer.SetLength(0);
        const HRESULT read_hr = reader->Read(&buffer);
        buffer.Data(&data, &length);
        double seek_peak = 0.0;
        for (DWORD i = 0; i + 1 < length; i += 2) {
            int16_t sample = 0;
            std::memcpy(&sample, data + i, 2);
            seek_peak =
                std::max(seek_peak, std::fabs(double(sample) / 32768.0));
        }
        std::printf("seek(%lu): 0x%08lX, read: 0x%08lX, peak=%.4f\n",
                    (unsigned long)seek, (unsigned long)seek_hr,
                    (unsigned long)read_hr, seek_peak);
        if (FAILED(seek_hr) || FAILED(read_hr) || seek_peak <= 0.0001)
            aborted = true;
    }

    reader->Stop();
    if (metadata)
        metadata->Release();
    reader->Release();
    stream->Release();
    creator->Release();
    addin->Release();
    FreeLibrary(library);
    LocalFree(argv);

    const bool ok = !aborted && decoded > 0 && duration > 0 && peak > 0.001;
    std::printf("result: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 7;
}
