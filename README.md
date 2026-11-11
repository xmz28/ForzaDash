# forza-dash

**Forza telemetry → ESP32 → Ninebot CAN dashboard.**

面向九号5寸TFT仪表的ESP32中继固件，配套Windows调试台 **ForzaDash**。先用USB测试，再通过串口配网，最后使用游戏UDP或模拟器驱动仪表。

当前实测对象是 **KZ110仪表**，不是所有九号CAN仪表都保证兼容。本仓库只包含九号协议，没有雅迪协议，也没有继承旧仓库的个人Wi-Fi配置与Git历史。

## 当前状态

- 已实机：USB手动输入、Wi-Fi串口配置、UDP手动输入、324字节游戏模拟器输入、三帧持续发送。
- 已确认：仪表独立重启后，无须原车控制器或21帧预热，速度/功率百分比/驾驶模式可控制。
- 游戏接收器使用Horizon 324字节布局；FH6格式参照[官方Data Out文档](https://support.forza.net/hc/en-us/articles/51744149102611-Forza-Horizon-6-Data-Out-Documentation)，速度和功率偏移也与旧FH_5中继一致。
- **FH4/FH5/FH6实际游戏各自的完整验证尚未完成**。模拟包通过不等于三款游戏全部实测通过；不接受未确认的311字节等布局。

## 硬件与编译

ESP32开发板 + 外部CAN收发器。当前板端：GPIO21=CAN TX、GPIO22=CAN RX，500 kbit/s。连接仅用于独立供电仪表台架，注意收发器、电源与终端电阻按实际硬件接线。

```powershell
pio run
pio run -t upload --upload-port COM7
```

上电后默认等待串口，没有预设Wi-Fi名称/密码，也不启用写死密码的热点。网络配置保存在设备NVS；重启非阻塞连接，Wi-Fi不可用时USB仍可工作。

## 配套电脑软件

桌面项目名 **ForzaDash**，位于独立桌面项目目录，发布时提供便携版`ForzaDash.exe`，包含Python/Tk运行时，无需用户安装Python。

1. USB连接开发板，选择COM端口，保持USB通道，点击“启动”。
2. 拖动速度/功率或选择E/C/F/A，即时提交；“停止”归零并保留串口连接。
3. 在Wi-Fi页填写SSID和密码，通过串口保存；查看设备的连接状态/IP。
4. 电脑与ESP32处于可互访网络，选择UDP，再启动手动测试或游戏UDP模拟。

只有测试区“启动、停止”两个主按钮。串口是独立配置通道，即使数据改走UDP，仍可读取诊断、修改网络与标定。配套软件核对串口与UDP的设备标识，防止目标IP指错设备。

## 数据与控制接口

UDP端口12345：支持324字节Horizon二进制包，以及JSON对象。USB115200，每行一个JSON对象，LF结尾，最长512字节。JSON应答回传seq；游戏UDP不应答。

```json
{"cmd":"status","seq":1}
{"cmd":"set","seq":2,"speed_kmh":20,"power_pct":60,"gear":"C"}
{"cmd":"mode","seq":3,"gear":"F"}
{"cmd":"stop","seq":4}
```

仅USB接受以下配置（示例占位值，不是项目内置网络）：

```json
{"cmd":"wifi_config","seq":5,"ssid":"YOUR_SSID","password":"YOUR_PASSWORD"}
{"cmd":"wifi_disconnect","seq":6}
{"cmd":"wifi_clear","seq":7}
{"cmd":"power_scale","seq":8,"watts":100000}
```

SSID为1..32 UTF-8字节，密码为空（开放网络）或8..63字节；密码不回读、不写诊断日志。disconnect暂时断开但保留配置，下次重启可连接；clear删除SSID与密码。功率满量程支持1000..2000000瓦，保存到NVS，默认100000瓦。

JSON速度0..200 km/h，功率0..100%；游戏速度也限幅200。游戏原始Power单位瓦，`power_pct=clamp(watts / full_scale_watts * 100, 0, 100)`，负功率显示0%。不沿用旧雅迪的功率缩放和平滑滤波。

游戏变速箱挡位只是遥测，不自动映射仪表E/C/F/A驾驶模式。可用mode独立选择模式。manual与游戏输入按最后收到的有效值更新，不要同时启动多个数据发送器；status不改变目标或刷新有效期。

输入断流3秒自动速度/功率归零并回E，仍保持三帧在线；比赛标志为0时立即归零。固件不改仪表固件或显示限制。

## 九号已验证三帧

标准数据帧、DLC8、每种100ms。字节从B0编号：

| ID | 示例载荷（10 km/h、20%、E） | 含义 |
| --- | --- | --- |
| 0x170 | `00 00 00 00 AA 00 00 89` | B6：0=E、1=C、2=F、3=A |
| 0x210 | `0E 0B 00 68 A0 00 50 00` | B4/B5小端速度raw/16；B6/B7小端功率raw/4 W% |
| 0x212 | `00 20 00 08 41 00 01 00` | 必需候选伴随状态，字段含义未拆解 |

三帧是已验证可用组合，尚未证明绝对最少。F是实测标签，官方模式名称未核实；P/H尚未映射。0x203是仪表自己外发的速度遥测，不作为输入注入。

当前KZ110主题屏幕10/20/30/40输入显示10/20/25/25，内部遥测仍能超过25；小数速度也可能取整。功率是百分比，**不是瓦数**。

## 结构与开源

- `src/main.cpp`：输入调度、串口配置、Wi-Fi/NVS、CAN周期与诊断。
- `include/forza_packet.h`：有长度/数值校验的二进制解析，独立于仪表编码。
- `include/dashboard_protocol.h`：九号三帧编码。
- `docs/VALIDATION.md`：实测范围与限制。

固件源码采用MIT许可证。桌面端发布包和桌面源码授权单独整理；本地仓库尚未创建或推送GitHub远端。发布二进制应放Release附件，构建输出不提交进源码历史。

Forza、Ninebot等名称仅用于说明兼容对象，本项目非官方产品。
