# vhud() 复核（LVGL 9.5 最佳实践）+ ESP32-S3 移植备忘

> 记录日期：2026-09-16
> 代码基线：`develop` 分支，LVGL **9.5.0**（submodule `85aa60d`），FreeType `459af335`
> 复核对象：`LvglWindowsSimulator/LvglWindowsSimulator.cpp` 的 `vhud()`（定义在 142 行；`main()` 第 112 行调用它）及其 helper
> 目的：作为迁移到 ESP32-S3 前的设计备忘录，不是上游文档

---

## 0. 速览

| 问题 | 结论 |
| --- | --- |
| `vhud()` 是四版里最完整的吗 | 是（部件覆盖最全），但它是**产品原型**，不是 LVGL 官方 demo |
| 符合 9.5 最佳实践吗 | **一半**：部件选型对（`lv_scale`/grid/分层/样式复用），实现有 **7 处 MCU 不友好**（见 §2.2） |
| 移植 S3 前必须先做什么 | 修 3 个已知缺陷 + 瘦身 + 换 9.5 写法（见 §3.6） |
| 最大的性能风险 | 旋转：默认走 **layer 路径**，每帧要为 `card` 分配 ≈79 KB 离屏缓冲（见 §3.1） |

### 0.1 前置事实：哪些代码是活的

调用图（从 `main()` 起算）显示，**当前生效的只有 7 个函数**：

```
main → vhud → guide_lines(278)
           → lv_roll_scale(258)
           → rollSlider_event_cb(406)  → update_ai(389)
           → pitchSlider_event_cb(414) → update_ai(389)
```

不可达（但都在 exe 里）：`init_anim(462)` / `init_anim_obj(430)` / `anim_canvas_cb(423)` / `anim_completed_cb(443)`（旋转动画链，因为 237 行 `// init_anim(card);` 被注释）、`vhud2(470)`、`vhud3(512)`、以及 **`ui.cpp` 整个文件**（`ui()` 无调用点）。
`ui2.cpp` 完全未参与编译，且用 LVGL 9.5 已经编不过（`ui2.cpp(37) error C2664`）。

### 0.2 已纠正的认知错误

早期判断"v9 里父对象的 transform 不作用于子对象"是**错的**。源码依据：

- `lv_obj_redraw()` 会递归重绘子对象（`src/core/lv_refr.c` 的 `refr_children` 分支）
- `lv_obj_get_layer_type()` 在 `transform_rotation != 0` 时返回 `LV_LAYER_TYPE_TRANSFORM`（`src/core/lv_obj_style.c:1089`）

结论：`card` 旋转时 `sky` / `gnd` / 梯线**会一起转**。`vhud()` 的"旋转层放地平线+梯线、固定层放红心+刻度"分层结构因此是**正确且值得保留**的（真实 EFIS 也是这个分层）。

---

## 1. Q2：`vhud()` 符合 LVGL 9.5 最佳实践吗

### 1.1 符合的部分（保留）

| 做法 | 为什么是对的 |
| --- | --- |
| 用 `lv_scale` 画刻度弧（`LV_SCALE_MODE_ROUND_OUTER`，19 ticks，major 3，range ±90，angle_range 90，rotation 225） | 一个对象顶几十个刻度/标签，MCU 正确姿势 |
| 用 grid 的 `row_dsc/column_dsc` 摆 sky/gnd | 比像素级 `lv_obj_set_pos` 健壮，改尺寸容易 |
| 样式在函数内用 `static lv_style_t` 建一次 | 不是每帧创建，无堆抖动 |
| 分层：固定层（`ai_hole_panel` 上的 scale + 2 条红心线）与旋转层（`card` 上的 sky/gnd/梯线）分离 | 与真实 EFIS 一致，且不用为固定元素付变换代价 |
| 圆窗用 `lv_obj_set_style_clip_corner(true)` + `LV_RADIUS_CIRCLE` | 用样式裁剪代替手工 mask，成本低 |

### 1.2 不符合 / 9.5 有更好写法（7 条）

#### ① 没有剥离默认主题

- 现象：`lv_obj_create()` 出来的容器/卡片都带着默认主题的 `card` 样式（`radius`、`bg_color`、`border_width`、`pad_all = PAD_DEF`、`pad_row/column`）。见 `src/themes/default/lv_theme_default.c` 的 `theme->styles.card`。
- 代价：每个 style 属性都占堆；每帧样式匹配也要遍历这些属性。
- 9.5 写法：`lv_obj_remove_style_all(obj);` 之后再 `lv_obj_add_style()` / `lv_obj_set_style_*()` 只加需要的。

#### ② transform 走 layer 路径，每帧一次大分配

