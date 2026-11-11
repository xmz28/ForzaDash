# 安装与升级

从 [Latest Release](https://github.com/xmz28/ForzaDash/releases/latest) 下载电脑软件和 ESP32 固件。固件适用于 `esp32dev` / 普通 ESP32、4 MB Flash，GPIO21 TX、GPIO22 RX、CAN 500 kbit/s；请勿烧到 ESP32-S3 等其他芯片。

## Windows 电脑软件

解压 `ForzaDash-v0.5.0-lite-windows-x64.zip`，运行 `ForzaDash-lite.exe`。保留随包的说明与 `licenses` 目录。无需安装 Python 或第三方编码器；投屏需要 Windows Media Foundation 和可用 Wi-Fi。当前只验证 Windows 10 x64。USB CAN 需要系统正常识别开发板串口。

电脑软件暂不公开源码，固件仓库的 MIT 许可证不涵盖电脑程序本身；第三方组件保留各自许可。Windows 系统字体和媒体组件由系统提供，不随包分发。

## ESP32 首次安装

完整包包含 `ForzaDash-esp32-v0.2.0-full.bin`，烧录地址为 **0x0**。它合并启动加载器、分区表、OTA 初始化数据和应用，不是用户开发板 Flash 备份。

可使用 Espressif Flash Download Tool：芯片 ESP32，SPI 模式 DIO、40 MHz，文件地址 `0x0`。也可使用 esptool 4.x：

```powershell
python -m pip install "esptool>=4,<5"
python -m esptool --chip esp32 --port COM7 --baud 460800 write_flash 0x0 ForzaDash-esp32-v0.2.0-full.bin
```

替换 `COM7` 为实际串口。首次安装的完整镜像会覆盖镜像覆盖范围内的 NVS 配置；已有配置请使用下面的升级方式。烧录后复位，再通过软件 USB 配网。开发板不能自动进入下载模式时，按开发板要求使用 BOOT / EN。

## 保留设置升级

从本项目相同分区布局升级时，只写 `firmware.bin`，地址 **0x10000**：

```powershell
python -m esptool --chip esp32 --port COM7 --baud 460800 write_flash 0x10000 firmware.bin
```

不执行 `erase_flash`，不写完整镜像或分区表，保留 NVS 中 Wi-Fi、输入方式和速度比例。从其他项目或不同分区布局迁移，请使用首次安装方式。此版本没有提供网络 OTA 升级入口。

完整包还包含分段文件：`bootloader.bin` @ `0x1000`、`partitions.bin` @ `0x8000`、`boot_app0.bin` @ `0xe000`、`firmware.bin` @ `0x10000`。

## 校验与版本

```powershell
Get-FileHash .\ForzaDash-esp32-v0.2.0.zip -Algorithm SHA256
Get-FileHash .\ForzaDash-v0.5.0-lite-windows-x64.zip -Algorithm SHA256
```

与 Release 的 `SHA256SUMS.txt` 比较。`SOURCE.txt` 记录固件源码提交、桌面版本和打包信息。固件通过 USB `status` 返回 `firmware_version`；本次固件 0.2.0、电脑软件 0.5.0，版本号独立。
