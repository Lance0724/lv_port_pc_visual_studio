# ESP32-S3 1.47 英寸目标板 HUD 移植备忘录

本文供目标板代码工程中的 AI agent 使用。目标是把本仓库的 `hud_minimal` 原样移植到 **VS Code + PlatformIO + Arduino 框架**工程，并在 320×172 横屏上保持 PC 模拟器已经确认的行为。

## 1. 开始前先确认硬件

本文按以下目标作为基线：

- MCU：ESP32-S3；
- LCD：1.47 英寸、原生 172×320、ST7789、SPI；
- LVGL 逻辑方向：旋转为横屏 320×172；
- 推荐硬件基线：Waveshare `ESP32-S3-LCD-1.47`，16 MB Flash、8 MB PSRAM。

若实际板卡不是该 Waveshare 型号，**不得套用下面的 GPIO**；先读目标板原理图和厂商示例，只保留 320×172、ST7789 和 HUD 部分。

Waveshare 板载 LCD 引脚：

| 信号 | GPIO |
| --- | ---: |
| MOSI | 45 |
| SCLK | 40 |
| CS | 42 |
| DC | 41 |
| RST | 39 |
| BL | 48 |

该板没有触摸输入，HUD 移植不需要创建 `lv_indev`。

## 2. 移植边界

从本仓库 `develop` 分支的同一个提交复制：

- `LvglWindowsSimulator/hud.h`
- `LvglWindowsSimulator/hud_minimal.c`

不要复制：

- `hud_simulator.h/.cpp`：仅用于 Windows 手动控制面板；
- `LvglWindowsSimulator.cpp`：Win32 主程序；
- `ui.cpp/.h`：旧 HUD 原型；
- Windows 项目文件和 Windows `lv_conf.h`。

保持 `hud_minimal.c` 为 C 文件。Arduino 主程序可以是 C++；`hud.h` 已提供 `extern "C"`。若 PlatformIO 的 LVGL 头文件布局不接受 `#include "lvgl/lvgl.h"`，目标工程只做这一项适配，改为 `#include <lvgl.h>`，不要顺手改写 HUD 结构。

当前实现是单实例：文件内 `g` 保存所有 LVGL 对象句柄。目标产品只有一个 HUD 时不要为此重构。

## 3. 不得改变的显示契约

- HUD 固定尺寸为 320×172，1:1 覆盖横屏。
- Roll/Pitch 单位均为 0.1°。
- `roll_ddeg > 0` 表示右翼下沉。
- `pitch_ddeg > 0` 表示机头上仰。
- 固定飞机中心必须指向当前 Pitch 对应的梯线。
- Pitch 梯线覆盖 −90°..+90°，每 5° 一格、每 10° 标数值。
- Roll 与 Pitch 同时存在时，梯线滚动方向始终垂直于梯线；天地线与梯线姿态一致。
- 顶部绿色 Roll 指针固定；弧、刻线和红色限制标记随 Roll 旋转。
- 全部数值保持整数路径，不引入浮点格式化。
- `lv_draw_*` 使用绝对屏幕坐标；不要删除 `hud_origin()` 的根对象偏移。
- draw callback 的标签文字必须继续使用静态字符串和 `text_static = 1`。

## 4. 推荐移植顺序

必须按下面顺序推进；上一层未通过前，不接入下一层。

### 4.1 先点亮 LCD，不接 LVGL

1. 使用厂商示例确认 ST7789 初始化、背光和 SPI 引脚。
2. 设置横屏方向，确认逻辑分辨率确实为 320×172。
3. 画四角不同颜色和 RGB 色块，确认：
   - 没有 172/240 像素窗口偏移错误；
   - 红蓝通道未颠倒；
   - 横纵方向和坐标原点正确。

LVGL 官方 Arduino 指南推荐 LovyanGFX；也可以复用目标工程已经验证的 ST7789 驱动。不要同时保留两套显示驱动。

### 4.2 再接 LVGL 9.5