- 现象：`update_ai()` 每次滑块变化都 `lv_obj_set_style_transform_rotation(card, angle, 0)`。
- 机制：`LV_DRAW_TRANSFORM_USE_MATRIX` 默认 **0**（本仓库 `LvglWindowsSimulator/lv_conf.h:138` 也是 0）→ 走 layer 路径：把 `card` 整棵子树渲进离屏层再旋转贴回。
- 代价：层默认 ARGB8888，`card` 100×198 → **≈79 KB/帧**（100×198×4B）的分配 + 两遍绘制。
- 9.5 写法：`LV_DRAW_TRANSFORM_USE_MATRIX 1` → 就地按逆矩阵重绘，**不额外分配**（代价转为每像素逆变换计算）。选哪条要看 S3 实测（§3.1）。

#### ③ 14 个 `lv_line` 对象画梯线

- 现象：`guide_lines()` 建了 12 条梯线（`line0..line6` + 复用变量再建 `line1..line6`）+ 2 条固定红心线，共 14 个对象，每对象带独立样式副本。
- 代价：对象头 + 样式 + 每对象一个 draw task。
- 9.5 写法：在 `card` 上挂 `LV_EVENT_DRAW_MAIN` 自定义绘制回调，用 `lv_draw_line()`/`lv_draw_rect()` 一次画完 → **0 额外对象、1 个 draw task**，且天然随 `card` 变换。

#### ④ 浮点与浮点格式化

- 现象：`update_ai()` 里 `sin()/cos()` 每次重算，并有 4 次 `lv_label_set_text_fmt` 使用 `"%.2f"`。
- 代价：S3 上软件浮点格式化会拉进 newlib 的 float printf（flash + 时间），`sin/cos` 也不便宜。
- 9.5/单片机写法：整数运算 + `"%d.%02d"` 之类格式；角度三角函数用查表。

#### ⑤ `lv_obj_set_pos` 与 `lv_obj_center` 互相覆盖

- 现象：`vhud()` 里 `lv_obj_center(card)`，之后 `update_ai()` 里 `lv_obj_set_pos(card, x_offset, y_offset)`。
- 依据：LVGL 的 `lv_obj_align()` 结尾会把对齐**烘焙**成 x/y 并复位对齐 ——
  `lv_obj_set_style_align(obj, LV_ALIGN_TOP_LEFT, 0); lv_obj_set_pos(obj, x, y);`（`src/core/lv_obj_pos.c`）。
- 后果：第一次拖滑块时卡片从"面板内居中"跳到左上角 = `(x_offset, y_offset)` 的位置，而不是"居中 ± 平移"。[行为结论为源码推断，待实测确认]
- 写法：`lv_obj_set_style_translate_x(card, x, 0)` / `lv_obj_set_style_translate_y(card, y, 0)`（还顺带避免父容器重排）。

#### ⑥ 数值→UI 用手写回调，而不是 subject/observer

- 现象：滑块回调 → `update_ai()` → 手动 `lv_label_set_text_fmt` + `lv_obj_set_style_transform_rotation`。
- 9.5 能力（已核实 API 存在，且本仓库 `lv_conf.h:1086` 已 `LV_USE_OBSERVER 1`）：
  - `lv_subject_init_int()` / `lv_subject_set_int()`（`src/core/lv_observer.h:112` / `:119`）
  - `lv_label_bind_text()`（`src/widgets/label/lv_label.h:255`）
  - `lv_obj_bind_style_prop()`（`src/core/lv_obj_style.h:430`）—— 可直接绑 `LV_STYLE_TRANSFORM_ROTATION`
- 价值：数据源换成 IMU 后 UI 自动跟随；少写事件胶水；移植时改动面更小。

#### ⑦ 尺寸写死 + pos/align 混用

- `main_cont_col` / `status_cont_col` 都是 500×500，是 PC 模拟器脚手架；S3 面板是 320×240/480×480 级别 → 用 `lv_pct(100)` 或相对尺寸。
- `pPitchSlider` 同时 `lv_obj_set_pos` 和 `lv_obj_align`（后者覆盖前者）。

---

## 2. Q3：移植 ESP32-S3（流畅度优先）该怎么做

### 2.1 旋转成本的三个档位（按开销从高到低）

| 档 | 做法 | 开销 | 适用 |
| --- | --- | --- | --- |
| A | 保留 widget 变换，但开启 `LV_DRAW_TRANSFORM_USE_MATRIX 1` | 无每帧大分配；每像素逆变换计算 | 想保留现有代码结构，先试这档 |
| B | 地平线做成**一张预渲染 image**，用 `lv_image_set_rotation()` / `lv_image_set_pivot()`（`src/widgets/image/lv_image.h:127` / `:137`） | 只变换一张图，源图放 PSRAM | 视觉更复杂（带纹理/刻度）时 |
| C | **不旋转**：地平线只做 `translate_y`（pitch）+ 只转外圈刻度盘 | 最低 | 真机 EFIS 的常见低成本画法 |

