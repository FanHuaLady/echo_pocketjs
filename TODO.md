# 06_pocketjs_rv1106 待办

更新时间：2026-09-20

这个工程用于记录 PocketJS 在 RV1106 Linux/uClibc 开发板上的移植过程。

## 当前状态

当前已经完成第一个可运行的 PocketJS 垂直切片：

```text
Solid JSX guest
    -> PocketJS compiler
    -> display_demo.js + display_demo.pak
    -> QuickJS
    -> PocketJS Rust UI core
    -> BGRA software rasterizer
    -> RGB565 /dev/fb0
```

已验证内容：

- 使用 Rust nightly、`rust-src` 和 `build-std` 编译 `armv7-unknown-linux-uclibceabihf` Rust core。
- 使用 Rockchip RV1106 SDK 的 GCC 完成最终链接。
- QuickJS 已交叉编译为 RV1106/uClibc 静态库。
- 通过 `compat/fenv.h` 解决 SDK 缺少 `fenv.h` 的编译问题。
- native host 可以加载 guest JavaScript 和 `.pak` 资源包。
- `/dev/fb0` 已完成 framebuffer 映射、BGRA 到 RGB565 转换和显示。
- `/dev/input/event0` 已接入 Linux evdev 触摸输入。
- 已完成触摸坐标范围读取、坐标映射、按下/移动/抬起状态处理。
- PocketJS 的 `focusable + onPress` 已在开发板上验证。
- 图片可以通过 `.pak` 资源加载，不需要转换成 C 数组。
- 当前 demo 显示一张 `flower.jpg`，保留轨道动画和彩色动画条。
- host 支持有限帧测试和无限运行；传入 `0` 表示持续播放。
- 当前版本已经上传到开发板并持续运行验证。

开发板运行目录示例：

```text
/root/pocketjs-rv1106
```

启动命令：

```bash
./pocket_host display_demo.js display_demo.pak 0
```

## 还需要移植或完善的内容

并不是 PocketJS 的所有功能都必须一次性移植到 RV1106。当前基础显示链路已经完成，后续应该围绕稳定性、性能和实际产品功能逐步扩展。

### P0：基础运行稳定性

- [x] 增加 `SIGINT`、`SIGTERM` 处理，确保退出时正确关闭 framebuffer、触摸设备和 PocketJS runtime。
- [x] 将固定的 `nanosleep` 帧循环改为基于单调时钟的帧调度，记录实际 FPS 和丢帧情况；当前 demo 左上角显示 host 统计的 FPS。
- [ ] 增加统一的 host 日志等级和错误码，方便在开发板上定位启动、资源和渲染错误。
- [ ] 检查 guest、`.pak`、QuickJS 和 Rust core 的内存释放路径。
- [ ] 增加长时间运行测试，至少验证连续运行数小时后的内存、帧率和触摸状态。

### P0：Framebuffer 适配完善

- [ ] 验证不同 RV1106 固件下的分辨率、stride、x/y offset 和 RGB565 bitfield。
- [x] 确认是否可以使用 page flip、双缓冲或 `FBIOPAN_DISPLAY`，减少撕裂；当前 fbdev 只有一屏显存，`FBIOPAN_DISPLAY` 双缓冲不可用。
- [ ] 评估是否需要保留 `msync`；当前每帧全屏同步可能影响性能。
- [x] 增加屏幕旋转和横竖屏方向配置；host 支持 `RV1106_DISPLAY_ROTATION=0/90/180/270` 和 `RV1106_DISPLAY_ORIENTATION=landscape/portrait`，软件旋转 framebuffer，并同步变换触摸坐标。
- [ ] 对 framebuffer 映射范围、边界和异常设备状态增加更严格的检查。

### P0：触摸输入适配完善

- [x] 支持通过设备扫描或配置文件选择触摸设备，而不是长期依赖 `/dev/input/event0`。
- [x] 增加触摸坐标校准和 X/Y 方向翻转配置。
- [x] 根据实际触摸驱动验证多点触摸 slot、手指数和异常抬起事件。
- [ ] 增加触摸设备断开、事件溢出和状态恢复处理。
- [x] 明确单点触摸、多点触摸、拖动、点击和长按在 PocketJS runtime 中的统一语义；当前 host 选择第一个活动 contact 映射到 PocketJS 单点触摸，点击和拖动已由 demo 验证，长按暂不单独定义。

### P1：渲染性能

- [x] 增加独立的全屏纯色 framebuffer 基准程序，用于排除 PocketJS/QuickJS/Rust core 干扰并观察优化效果。
- [ ] 使用 PocketJS runtime 的 damage 信息，只更新发生变化的 framebuffer 区域。
- [ ] 对全屏 BGRA 到 RGB565 转换做基准测试，确认 CPU 占用和可接受帧率。
- [ ] 测量 QuickJS 启动时间、guest 执行时间、Rust 渲染时间和 framebuffer 提交时间。
- [ ] 测量不同图片尺寸、透明度、缩放比例和动画数量下的内存占用。
- [ ] 在开发板上增加可重复的性能测试命令和结果记录。

### P1：资源和 UI 能力

- [ ] 测试多张图片、透明 PNG、不同尺寸图片和多纹理同时显示。
- [ ] 测试图片缩放、线性采样、最近邻采样和裁剪边界。
- [ ] 测试字体、中文字体和更长文本的打包及显示。
- [x] 增加常用 UI 控件状态：普通、按下、聚焦和选中；当前 demo 包含按钮、开关、横向触摸滑动条和纵向拖动滚动区域。
- [ ] 增加滚动容器、列表和页面切换等真实应用需要的交互。
- [ ] 验证更多原生时间线动画在 RV1106 上的长期运行表现。

### P1：工程化和发布

- [ ] 将 SDK 路径、交叉编译器前缀、触摸设备和开发板地址集中到配置中。
- [ ] 让构建脚本明确区分 host 构建、Rust core 构建、QuickJS 构建和 guest 构建。
- [ ] 增加一键构建、部署、启动、停止和查看日志的开发脚本。
- [ ] 上传时先写入临时文件，再原子替换目标文件，避免覆盖正在运行的可执行文件。
- [ ] 固定第三方 PocketJS、QuickJS 和构建工具版本，记录对应 commit。
- [ ] 补充开源项目所需的许可证、第三方依赖说明和可复现构建文档。

## 可选的 PocketJS 平台能力

以下功能目前没有移植，不应在基础显示链路稳定前同时展开：

- 音频播放和音量控制
- 网络访问
- 文件系统和持久化存储
- 摄像头或其他媒体输入
- 3D 渲染
- 调试协议和远程开发工具
- 键盘、实体按键和其他输入设备

这些功能是否移植，应该由后续实际产品需求决定。当前开发板没有预留实体按键，因此暂不实现键盘或按键输入。

## 推荐下一步

按以下顺序推进：

1. 完成 framebuffer 参数验证和基础性能统计。
2. 实现 damage 区域局部刷新。
3. 完善触摸校准、设备配置和异常恢复。
4. 做长时间运行和内存稳定性测试。
5. 在稳定基础上扩展多图片、文本、列表和页面交互。
6. 最后根据实际产品需求评估音频、网络和存储等平台能力。

## 当前暂不做

- 不引入 LVGL，PocketJS 继续直接使用 framebuffer。
- 不切换到 `armv7l-unknown-linux-musl` 方案。
- 不实现实体按键输入。
- 不为了展示功能而一次性移植所有 PocketJS 平台模块。