- 目标版本必须与本仓库一致：LVGL 9.5.x；不要使用 Waveshare 示例附带的 LVGL 8.3.10 API。
- 先只显示一个全屏标签或色块，验证 flush callback。
- 使用 `LV_DISPLAY_RENDER_MODE_PARTIAL`。
- 建议双缓冲，每个缓冲 320×20×2 = 12,800 字节，两个共 25,600 字节。
- flush/DMA 缓冲放内部、DMA 可访问 RAM；不要默认放 PSRAM。
- 若同步 SPI 写屏，传输完成后立即调用 `lv_display_flush_ready()`；若异步 DMA，必须在 DMA 真正完成后调用。
- RGB565 字节序不对时，优先把显示格式设为 `LV_COLOR_FORMAT_RGB565_SWAPPED`，不要每帧手写逐像素交换。

### 4.3 最后接 HUD

初始化顺序：

```cpp
lv_init();
init_lcd_and_lvgl_display();
lv_tick_set_cb(board_millis_cb);
hud_minimal_create(lv_screen_active(), nullptr);
hud_minimal_update(&initial_data);
```

主循环最简形式：

```cpp
void loop()
{
    lv_timer_handler();
    delay(5);
}
```

所有 LVGL API，包括 `hud_minimal_update()`，必须由同一个 UI 执行上下文调用。遥测接收任务只投递数据快照；不要从 UART、Wi-Fi、BLE 回调或 ISR 直接调用 LVGL。

## 5. PlatformIO 基线

`platformio.ini` 至少明确：

