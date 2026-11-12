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


## 2026-11-12：raylib 5.5 构建定义修复

现有锁定提交和 /MT、/MTd、/fp:precise 均保留。上游 CUSTOMIZE_BUILD 从 config.h 扫描宏时把条件分支中的 MAX_MESH_VERTEX_BUFFERS=9 和 =7 都加入编译行；GPU skinning 同时开启，UploadMesh 会访问下标7/8。项目的 src3d/cmake/Graphics.cmake 在创建 raylib 目标后移除这两个冲突值，明确设置单一 MAX_MESH_VERTEX_BUFFERS=9。不修改缓存中的上游源码，不升级依赖。

scripts3d/audit_build.ps1 现在从 Release/Debug 的实际 CL.command.1.tlog 验证该宏唯一且等于9。Debug实际应用自检补充 CRT heap check 并把测试诊断写stderr，避免无人值守测试卡在运行库对话框。需要通过完整创建、绘制和资源释放的实际GUI自检，不能仅凭无窗口单元测试证明图形内存安全。

正式应用另外通过 rlgl 公开接口轮换三组渲染批缓冲。批刷新长帧曾在该改动后再次出现，结束不用的捕获会话后独立复测恢复；没有确定驱动内部的因果链。图形负载、坐标、几何、物理步及锁定版本不变，最终结论仅取完整运行验收，失败与复测数据见 VALIDATION.md。
