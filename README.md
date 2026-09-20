# 06_pocketjs_rv1106

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

结构上参考 `03_music_player`：设备适配位于 `src/device`，运行时和应用逻辑分开，头文件统一放在 `include`，构建输出统一放在 `build`。

## 环境

需要：

- RV1106 SDK 交叉编译器
- Rust nightly、`rust-src`
- Bun
- CMake

默认 SDK 路径：

```text
/home/flower/Echo-Mate/SDK/rv1106-sdk
```

可以通过环境变量覆盖：

```bash
export ECHO_SDK_ROOT=/path/to/rv1106-sdk
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

默认开发板：

```text
root@192.168.9.121
```

默认远程目录：

```text
/root/Flower/04_hello_rust
```

执行：

```bash
./tools/deploy_guest_rv1106.sh
```

也可以覆盖目标：

```bash
RV1106_BOARD=root@192.168.9.121 \
RV1106_REMOTE_DIR=/root/Flower/04_hello_rust \
./tools/deploy_guest_rv1106.sh
```

native host 默认持续运行；传入帧数可以执行有限帧测试。当前已接入 `/dev/input/event0` 的 FocalTech 单指触摸，PocketJS guest 使用 `onPress` 接收点击。

直接运行：

```bash
./pocket_host display_demo.js display_demo.pak 0
```

最后的 `0` 表示无限循环；需要停止时按 `Ctrl-C`。触摸设备默认使用 `/dev/input/event0`，也可以覆盖：

```bash
RV1106_TOUCH_DEVICE=/dev/input/event0 \
./pocket_host display_demo.js display_demo.pak 0
```

当前 demo 中的 `TOUCH BUTTON` 使用 PocketJS 的 `focusable + onPress`，触摸按下、移动、抬起由 host 转成每帧 contact，PocketJS 负责命中测试、按压状态和回调。

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

## 当前边界

已完成第一个完整的 PocketJS 垂直切片和连续动画例程，但还不是生产级移植。后续任务记录在 [TODO.md](TODO.md)。
