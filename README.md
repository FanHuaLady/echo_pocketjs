# pocketjs-rv1106

PocketJS 在 RV1106 Linux/uClibc 开发板上的移植工程。

当前已经验证：

```text
Solid JSX
    -> PocketJS compiler
    -> guest.js + guest.pak
    -> QuickJS
    -> PocketJS Rust UI core
    -> BGRA software rasterizer
    -> RGB565 /dev/fb0
```

## 目录结构

```text
app/
  display_demo/
    main.tsx                 PocketJS Solid guest 应用
    pocket.config.ts         原生时间线动画配置
    images.json              图片资源采样配置

img/
  flower.jpg                 图片测试输入

src/
  main.c                     native host 入口
  device/
    framebuffer.c            RV1106 framebuffer 适配
    touchscreen.c            Linux evdev 单指触摸适配
  runtime/
    guest_file.c             guest.js/pak 文件读取

tests/
  fb_bench.c                 framebuffer 全屏纯色基准测试

include/
  device/
    framebuffer.h
    touchscreen.h
  runtime/
    guest_file.h

third_party/
  pocketjs/                  PocketJS 源码、Rust core、compiler
  quickjs/                   QuickJS 源码和 RV1106 静态库

cmake/
  toolchains/rv1106.cmake   RV1106 交叉编译工具链

tools/
  build_quickjs_rv1106.sh   编译 QuickJS
  build_core_rv1106.sh      编译 PocketJS Rust core
  prepare_assets_rv1106.ts  将开发图片转换为 PocketJS 纹理
  build_guest_rv1106.sh     编译 Solid JSX guest
  deploy_guest_rv1106.sh    上传并运行开发板程序

build/
  rv1106-release/           CMake native host 构建目录
  guest/                    guest.js 和 guest.pak 构建目录
```

工程结构按常见嵌入式应用方式组织：设备适配位于 `src/device`，运行时和应用逻辑分开，头文件统一放在 `include`，测试工具放在 `tests`，构建输出统一放在 `build`。

## 环境

需要：

- RV1106 SDK 交叉编译器
- Rust nightly、`rust-src`
- Bun
- CMake

设置 SDK 路径：

```bash
export ECHO_SDK_ROOT=/path/to/rv1106-sdk
```

如果不设置，CMake toolchain 和构建脚本会尝试使用：

```text
/opt/rv1106-sdk
```

## 获取源码

本项目使用 submodule 引入 PocketJS 和 QuickJS：

```bash
git clone --recursive https://github.com/your-name/pocketjs-rv1106.git
cd pocketjs-rv1106
```

如果 clone 时没有加 `--recursive`：

```bash
git submodule update --init --recursive
```

## 首次准备

安装 PocketJS compiler 依赖：

```bash
cd third_party/pocketjs
bun install
cd ../..
```

## 构建

### 1. 编译 QuickJS

```bash
./tools/build_quickjs_rv1106.sh
```

QuickJS 的 uClibc SDK 缺少 `fenv.h`，工程内的 `compat/fenv.h` 只作为兼容头文件，不修改第三方源码。

### 2. 编译 PocketJS Rust core

```bash
./tools/build_core_rv1106.sh
```

使用：

```text
armv7-unknown-linux-uclibceabihf
Rust nightly + build-std
Rockchip GCC 链接
```

### 3. 编译 native host

```bash
cmake --preset rv1106-release
cmake --build --preset rv1106-release
```

生成：

```text
build/rv1106-release/pocket_host
build/rv1106-release/fb_bench
```

### 4. 编译 PocketJS guest

```bash
./tools/build_guest_rv1106.sh
```

生成：

```text
build/guest/display_demo.js
build/guest/display_demo.pak
```

## 部署和运行

设置开发板地址和远程目录：

```bash
export RV1106_BOARD=root@你开发板的IP
export RV1106_REMOTE_DIR=/root/pocketjs-rv1106
```

执行：

```bash
./tools/deploy_guest_rv1106.sh
```

也可以覆盖目标：

```bash
RV1106_BOARD=root@你开发板的IP \
RV1106_REMOTE_DIR=/root/pocketjs-rv1106 \
./tools/deploy_guest_rv1106.sh
```

native host 默认持续运行；传入帧数可以执行有限帧测试。当前已接入 Linux evdev 触摸输入，PocketJS guest 使用 `onPress` 和 `touches()` 接收点击、开关和滑动条交互。

直接运行：

```bash
./pocket_host display_demo.js display_demo.pak 0
```

最后的 `0` 表示无限循环；需要停止时按 `Ctrl-C`。触摸设备默认自动扫描 `/dev/input/event*`，也可以覆盖：

