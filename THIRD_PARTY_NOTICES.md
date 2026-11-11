# 第三方来源与许可

## ESP32 固件

本仓库原创固件源码使用 [MIT License](LICENSE)。构建通过 PlatformIO 获取以下组件，第三方代码不因本仓库许可证而改变授权：

| 组件 | 固定版本 / 来源 | 许可 |
| --- | --- | --- |
| PlatformIO Espressif32 平台 | 7.0.1，https://github.com/platformio/platform-espressif32 | Apache-2.0 |
| Arduino-ESP32 | 平台解析到 Arduino 2.0.17，https://github.com/espressif/arduino-esp32 | LGPL-2.1；附带 SDK 组件按各自许可 |
| ESP-IDF | Arduino-ESP32 随附，https://github.com/espressif/esp-idf | Apache-2.0 及组件自带许可 |
| ArduinoJson | 7.4.3，https://github.com/bblanchon/ArduinoJson | MIT |

固件包同时包含本机构建框架的许可文本；精确源码版本和获取方式记录在附件 `SOURCE.txt`，原项目保留完整组件许可。

## Windows 软件

电脑程序暂只发布可执行包，不适用本仓库原创固件的 MIT 授权；允许下载、运行和原样分享完整发布包，保留随包说明及第三方许可证。该说明不限制第三方组件自身授予的权利。

精简版使用 Python、Tcl/Tk、CustomTkinter、darkdetect、packaging、pyserial、Pillow，以及 PyInstaller 运行时；相关许可随 Windows ZIP 的 `licenses` 目录提供。系统 H.264 编码使用 Windows Media Foundation，不随包携带 FFmpeg、x264、x265、NumPy 或系统字体。PyInstaller 使用其原有许可及运行时分发例外。

## 协议与参考

- 游戏字段格式参考 [Forza 官方 Data Out 文档](https://support.forza.net/hc/en-us/articles/51744149102611-Forza-Horizon-6-Data-Out-Documentation)。
- 配网流程参考 [SlimeVR Quick Setup](https://docs.slimevr.dev/quick-setup)。
- 九号仪表私有视频报文布局参考维护者本地 `ninebot_pro` 项目 `NinebotProtocol.java`；本仓库不包含该 Android 工程或电脑端协议实现源码。
- Windows 系统编码接口参考 [Microsoft H.264 Video Encoder](https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-encoder)。

Forza、九号及相关商标属于各自权利人。本项目是独立实验项目。