无论哪档：**旋转层只放最小对象**（`card` + `sky` + `gnd`），刻度/红心/文本一律放固定层。
注意 `LV_DRAW_SW_COMPLEX` 必须为 1 才能画旋转/掩码（9.5 默认 1，见 `lv_conf.h:195`，别关）。

### 2.2 `lv_conf.h` 对照（PC 现状 → S3 建议）

| 配置项 | PC 现状 | S3 建议 | 原因 |
| --- | --- | --- | --- |
| `LV_COLOR_DEPTH` | 32 | **16**（RGB565） | 带宽/内存 |
| `LV_MEM_SIZE` | 256 KB 静态数组 | **PSRAM 分配器**（`LV_USE_STDLIB_MALLOC = LV_STDLIB_CUSTOM` + `heap_caps_malloc`） | S3 内部 SRAM 装不下 256 KB（还要留给 WiFi/BT） |
| 显示模式 | DIRECT（lv_windows 驱动） | **PARTIAL + 2 个 1/10 屏缓冲放内部 RAM**，DMA 完成回调里 `lv_display_flush_ready()` | 与 SPI 传输重叠，避免全屏缓冲 |
| `LV_DEF_REFR_PERIOD` | 10 ms | **30 ms**（≈33 fps） | HUD 不需要 60 fps |
| `LV_DRAW_TRANSFORM_USE_MATRIX` | 0 | **1**（先试） | 去掉每帧 layer 分配（§2.1 档 A） |
| `LV_DRAW_SW_COMPLEX` | 1 | **1**（保持） | 旋转/掩码依赖它 |
| `LV_USE_DRAW_SW_ASM` | `LV_DRAW_SW_ASM_NONE` | 只能选 `NONE/NEON/HELIUM/RISCV_V/CUSTOM`（`src/lv_conf_internal.h:28-32`） | **上游 LVGL 不带 Xtensa/PIE 加速**；要用 S3 SIMD 得走 `LV_DRAW_SW_ASM_CUSTOM` 或 Espressif 的 LVGL 分支 |
| `LV_DRAW_SW_DRAW_UNIT_CNT` | 1 | 配 `LV_USE_OS = LV_OS_FREERTOS` 后可 >1 | 用 S3 双核并行绘制（注意内存开销） |
| `LV_USE_OBSERVER` | 1 | 保持 1 | 用 subject/observer 取代手写回调（§1.2 ⑥） |
| `LV_USE_FREETYPE` | 0 | 保持 0 | 字体用位图/预转换，FreeType 在 S3 上内存与速度都吃紧 |

### 2.3 触摸/输入/系统

- 时间源：`lv_tick_set_cb()` 接 `esp_timer_get_time()`；`LV_USE_OS = LV_OS_FREERTOS`；在单独 FreeRTOS 任务里跑 `lv_timer_handler()` 并按返回值 `vTaskDelay()`。
- 输入：`lv_indev` 用 esp_lvgl_port 的触摸/按键驱动；注意与显示 flush 的线程安全（esp_lvgl_port 已处理锁）。
- 内存碎片：`lv_mem_monitor()` 观察 `frag`；避免每帧分配（§1.2 ② 就是要解决的）。

### 2.4 字体

- 数字/英文 UI 用 `LV_FONT_MONTSERRAT_*` 位图字体即可。
- 需要中文/多字形时用 `LV_FONT_SIMSUN_16_CJK` 或自行裁剪子集，**不要**为省事开 FreeType。

### 2.5 实测方法（不要把 PC 的数字外推）

PC 窗口上显示的 `96 FPS / 1% CPU` 是 32bpp + x86 + 大带宽的成绩，**不能外推**到 S3。上板后：

1. 打开 `LV_USE_PERF_MONITOR`（以及 `LV_USE_MEM_MONITOR`）看 fps、CPU、`frag`。
2. 用 `esp_timer_get_time()` 包住 `lv_timer_handler()`，统计 p95 耗时；重点看"拖滑块/动画进行中"时的尖峰。
3. 用 `lv_mem_monitor()` 确认旋转期间**没有每帧分配**。
4. 逐步降级验证：档 A → 档 B → 档 C，记录各自帧率/CPU，选满足 30 fps 且余量最大的。

### 2.6 移植前必须完成的修复清单

**A. 缺陷修复（3 个已知问题）**

