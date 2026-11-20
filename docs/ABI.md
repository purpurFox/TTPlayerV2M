# TTPlayer Sound AddIn ABI

The decoder exports one undecorated function:

```c
HRESULT WINAPI ttpGetSoundAddIn(void **out);
```

`out` receives an `ttp::AddIn`. The host calls `Enum(0, &category, &object)`;
for a reader plugin `category` is `ttp::cat_reader` and `object` is an
`ttp::ReaderCreator`.

## GUIDs

| Name | Value |
| --- | --- |
| `iid_addin` | `eec6c534-feba-421e-aa5d-10ac66f98784` |
| `iid_reader` | `30c7c165-c0a9-4204-995d-456a764998fb` |
| `iid_metadata` | `7ad84e00-5fef-4481-b532-fbbd677e67c2` |
| `cat_reader` | `476d15a5-d863-416a-8a59-a9c7d72ce04e` |

## Object lifetimes

- Every interface returned by `QueryInterface` has already been `AddRef`ed.
- `ReaderCreator::Create` returns a new `Reader` with one reference.
- Strings returned through `wchar_t **` are allocated with
  `CoTaskMemAlloc`; the host owns and frees them.
- `Format` returns a `WAVEFORMATEX` allocated with `CoTaskMemAlloc`.
- `Buffer` is owned by the host. `Data` returns the host storage and the
  current length; `Capacity` returns the allocation size. The plugin calls
  `SetLength` before and after filling.

## Reader behaviour

| Method | Behaviour |
| --- | --- |
| `Open(IStream *, flags)` | Buffers the whole stream (≤ 256 MiB), validates/converts V2M, initialises the synth. Flags are ignored. |
| `Capabilities` | `2` (seekable). |
| `Duration` | Milliseconds from the V2M tempo map, plus a 2 second tail. |
| `Format` | PCM, 2 channels, 44100 Hz, 16 bit, block align 4. |
| `BufferSize` | 4096 bytes. |
| `CodecName` | `V2|Farbrausch V2 Module`. |
| `Bitrate` | `file_size * 8000 / duration`. |
| `BitDepth` | 16. |
| `SetCallback` | Stores the host callback and forwards it to the stream when the stream exposes the callback interface (`1717a4a7-a3dc-416b-a297-7216749bd7bf`). Returns `S_OK` when the stream has no such interface. |
| `Start` | Clears the abort flag and starts/restarts the synth at the current position. |
| `Stop` | Sets the abort flag; subsequent `Read` returns `E_ABORT` until `Start`/`Seek`. |
| `Read` | Renders PCM; returns `S_FALSE` at end of track/tail. |
| `Seek` | Clamps to duration, restarts the synth at the requested millisecond. |

`Read` enforces the reported duration even for V2M files whose event stream
would otherwise loop forever. After the synth stops, up to two seconds of
release tail is rendered before `S_FALSE`; a one-shot `tail_done` flag
prevents the tail from restarting.

## Export decoration

The DLL is built as 32-bit PE with MSVC x86:

- `extern "C" HRESULT WINAPI ttpGetSoundAddIn(void **)`
- export table entry in `src/ttp_v2m.def`:
  `ttpGetSoundAddIn=_ttpGetSoundAddIn@4 @1`

This produces the undecorated export `ttpGetSoundAddIn`, matching the
original `ttp_mod.dll` / `ttp_flac.dll` convention.

## Optional probe interfaces

| Interface | GUID | Purpose |
| --- | --- | --- |
| `ttp::format::Probe` | `7e892943-481a-43db-a89b-5993f50b6049` | Header-only format probe on the `ReaderCreator`. |
| `ttp::format::SourceInfo` | `b58ee1a2-812c-4920-920e-5be9379f3132` | Formats decided after `Open`, queried on the `Reader`. |

Both are optional: hosts may query them with `QueryInterface` and must
handle `E_NOINTERFACE`.