```bash
RV1106_TOUCH_DEVICE=/dev/input/event0 \
./pocket_host display_demo.js display_demo.pak 0
```

触摸坐标校准和方向可以通过环境变量配置：

```bash
RV1106_TOUCH_MIN_X=0 \
RV1106_TOUCH_MAX_X=320 \
RV1106_TOUCH_MIN_Y=0 \
RV1106_TOUCH_MAX_Y=240 \
RV1106_TOUCH_FLIP_X=0 \
RV1106_TOUCH_FLIP_Y=0 \
RV1106_TOUCH_SWAP_XY=0 \
./pocket_host display_demo.js display_demo.pak 0
```

当前 demo 中的按钮和开关使用 PocketJS 的 `focusable + onPress`。横向滑动条每帧读取 `touches()` 的坐标，按手指位置更新数值。主内容区域高度大于屏幕可视区域，可以上下拖动滚动，右侧滚动条显示当前位置，用于验证超出屏幕后的控件交互。触摸按下、移动、抬起由 host 转成每帧 contact，PocketJS 负责命中测试、按压状态和回调。

host 已接入 `SIGINT` 和 `SIGTERM`，退出时会关闭 PocketJS runtime、触摸设备和 framebuffer。帧循环使用 `CLOCK_MONOTONIC` 做 60 Hz 调度，并统计实际 FPS 和 dropped frames。demo 左上角的 `FPS` 数字来自 host 每秒上报的真实帧统计。

图片测试使用 `img/flower.jpg`。PocketJS 的官方打包器接收 PNG/SVG，构建前会将这张 `130x130` JPEG 转成 `256x256` PNG，再按 `Image src="flower.png"` 打包为 `ui:img.flower.png`。

## 图片显示逻辑

图片不需要手工转换成 C 数组：

```text
flower.jpg
  -> 构建阶段解码为 RGBA
  -> 编码为 PocketJS IMG entry
  -> 写入 display_demo.pak
  -> QuickJS 解析 pak 并调用 ui.uploadImgEntry()
  -> Rust core 分配纹理句柄
  -> <Image src="flower.png" /> 绑定纹理句柄
  -> Rust 软件光栅器按节点的 width/height 采样绘制
  -> C host 把 BGRA framebuffer 转为 RGB565 写入 /dev/fb0
```

`src` 是资源名，不是文件路径。`Image` 首次创建时，框架根据资源名找到已注册的纹理句柄，然后调用 `setImage`；图片像素不会经过应用层的 C 数组。当前 `images.json` 设置了 `linear: true`，缩放时使用线性采样；不设置时默认使用最近邻采样。

当前例程使用一张图片显示为 `96x96`，验证的是资源加载、纹理绑定和运行时缩放。原始图片只打包一份，由一个 Image 节点引用。

## Framebuffer 基准测试

`fb_bench` 位于 `tests/fb_bench.c`，是独立的 framebuffer 测试程序，不经过 PocketJS、QuickJS 或 Rust UI core。它只做全屏纯色快速切换，用于测试 `/dev/fb0` 的基础吞吐、`msync` 成本和 fbdev 双缓冲/page flip 支持情况。

用法：

```bash
./fb_bench [auto|single|single-msync|pan|pan-msync] [seconds]
```

模式：

- `auto`：优先尝试双缓冲 pan，失败后退回单缓冲。
- `single`：单缓冲全屏填色，不主动 `msync`。
- `single-msync`：单缓冲全屏填色，每帧 `msync`。
- `pan`：双缓冲填色后 `FBIOPAN_DISPLAY`。
- `pan-msync`：双缓冲填色、`msync`、再 `FBIOPAN_DISPLAY`。

`seconds` 为 `0` 或省略时持续运行，按 `Ctrl-C` 或发送 `SIGTERM` 停止。

当前 RV1106 开发板实测：

```text
auto:
  framebuffer double buffer unavailable: yres_virtual=240 smem_len=153600 required=307200
  mode=single msync=no pan=no buffers=1
  fps ~= 472

single-msync:
  fps ~= 135-143

single:
  fps ~= 461-477
```

结论：

- 当前 fbdev 只暴露一屏显存，`yres_virtual` 不能扩展到两屏。
- 目前不能通过 `FBIOPAN_DISPLAY` 做 fbdev 双缓冲/page flip。
- 全屏纯色填充本身很快，PocketJS demo 的 `~27 FPS` 主要不是 framebuffer 纯写入上限导致。
- 后续更值得优先优化的是 PocketJS 软件渲染、BGRA 到 RGB565 转换、damage 局部刷新和每帧同步策略。

## 当前边界

已完成第一个完整的 PocketJS 垂直切片和连续动画例程，但还不是生产级移植。后续任务记录在 [TODO.md](TODO.md)。
