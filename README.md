# TTS 文本朗读工具

通用文本转语音桌面工具，支持 OpenAI 兼容 API，使用 C++17 和 Qt6 开发。默认配置为小米 MiMo TTS。

## 功能

- 文本转语音合成
- 音色复刻（上传参考音频克隆声音）
- 风格控制（自然语言描述语气风格）
- 支持多种声音模型
- 实时播放
- 保存音频文件（WAV/PCM）
- 动态获取可用模型列表
- 自定义音色管理
- 配置持久化

## 依赖

- Qt 6.x (Core, Gui, Widgets, Network, Multimedia)
- CMake 3.16+

## 编译

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build
```

## 运行

```bash
./build/tts
```

## 配置

配置文件保存在 `~/.tts_gui_config.json`，包含：
- API 地址（默认 `https://api.xiaomimimo.com/v1`）
- API Key
- 模型选择
- 声音选择
- 输出格式
- 自定义音色列表

## 支持的 API

兼容 OpenAI TTS API 格式的平台，例如：
- 小米 MiMo（默认）
- 其他兼容 OpenAI 格式的 TTS 服务

## 许可证

MIT
