# ForzaDash

[下载电脑软件与 ESP32 固件（Latest）](https://github.com/xmz28/ForzaDash/releases/latest) · [安装与升级](docs/INSTALL.md) · [版本记录](CHANGELOG.md)

把 Forza 游戏遥测显示到九号五寸仪表：Windows 软件通过 Wi-Fi 绘制并投送仪表画面，也可通过 ESP32 输出 CAN，让原生速度、功率和挡位跟随游戏。

当前发布组合为 **ForzaDash Desktop 0.5 精简版 + ESP32 固件 0.2.0**。已验证 KZ110 独立供电台架与模拟游戏数据；FH4/FH5/FH6 真实游戏仍待分别验证，不保证其他仪表型号兼容。

## 下载与使用

在 [Latest Release](https://github.com/xmz28/ForzaDash/releases/latest) 下载：

| 文件 | 用途 |
| --- | --- |
| `ForzaDash-v0.5.0-lite-windows-x64.zip` | Windows 便携软件，解压运行 `ForzaDash-lite.exe` |
| `ForzaDash-esp32-v0.2.0.zip` | ESP32 完整烧录包与保留配置的升级文件 |
| `SHA256SUMS.txt` | 发布附件校验值 |
| `SOURCE.txt` | 固件源码提交和电脑软件版本对应关系 |

电脑软件约 13.62 MiB，使用 Windows 自带 Media Foundation H.264 编码器，无需安装 Python、FFmpeg 或额外编码软件。当前验证环境为 Windows 10 x64；缺少系统媒体组件时，投屏编码会报告初始化失败。

本仓库公开 **ESP32 固件源码（MIT）**；电脑软件暂只发布可执行程序，其使用说明和第三方许可证随包提供。固件与电脑软件使用各自的版本号，同一 Release 提供当前配套组合。

## 三种使用方式

| 方式 | 数据路径 | 需要 ESP32 |
| --- | --- | --- |
| Wi-Fi 投屏 | 游戏 → 电脑绘图 → 仪表 Wi-Fi | 否 |
| USB CAN | 游戏 → 电脑 → USB → ESP32 → CAN 仪表 | 是 |
| 纯 Wi-Fi CAN | 游戏 → 局域网 UDP → ESP32 → CAN 仪表 | 是 |

投屏可以与一种 CAN 输出同时使用。电脑 Wi-Fi 连着仪表时，搭配 USB CAN 更方便；Wi-Fi CAN 需要电脑同时能够访问 ESP32 所在网络。

### Wi-Fi 投屏

1. 仪表通电，用 iPhone 原 App 连接，在 iPhone 系统中查看仪表网络名称和密码。
2. 在 Windows Wi-Fi 设置中连接并保存该网络。
3. 打开软件，在欢迎页选择是否使用 ESP32，进入“Wi-Fi 投屏”选择自己的已连接仪表。
4. 先选“测试数值”，开始投屏，检查速度、RPM 和挡位跟随。
5. 接游戏时选择“游戏 UDP”，游戏 Data Out 开启，目标 IP 填 `127.0.0.1`，端口填 `12345`（或与软件一致的自定义端口）。

软件不内置仪表密码，不读取系统明文密码，也不替用户切换网络。首次连接详见 [连接说明](docs/FIRST_CONNECTION.md)。这一路是九号仪表私有视频传输，不是 Windows Miracast。

### USB CAN

1. 按 [安装说明](docs/INSTALL.md) 烧录 ESP32，并连接 CAN 收发器与仪表。
2. 软件选择 USB 串口并连接，在“使用方式”选择 USB 输出。
3. 游戏目标填写 `127.0.0.1:12345`，启动有线中继，游戏期间保持软件运行。

不需要无线网卡。电脑只监听本机回环；超过 0.5 秒没有新游戏数据时归零并等待恢复。

### 纯 Wi-Fi CAN

1. 首次通过 USB 连接，在软件配网页配置 ESP32 的 2.4 GHz 网络。
2. 连接成功后选择“完成，启用纯 Wi-Fi”。
3. 游戏目标填写 ESP32 的 IP 和端口 `12345`，电脑软件可以关闭。

Wi-Fi、输入方式和速度比例保存在 ESP32 NVS，重启恢复。默认固件没有写死的网络名称或密码。USB 始终可用于配置和诊断。

## CAN 默认映射

| 游戏数据 | 原生仪表输入 |
| --- | --- |
| 200 km/h，原速 | 200 km/h |
| 200 km/h，缩小 10 倍 | 20 km/h |
| 0 / 4000 / 8000 RPM | 0 / 50 / 100% 功率 |
| 1、2 挡 / 3、4 挡 / 5 挡以上 | E / C / F |

速度比例只影响 CAN，投屏仍显示游戏原速。RPM 超过 8000 时功率保持 100%；原始挡位 0 暂按 E 处理。当前测试仪表原生主题速度数字最高显示 25 km/h，内部速度遥测可更高，投屏数字不受此上限限制。

自定义 CAN 映射修改 `include/game_mapping.h`。ESP32 有效运动输入停止更新超过 3 秒时归零；比赛标志为 0 时立即归零。

## 硬件与构建

固件目标为 **普通 ESP32 Dev Module（esp32dev，4 MB Flash）**，不是 ESP32-S3。需要外部 CAN 收发器：GPIO21 → TXD，GPIO22 ← RXD，500 kbit/s。CANH/CANL、公共地、收发器逻辑电平和供电按实际模块连接。仪表接插件针脚与供电电压尚未整理为公开接线图，请以已确认的实际硬件资料为准。

安装 PlatformIO 后，在项目根目录执行：

```powershell
pio run
pio run -t upload --upload-port COM7
```

`COM7` 替换为实际端口。平台和 ArduinoJson 版本固定在 `platformio.ini`。完整固件首次安装及保留配置升级见 [安装与升级](docs/INSTALL.md)。

## 协议、验证与来源

- [USB / UDP、CAN 协议与开发说明](docs/PROTOCOL.md)
- [验证记录及已知限制](docs/VALIDATION.md)
- [第三方来源与许可](THIRD_PARTY_NOTICES.md)
- [关联项目 DiPlay-Android](https://github.com/xmz28/DiPlay-Android)

当前仅解析 324 字节 Horizon 数据布局，格式参考 [Forza 官方 Data Out 文档](https://support.forza.net/hc/en-us/articles/51744149102611-Forza-Horizon-6-Data-Out-Documentation)。模拟数据验证不等于所有真实游戏版本兼容；长时间运行、其他仪表和系统版本仍待验证。源码与 Release 不包含用户现场 Wi-Fi 凭据或抓包记录。
