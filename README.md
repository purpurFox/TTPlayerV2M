# TTPlayerV2M

为 [千千静听社区版](https://github.com/Sonic853/TTPlayer) 编写的
Farbrausch V2（.v2m）解码器，使用 TTPlayer 的 Sound AddIn ABI
（导出 ttpGetSoundAddIn）。

![TTPlayer 播放 pzero_new](docs/ttplayer.png)

## 状态

- 解码器：ttp_v2m.dll（Win32，i386）
- 构建工具链：MSVC x86 + Windows SDK，通过 msvc-wine 和 Wine 运行
- 宿主接口：ttpGetSoundAddIn → AddIn → ReaderCreator → Reader / Metadata
- 扩展接口：工厂 Probe、Reader SourceInfo、Reader SetCallback
- 输出：固定 44100 Hz / 16-bit / 双声道
- 合成器：官方 synth.asm（NASM），与 Winamp 官方解码器同源
- 语音合成：RONAN 默认启用；ronan.cpp 使用可移植的
  lrintf / pow / exp 数学实现，避免 MSVC 下的 NaN
- 支持旧版 V2M 的版本检测与转换（v2mconv）
- 支持时长、Seek、Title 元数据
- 对循环型 V2M 强制在报告时长处返回 S_FALSE，不会无限播放
- 使用 .def 导出：ttpGetSoundAddIn=_ttpGetSoundAddIn@4 @1

## 构建

依赖：

- CMake ≥ 3.25
- Ninja
- MSVC x86（本机 14.52.37025）
- Windows SDK 10.0.26100.0
- NASM（synth.asm）
- Wine 8（本机 GE-Proton8-30-rtsp-3 自带的 Wine 8.0 Staging）

本机工具链安装在 $HOME/Library/.msvc，Wine prefix 为
$HOME/Library/.msvc/.wine_8.0。先激活环境再构建：

    source $HOME/Library/.msvc/activate-x86.sh
    ./tools/build.sh

等价命令：

    source $HOME/Library/.msvc/activate-x86.sh
    cmake -S . -B build -G Ninja \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_SYSTEM_PROCESSOR=x86 \
      -DCMAKE_C_COMPILER=cl \
      -DCMAKE_CXX_COMPILER=cl \
      -DCMAKE_RC_COMPILER=rc \
      -DCMAKE_ASM_NASM_COMPILER=nasm \
      -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded \
      -DCMAKE_BUILD_TYPE=Release
    cmake --build build

产物：

- build/ttp_v2m.dll
- build/ttp_v2m_test.exe（ABI/Probe 测试宿主）

## 安装

把 build/ttp_v2m.dll 复制到千千静听的 AddIn 目录：

    cp build/ttp_v2m.dll $HOME/Programs/TTPlayer/AddIn/

重启播放器后，.v2m 文件会由 Farbrausch V2 Reader 打开。

## 测试

    ./tests/run.sh

测试宿主通过真实的 ttpGetSoundAddIn 入口加载 DLL，验证
AddIn → ReaderCreator → Probe/Reader/Metadata/SourceInfo，
把音频写成 WAV 到 $HOME/.cache/ttpv2m。

本机结果（MSVC 构建，Wine 8）：

| 文件 | 报告时长 | 实际解码 | 峰值 | RMS | 结果 |
| --- | ---: | ---: | ---: | ---: | --- |
| pzero_new.v2m | 232806 ms | 233.83 s / 41,247,264 B | 1.0000 | 0.2001 | PASS |
| v2_zeitmaschine_new.v2m | 166536 ms | 166.95 s / 29,450,784 B | 1.0000 | 0.1225 | PASS |

Probe/SourceInfo 测试输出：

    probe: hr=0x00000000 confidence=3 family=65536 codec=v2m ext=.v2m
    info:  hr=0x00000000 confidence=4 family=65536 codec=v2m ext=.v2m

语音测试 V2mPlayer--fr-037_the_code_inside.v2m：
30 秒，peak=1.0000，rms=0.2902，PASS。

网络流测试使用：

    https://gifx.co/music/Farbrausch%20V2%2FDafunk%2Fscream.v2m

TTPlayer 能打开该 URL 并显示时长 4:35；当前截图中播放进度仍为
00:00，尚未确认实际播放推进。标题保留 URL 百分号编码。

### 与官方解码器对齐

用 Winamp 2 宿主加载官方 in_v2m.dll，与 ttp_v2m.dll 解码
iOTA-MTop_HTML_Password_Lock_5.0_nfo_1.v2m 的 PCM 逐样本比较：

- 全曲相关系数：0.99999+
- 相对 PCM 差异：约 1.1e-4
- 各频段能量比：1.0000x（20 Hz–22 kHz）

## 结构

    include/ttp_player_abi.h                TTPlayer Sound AddIn 接口
    include/ttpcomm/audio_format_probe.h    Probe/SourceInfo 协议
    src/plugin.cpp                          ttpGetSoundAddIn / AddIn / ReaderCreator / Probe
    src/reader.cpp                          Reader + Metadata + SourceInfo + SetCallback
    src/common.h                            COM 辅助、HRESULT 保护、IStream 读取
    src/v2m/synth.asm                       官方 ASM 合成器（NASM）
    src/v2m/ronan.cpp                       RONAN 语音（可移植数学）
    src/v2m/v2mplayer.cpp                   上游 MSVC 播放器
    src/v2m/v2mconv.cpp                     V2M 版本转换
    src/v2m/v2init.cpp                      v2mconv 版本/尺寸表
    src/ttp_v2m.def                         MSVC 导出表
    tests/test_host.cpp                     ABI/Probe/SourceInfo 测试宿主
    tests/data/                             示例 V2M
    docs/ABI.md                             接口与行为说明
    tools/build.sh                          MSVC 构建脚本
    tools/package.sh                        生成 dist/ 与 SHA256

## 上游说明

src/v2m/ 来自
[farbrausch/fr_public](https://github.com/farbrausch/fr_public/tree/master/v2)
的 v2 目录，原始代码为 public domain（见 src/v2m/LICENSE.txt）。

- v2mplayer.cpp、v2mplayer.h、types.h 已恢复为上游 MSVC 版本，
  使用原始 __asm 和 __int64。
- synth.asm 使用原始 RONAN 配置，由 NASM 汇编。
- ronan.cpp 的 MSVC 内联汇编在当前 Wine/MSVC 组合下会产生 NaN，
  因此改为可移植的 lrintf / pow / exp 数学实现；这不是 MinGW
  专用代码。
- v2init.cpp 重建 v2mconv 需要的版本/尺寸表。
- reader.cpp 负责 IStream 读取、V2M 转换、时长、Seek、
  SourceInfo、SetCallback 和 EOF 处理。

## 已知限制

- 仅注册 .v2m，不处理旧式 .v2 或其他 tracker 格式。
- 打开时把整个流读入内存，单文件上限 256 MiB；网络流会先完整读取。
- 固定 44.1 kHz / 16-bit / stereo，不实现 flags & 2 的浮点输出。
- 未实现 Thumbnail 和 MetadataCommit；V2M 为只读。
- RONAN 语音波形尚未与官方逐位一致；非语音音色保持 ASM 对齐。
- 网络 URL 已能打开并识别时长，但实际播放进度仍需进一步验证。
- 无配置界面。

## 许可

- 本仓库新增代码（插件、ABI/Probe 头、测试宿主、脚本）：MIT，
  见 LICENSE。
- src/v2m/：Farbrausch V2 public domain，见 src/v2m/LICENSE.txt。
- TTPlayer AddIn 的 GUID 与方法布局来自 TTPlayerRebuild 参考插件
  的实际 ABI；include/ttp_player_abi.h 为重新编写的接口声明。
