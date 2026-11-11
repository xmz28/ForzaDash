# 固件协议与开发说明

返回 [项目首页](../README.md)。固件版本 0.2.0。

## 游戏格式

只接收324字节Horizon布局，字段采用小端：速度offset256，单位m/s；RPM offset16；游戏挡位offset319；比赛标志offset0。参照[FH6官方Data Out文档](https://support.forza.net/hc/en-us/articles/51744149102611-Forza-Horizon-6-Data-Out-Documentation)，也与旧FH_5中继的已用偏移一致。

模拟器已经打通两条实机链路；FH4、FH5、FH6真实游戏仍需分别验证。二进制 UDP 拒绝错误长度、非有限数值、负速度 / RPM 和非法比赛标志；正数速度 / RPM 经映射限幅，游戏挡位未做上限校验。USB game 命令额外要求速度 0..500 km/h、RPM 0..50000、挡位 0..20。

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

详见[验证记录](VALIDATION.md)。固件源码MIT授权，桌面项目单独管理。代码不含雅迪协议或旧项目的个人Wi-Fi信息，现场记录与构建产物不纳入源码历史。发布二进制使用Release附件。