- [ ] 动画链：`init_anim_obj()` 只配置不启动 + `anim_completed_cb()` 里两处 `lv_anim_start` 被注释 → 动画跑两趟后永久停住。要么补 `lv_anim_start` 让摆动闭环，要么直接删掉整条链。
- [ ] `update_ai()` 的 `lv_obj_set_pos` → 改 `lv_obj_set_style_translate_x/y`（§1.2 ⑤）。
- [ ] `%.2f` → 整数格式化（§1.2 ④）。

**B. 瘦身 / 9.5 化**

- [ ] 所有容器加 `lv_obj_remove_style_all()`，只加必要样式（§1.2 ①）。
- [ ] 梯线从 14 个 `lv_line` 改为 `LV_EVENT_DRAW_MAIN` 自定义绘制（§1.2 ③）。
- [ ] 角度/俯仰数值改 `lv_subject_int` + `lv_label_bind_text` + `lv_obj_bind_style_prop(TRANSFORM_ROTATION)`（§1.2 ⑥）。
- [ ] `500×500` 写死尺寸改相对尺寸；`pPitchSlider` 去掉 pos/align 混用（§1.2 ⑦）。
- [ ] 旋转层最小化：`card` 只保留 `sky`/`gnd`（+ 必要的梯线），其余移出（§2.1）。

**C. 清理**

- [ ] 删除死代码：`ui2.cpp`（未编译且编不过）、`LvglWindowsSimulator.cpp` 的 `vhud2()`/`vhud3()`、不可达的动画链（若选删除路线）、`ui.cpp`（从 vcxproj 移除）、文件级未使用的 `static lv_style_t style_sky/style_ground`（`LvglWindowsSimulator.cpp:126-127`）。
- [ ] `lv_conf.h` 中 `LV_USE_DEMO_RENDER` / `LV_USE_DEMO_TRANSFORM` / `LV_FONT_MONTSERRAT_18` 的本地覆盖：对应的调用点都在注释里，若不再需要可还原为上游默认，减少每次同步上游时的冲突面。

---

## 3. 附录：证据索引（源码 file:line）

| 结论 | 出处 |
| --- | --- |
| 子对象随父对象一起被变换 | `lvgl/src/core/lv_refr.c`（`lv_obj_redraw` 的 `refr_children`）；`lvgl/src/core/lv_obj_style.c:1089` |
| transform 默认走 layer 路径 | `lvgl/lv_conf_template.h:138`（`LV_DRAW_TRANSFORM_USE_MATRIX 0`）；`lvgl/src/core/lv_refr.c:587`（`layer_draw_dsc.rotation = ...`） |
| 矩阵路径实现（不额外分配） | `lvgl/src/core/lv_refr.c`（`obj_get_matrix` / `refr_obj_matrix`） |
| `lv_obj_align` 会把对齐烘焙成 x/y | `lvgl/src/core/lv_obj_pos.c`（`lv_obj_align` 末尾 `lv_obj_set_style_align(obj, LV_ALIGN_TOP_LEFT, 0); lv_obj_set_pos(obj, x, y);`） |
| 默认主题给 `lv_obj` 加了 card 样式 | `lvgl/src/themes/default/lv_theme_default.c`（`theme->styles.card`：radius / bg_color / border_width / pad_all） |
| SW 加速选项无 Xtensa/PIE | `lvgl/src/lv_conf_internal.h:28-32` |
| observer/subject 绑定 API | `lvgl/src/core/lv_observer.h:112,119`；`lvgl/src/widgets/label/lv_label.h:255`；`lvgl/src/core/lv_obj_style.h:430` |
| 图片旋转 API | `lvgl/src/widgets/image/lv_image.h:127,137` |
| `vhud()` 及其 helper 行号 | `LvglWindowsSimulator/LvglWindowsSimulator.cpp`：142（vhud）/258（lv_roll_scale）/278（guide_lines）/389（update_ai）/406/414（slider 回调）/423（anim_canvas_cb）/430（init_anim_obj）/443（anim_completed_cb）/462（init_anim）/470（vhud2）/512（vhud3） |
| `ui2.cpp` 的同一份逻辑（未编译、9.5 下编不过） | `LvglWindowsSimulator/ui2.cpp`：14（vhud）/37（C2664 处）/84/89 |
| 本仓库配置相关行 | `LvglWindowsSimulator/lv_conf.h`：132（`LV_DRAW_BUF_ALIGN`）/138（`LV_DRAW_TRANSFORM_USE_MATRIX`）/184（`LV_DRAW_SW_DRAW_UNIT_CNT`）/195（`LV_DRAW_SW_COMPLEX`）/210（`LV_USE_DRAW_SW_ASM`）/555（`LV_USE_FLOAT`）/1086（`LV_USE_OBSERVER`） |
