# forza-dash

**Forza UDP → ESP32 → 九号CAN仪表**

九号5寸TFT仪表中继固件，配套Windows便携程序 **ForzaDash**。当前实测仪表为KZ110，固件版本0.2.0。

## 两种使用方式

| 模式 | 数据路径 | 电脑软件 |
| --- | --- | --- |
| 纯Wi-Fi | 游戏 → 局域网UDP → ESP32 → 仪表 | 首次串口配网，完成后可关闭 |
| 有线 + 软件 | 游戏 → 127.0.0.1 UDP → ForzaDash → USB串口 → ESP32 → 仪表 | 游戏期间保持运行 |

有线模式不需要无线网卡，也不需要局域网设备互通。电脑UDP监听仅绑定127.0.0.1。两种模式在ESP32使用同一套映射。

默认无写死的Wi-Fi名称或密码。网络、输入模式、速度比例保存在NVS，重启恢复。串口一直可用于配置与诊断。

## 默认映射

| 游戏数据 | 仪表输入 |
| --- | --- |
| 速度200 km/h，原速 | 200 km/h |
| 速度200 km/h，缩小10倍 | 20 km/h |
| 转速0 / 4000 / 8000 RPM | 功率0 / 50 / 100% |
| 游戏1、2挡 | E |
| 游戏3、4挡 | C |
| 游戏5挡以上 | F |

速度默认除数1，可选10。RPM超过8000时功率保持100%，功率显示不再取游戏Power字段。游戏原始挡位0暂按E处理，不自动映射P/H/R。

自定义请修改`include/game_mapping.h`。当前测试仪表屏幕最高显示25 km/h，内部速度遥测可超过25；小数速度可能取整。

## 快速开始

1. 编译烧录，或使用已编译的固件。
2. 打开ForzaDash，选择USB串口并连接设备。
3. 进入模拟测试，调整速度、RPM和挡位。
4. 纯Wi-Fi：配网页选择2.4GHz网络，连接成功后点击“完成，启用纯Wi-Fi”；游戏填设备IP、端口12345。
5. 有线：游戏填127.0.0.1及软件监听端口（默认12345），启动有线中继。

配网流程参考[SlimeVR Quick Setup](https://docs.slimevr.dev/quick-setup)。软件支持电脑2.4GHz扫描、设备扫描和手动输入。网络名称按原始SSID传递，不从附加信号文字中截取。电脑缺少扫描权限或无线网卡时仍可手动输入。

## 硬件与编译

ESP32开发板和外部CAN收发器；GPIO21=TX、GPIO22=RX，500 kbit/s。当前实测为独立供电仪表台架。CAN接线、电源与终端电阻按实际硬件配置。

```powershell
pio run
pio run -t upload --upload-port COM7
```

0.2.0已在COM7开发板烧录并验证。固件启动不会等待Wi-Fi连接，网络不可用时USB仍工作。获取IP后重新绑定UDP端口。

## 游戏格式

只接收324字节Horizon布局，字段采用小端：速度offset256，单位m/s；RPM offset16；游戏挡位offset319；比赛标志offset0。参照[FH6官方Data Out文档](https://support.forza.net/hc/en-us/articles/51744149102611-Forza-Horizon-6-Data-Out-Documentation)，也与旧FH_5中继的已用偏移一致。

模拟器已经打通两条实机链路；FH4、FH5、FH6真实游戏仍需分别验证。其他长度、非有限数值和越界数值会被拒绝。

有线中继只取最新包，约100ms转发一次；超过0.5秒无新数据时归零，保留监听等待恢复。ESP32所有有效运动输入超过3秒未更新也会归零。比赛标志为0时立即归零。

## USB / UDP接口

USB115200，每行JSON、LF结尾，输入最长512字节。UDP端口12345，接收游戏二进制或JSON；JSON回传seq，游戏UDP不应答。

```json
{"cmd":"status","seq":1}
{"cmd":"stop","seq":2}
```

以下仅USB可执行：

```json
{"cmd":"wifi_config","seq":3,"ssid":"YOUR_SSID","password":"YOUR_PASSWORD"}
{"cmd":"wifi_scan","seq":4}
{"cmd":"wifi_scan_status","seq":5}
{"cmd":"wifi_disconnect","seq":6}
{"cmd":"wifi_clear","seq":7}
{"cmd":"mapping_config","seq":8,"input_mode":"usb","speed_divisor":10}
{"cmd":"game","seq":9,"speed_kmh":200,"rpm":4000,"game_gear":4,"racing":true,"brief":true}
```

input_mode为usb或wifi；game命令须在usb输入模式使用，发送未缩放的游戏数值，避免重复映射。wifi输入模式接收游戏UDP，usb模式忽略游戏UDP，避免两条来源冲突。

SSID为1..32 UTF-8字节；密码为空或8..63字节。disconnect保留配置，clear删除配置。状态不返回密码。scan为异步操作，使用scan_status读取完成结果。

早期手动`set`/`mode`接口继续保留用于调试；`set`接收已映射的speed_kmh、power_pct、E/C/F/A。旧power_scale配置不参与0.2游戏映射。

## CAN协议

标准数据帧，DLC8，每种100ms。字节从B0编号：

| ID | 示例：10 km/h、20%、E | 字段 |
| --- | --- | --- |
| 0x170 | `00 00 00 00 AA 00 00 89` | B6：0 E / 1 C / 2 F / 3 A |
| 0x210 | `0E 0B 00 68 A0 00 50 00` | B4/B5小端raw/16 km/h；B6/B7小端raw/4 W% |
| 0x212 | `00 20 00 08 41 00 01 00` | 配套状态，字段未拆解 |

三帧已验证独立启动，尚未证明绝对最少。仪表自己发布的0x203用于速度回传，不作为输入注入。

## 验证与发布

详见[验证记录](docs/VALIDATION.md)。固件源码MIT授权，桌面项目单独管理。代码不含雅迪协议或旧项目的个人Wi-Fi信息，现场记录与构建产物不纳入源码历史。发布二进制使用Release附件。