```ini
[env:esp32-s3-hud]
platform = platformio/espressif32
framework = arduino
board = esp32-s3-devkitc-1
monitor_speed = 115200
lib_deps =
    lvgl/lvgl @ 9.5.0
build_flags =
    -DBOARD_HAS_PSRAM
    -DLV_CONF_INCLUDE_SIMPLE
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

这是工程骨架，不是完整板卡定义。目标工程 agent 还必须：

- 根据实际模组设置 16 MB Flash 和 8 MB PSRAM 类型；
- 必要时增加项目私有的 PlatformIO board JSON；
- 在启动日志中验证 `psramFound()` 为真，并打印实际 Flash/PSRAM 容量；
- 原型跑通后固定 PlatformIO platform、Arduino core 和 LCD 驱动版本，避免浮动依赖。

## 6. `lv_conf.h` 最小要求

| 配置 | 建议值 |
| --- | --- |
| `LV_COLOR_DEPTH` | `16` |
| `LV_DEF_REFR_PERIOD` | `33`，约 30 fps |
| `LV_USE_OS` | 首次移植用 `LV_OS_NONE`，所有 LVGL 调用单线程 |
| `LV_DRAW_TRANSFORM_USE_MATRIX` | `1`，先验证现有旋转效果 |
| `LV_DRAW_SW_COMPLEX` | `1` |
| `LV_GRADIENT_MAX_STOPS` | 至少 `2` |
| `LV_USE_FREETYPE` | `0` |
| `LV_FONT_MONTSERRAT_12` | `1` |
| `LV_FONT_MONTSERRAT_14` | `1` |
| `LV_FONT_MONTSERRAT_20` | `1` |
| `LV_FONT_MONTSERRAT_40` | `1` |
| `LV_FONT_MONTSERRAT_48` | `1`；当前默认主题仍引用它 |
| `LV_USE_LABEL` / `LV_USE_BAR` | `1` |
| demos/examples | 关闭 |

LVGL 对象堆优先放 PSRAM；SPI DMA draw buffer 留在内部 RAM。可使用 LVGL builtin allocator，并通过 `LV_MEM_POOL_ALLOC` 从 PSRAM 建立固定池。初始池大小可从 512 KiB 开始，但最终数值必须依据实测峰值调整，不能把该值当成结论。

## 7. 遥测数据接入

目标工程只构造 `hud_data_t`，然后调用：

```cpp
hud_minimal_update(&snapshot);
```

输入约定：

| 字段 | 单位/范围 |
| --- | --- |
| `heading_ddeg` | 0.1°；HUD 内部归一化到 0..359° |
| `roll_ddeg` | 0.1°，右翼下沉为正 |
| `pitch_ddeg` | 0.1°，建议输入 −900..+900 |
| `speed_kmh` | km/h |
| `alt_m` | m |
| `vs_cms` | cm/s，有符号 |
| `thr_pct` | 0..100 |
| `home_m` | m，版面按最多四位数设计 |
| `batt_mv` | mV |
| `batt_pct` | 0..100 |

在遥测适配层完成单位换算和范围裁剪，不要把 MAVLink/串口解析逻辑放进 `hud_minimal.c`。建议遥测更新 20–30 Hz；无新快照时不必重复写全部标签。

## 8. 资源和性能检查

当前最需要关注的是变换绘制，不是 C/C++ 包装：

- 天地 card：320×320；
- 梯线变换层：128×256；
- 若矩阵变换未生效而走 ARGB8888 layer 路径，名义缓冲量级分别约 400 KiB 和 128 KiB。

这些不是实际峰值结论。上板后必须记录：

- 空闲内部 RAM、最大连续内部 RAM；
- 空闲 PSRAM、最大连续 PSRAM；
- `lv_timer_handler()` 平均值和 p95 耗时；
- Roll/Pitch 连续变化时的最低 fps；
- 运行至少 10 分钟后的堆碎片和最低余量。

性能不足时按此顺序处理：

1. 确认 `LV_DRAW_TRANSFORM_USE_MATRIX = 1` 实际生效；
2. 检查是否发生每帧大块分配；
3. 降低 HUD/遥测更新频率到 20–30 Hz；
4. 再考虑预渲染地平线或降低视觉效果。

不要先删掉 Roll/Pitch 复合关系或把梯线固定在屏幕上；那会改变已经确认的功能。

## 9. 验收清单

### 显示驱动

- [ ] 四角和色块测试通过，无窗口偏移、镜像、红蓝颠倒。
- [ ] LVGL 逻辑分辨率为 320×172。
- [ ] partial flush 的每个区域坐标和像素数正确。
- [ ] 同步或 DMA flush 都只在传输完成后调用 `lv_display_flush_ready()`。

### HUD 行为

- [ ] Roll 0° / Pitch 0°：天地线水平，0° 位于飞机中心。
- [ ] Roll 0° / Pitch +20°：+20° 梯线经过飞机中心。
- [ ] Roll +45° / Pitch +20°：天地线和梯线一起旋转，+20° 仍经过中心。
- [ ] Roll +45° / Pitch −20°：滚动方向反向且仍垂直于梯线。
- [ ] Roll +45° / Pitch 接近 +90°：天空占满背景，+90° 梯线仍可见并经过中心。
- [ ] 速度、高度四位数、电池、模式、垂直速度和 Home 数值没有裁切。

### 稳定性

- [ ] PSRAM 容量识别正确。
- [ ] 连续运行 10 分钟无重启、看门狗、花屏或持续内存下降。
- [ ] 记录实际 fps、CPU/处理耗时和内存数据后再做优化。

## 10. 参考资料

- 本仓库实现说明：`Documents/Lvgl95Review-And-ESP32S3Porting.md`
- Waveshare 板卡资料：<https://www.waveshare.com/wiki/ESP32-S3-LCD-1.47>
- PlatformIO Espressif32：<https://docs.platformio.org/platforms/espressif32.html>
- PlatformIO Arduino：<https://docs.platformio.org/frameworks/arduino.html>
- LVGL Arduino 指南：本仓库子模块 `LvglPlatform/lvgl/docs/src/integration/frameworks/arduino.rst`
- LVGL display buffer/flush：本仓库子模块 `LvglPlatform/lvgl/docs/src/main-modules/display/setup.rst`
