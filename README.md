# MiMo TTS 文本朗读工具

基于小米 MiMo TTS API 的文本转语音桌面工具，使用 C++17 和 Qt6 开发。

## 功能

- 文本转语音合成
- 支持多种声音模型
- 风格控制（自然语言描述语气风格）
- 实时播放
- 保存音频文件
- 动态获取可用模型列表
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
./build/mimo_tts
```

## 配置

配置文件保存在 `~/.tts_gui_config.json`，包含：
- API 地址
- API Key
- 模型选择
- 声音选择
- 输出格式

## 声音列表

- mimo_default
- 冰糖
- 茉莉
- 苏打
- 白桦
- Mia
- Chloe
- Milo
- Dean

## 许可证

MIT
