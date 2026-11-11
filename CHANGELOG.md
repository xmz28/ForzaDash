# 版本记录

## v0.5.0 · 2026-11-11

首次公开发布：桌面 0.5.0 精简版 + ESP32 固件 0.2.0。

- 公开 ESP32 MIT 源码，Windows 软件只提供便携二进制。
- Windows 使用系统 Media Foundation 编码，EXE 约 13.62 MiB，不携带 PyAV、FFmpeg 或 NumPy。
- 提供 Wi-Fi 投屏、USB CAN、纯 Wi-Fi CAN；投屏可以与一种 CAN 输出共享游戏数据。
- CAN 支持原速 / 十倍缩小、RPM 功率映射、E/C/F 挡位映射、USB 配网及配置持久化。
- Release 提供完整 ESP32 镜像、单独应用升级文件、Windows ZIP 和 SHA-256 校验。

已验证 KZ110 台架与模拟游戏数据。真实 FH4/FH5/FH6、其他仪表型号、其他 Windows 版本及长时间稳定性仍待验证。此次发布整理不更改固件运行逻辑。
