# TTS 文本朗读工具

通用文本转语音桌面工具，支持 OpenAI 兼容 API，使用 C++17 和 Qt6 开发。
既可以用默认的小米 MiMo 云端服务，也可以接入本地部署的 TTS 服务。

## 功能

- 文本转语音合成
- 音色复刻（上传参考音频克隆声音，MiMo 协议）
- 风格控制（自然语言描述语气风格）
- 两种接口协议：
  - **MiMo `/chat/completions`** —— 音频以 base64 内嵌在 JSON 里
  - **OpenAI `/audio/speech`** —— 响应体是原始音频字节，本地服务通用
- 本地部署服务支持（不需要 API Key）
- 实时播放
- 保存音频文件（统一保存为 WAV）
- 动态获取可用模型列表
- 自定义音色管理
- 配置持久化

## 依赖

- Qt 6.7+ (Core, Gui, Widgets, Network, Multimedia)
- CMake 3.16+

## 编译

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build
```

## 运行

```bash
./build/tts            # Linux / Windows
./build/tts.app/Contents/MacOS/tts   # macOS
```

## 接入本地部署的 TTS 服务

「API 配置」里选一个预设就能自动填好地址、协议、模型和音色，也可以全部手动填写。

| 预设 | 默认地址 |
| --- | --- |
| 小米 MiMo（云端） | `https://api.xiaomimimo.com/v1` |
| Speaches / openedai-speech | `http://127.0.0.1:8000/v1` |
| Kokoro-FastAPI | `http://127.0.0.1:8880/v1` |
| LocalAI | `http://127.0.0.1:8080/v1` |
| LM Studio | `http://127.0.0.1:1234/v1` |

要点：

- **本地 / 内网地址不需要 API Key**。识别范围包括 `localhost`、`127.0.0.1`、`::1`、
  `*.local`、`10.x`、`192.168.x`、`172.16–31.x`、`169.254.x` 以及 IPv6 `fc00::/7`、`fe80::`。
- **接口协议**要和服务端一致。本地服务基本都是 OpenAI 标准的 `/audio/speech`，
  选 `OpenAI /audio/speech`；小米 MiMo 选 `MiMo /chat/completions`。
- **模型名可以手填**。本地服务的模型名往往不在下拉列表里，直接输入即可；
  点「刷新」会请求 `<API 地址>/models` 拉取列表。
- **「仅 TTS」开关**默认勾选，只显示模型名里带 `tts` 的条目。
  本地服务的模型名常常不含 `tts`（如 `kokoro`、`tts-1`），必要时取消勾选。
- **音色复刻**依赖 MiMo 私有的 `voiceclone` 协议，切到 `/audio/speech` 时不可用。

### 验证本地服务是否连通

```bash
curl http://127.0.0.1:8880/v1/models
```

## 音频格式

| 选项 | 发给服务端的参数 | 本地表现 |
| --- | --- | --- |
| `wav` | `wav` / `response_format=wav` | 直接保存，兼容性最好 |
| `pcm16` | `pcm16` / `response_format=pcm` | 裸 PCM（24kHz 单声道 16bit），本地自动补 WAV 头 |

两种格式最终都保存为 `.wav`，因为播放器和大多数工具无法直接识别无容器头的裸 PCM。

## 配置

配置文件保存在 `~/.tts_gui_config.json`，包含：

- API 地址、API Key、接口协议（`chat` / `speech`）
- 模型、声音、输出格式
- 是否只显示 TTS 模型
- 自定义音色列表

旧版本生成的配置文件可以直接沿用，缺失的字段会用默认值补齐。

## 许可证

MIT
