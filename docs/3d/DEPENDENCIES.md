# 三维依赖与构建记录

2026-11-12。下列固定依赖已通过P0a/P0b及首版P5集成复验；构建/原型选型证据与正式几何、物理、统计、窗口验收分列于VALIDATION。

| 依赖 | 固定版本 / 完整提交 | 来源 | 许可 | 本地缓存 |
|---|---|---|---|---|
| Jolt | v5.3.0 / `0373ec0dd762e4bc2f6acdb08371ee84fa23c6db` | https://github.com/jrouwe/JoltPhysics | MIT，仓库 LICENSE | `.deps3d/JoltPhysics` |
| raylib | 5.5 / `c1ab645ca298a2801097931d1079b10ff7eb9df8` | https://github.com/raysan5/raylib | zlib/libpng，仓库 LICENSE | `.deps3d/raylib` |
| raygui | 4.0 / `25c8c65a6e5f0f4d4b564a0343861898c6f2778b` | https://github.com/raysan5/raygui | zlib/libpng，仓库 LICENSE | `.deps3d/raygui` |

正式发布tag固定并本地核对HEAD。参考[raygui 4.0发布说明](https://github.com/raysan5/raygui/releases/tag/4.0)与[raylib 5.5发布说明](https://github.com/raysan5/raylib/releases/tag/5.5)选作候选；本机编译/运行证据才决定配对是否可用。

`scripts3d/bootstrap.ps1` 下载并验证这些源代码；不会安装编译器。缓存不提交。已有源码的离线路径可在CMake设置 `RS_JoltPhysics_SOURCE_DIR`、`RS_raylib_SOURCE_DIR`、`RS_raygui_SOURCE_DIR`，要求对应固定HEAD且无跟踪文件修改。核心构建 `build_3d.cmd Release --core` 不读取图形依赖。

编译器沿用 VS2022 Enterprise / MSVC 19.44.35228，x64、v143、Windows SDK 10.0.26100.0。优先使用该VS附带 CMake 3.31.6-msvc6；`CMAKE_EXE` 可指定另一现有CMake。所有目标/MT，Debug /MTd；Jolt静态，双精度位置、确定性开关开，C++异常开，硬件FP异常关，IPO关，数值代码/fp:precise。消费者链接 Jolt CMake 目标传播ABI宏。Debug/Release构建与消费者均已运行通过；实际编译命令审计见 `build/3d/evidence/compiler-audit.json`。采用/Z7嵌入式调试信息以避免本环境PDB服务器C1902。

raylib使用固定源码附带GLFW、OpenGL 3.3桌面后端及其内嵌第三方代码；相关许可见该固定树的 `src/external` 与 `src/external/glfw`。不另外下载GLFW。raygui仅在 `prototype.cpp` 定义实现。音频模块关闭。

中文字体默认 `C:/Windows/Fonts/simhei.ttf`；只在本机读取，不随程序提交或分发。可用 `--font` 指定有权使用的中文TTF/TTC；宽字符路径以filesystem/ifstream读字节，再交给LoadFontFromMemory。缺文件或缺必需字形明确失败，不静默显示乱码。
