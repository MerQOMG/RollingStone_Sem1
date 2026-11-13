# 山区滚石模拟 RollingStone3D

三维山区滚石模拟程序：用 Jolt 刚体引擎模拟真实凸多面体石块沿参数化山坡滚落，支持固定随机障碍、弹性挡板，按批次做蒙特卡洛统计坡脚 10 m 高度范围内的路线概率，并在 raylib 三维界面中查看轨迹、热图和逐次试验回放。

同济大学《程序设计（基础项目一）工业互联领域的程序开发》课程项目。

![场景页：参数化山坡、石块、坡脚统计带](docs/3d/images/ui_scene.png)

## 1. 功能

- **石块**：按外包半径 r、体积 V、顶点数 k 随机生成不规则凸多面体（质心、体积、完整惯量张量由实体积分得到），每次试验的石块由批次种子确定，结果可重复。
- **地形**：参数化山坡（坡顶高度 `height`、陡坡角、缓台起点/长度/坡角、坡脚平台长度）或平地（`type=flat`）；可设宽度、横坡角和沟槽振幅。
- **障碍与挡板**：同批固定随机障碍（碎石）；刚性或弹性挡板（`mode=elastic`：质量—弹簧—阻尼支承，真实位移与回弹）。
- **批量蒙特卡洛**：N 次随机试验，后台线程运行，可暂停、取消；结果事务式写盘，未写完的目录以 `.pending` 标记。
- **统计**：坡脚 10 m 统计带（见第 6 节）、收集截面、全路径热图、首次触板热图、逐试验轨迹与时间轴；未完成批次只给出频率的上下界，不当作概率。
- **三维视图**：透视 / 俯视 / 侧视 / 跟随石块四种视角，右上角导航轴，等高线、高程着色、太阳光阴影贴图，CC0 实拍地形纹理（可切回程序化地形）。
- **回放与结果文件**：保存的结果可用 `--view-results` 只读打开，直接读取记录，不重新计算物理。

## 2. 环境与依赖

- Windows 10/11 x64，支持 OpenGL 3.3 的显卡。
- Visual Studio 2022 或 2026，安装"使用 C++ 的桌面开发"（含 MSVC x64 与随附的 CMake）。
- Git（首次获取依赖时需要）和网络。
- 第三方依赖（版本和提交号固定，`scripts3d/bootstrap.ps1` 会校验）：

| 依赖 | 版本 | 用途 |
| --- | --- | --- |
| Jolt Physics | v5.3.0 | 刚体物理 |
| raylib | 5.5 | 窗口、渲染 |
| raygui | 4.0 | 仅作头文件依赖，界面控件在 `src3d/ui/widgets.*` 自绘 |

依赖放在仓库根目录的 `.deps3d/`（已被 .gitignore 忽略）。不需要单独安装字体：界面字体 Noto Sans SC 和地形纹理已在 `assets/` 中。

## 3. 构建

首次构建请先取依赖，再编译、测试：

```bat
powershell -ExecutionPolicy Bypass -File scripts3d\bootstrap.ps1
build_3d.cmd
test_3d.cmd
run_3d.cmd
```

- `scripts3d\bootstrap.ps1`：克隆并校验 Jolt、raylib、raygui 到 `.deps3d\`（`-CoreOnly` 只取 Jolt）。**正常流程应先运行它。**依赖若放在别处，可在 CMake 里用 `RS_JoltPhysics_SOURCE_DIR`、`RS_raylib_SOURCE_DIR`、`RS_raygui_SOURCE_DIR` 指定。
- `build_3d.cmd [Release|Debug] [--core]`：用 `vswhere` 自动找到本机最新的 Visual Studio（2022 / 2026）及其 CMake，不写死路径和生成器；已有的 `build\3d` 沿用原来的生成器。`--core` 只编核心库和测试，不含界面和 raylib。可设环境变量 `CMAKE_EXE` 指定另一个 cmake。Release 输出到 `build\3d\bin\`，Debug 输出到 `build\3d\bin\debug\`。
- 构建时会把 `assets\` 复制到 exe 旁边（`build\3d\bin\assets\`），所以直接双击 exe 也能找到字体和纹理。
- `scripts3d\package.ps1 [-Destination 目录]`：生成发布目录（exe、自检 exe、`assets`、`configs3d`、文档、依赖许可、`package-sha256.json`）。

## 4. 运行与命令行参数

不带参数运行 `run_3d.cmd`（或双击 `build\3d\bin\RollingStone3D.exe`）打开三维程序。快捷启动：

```bat
run_3d.cmd                                   :: 默认场景
run_elastic_3d.cmd                           :: 参数化山坡 + 弹性挡板
run_obstacles_3d.cmd                         :: 障碍 + 热图 + 时间轴演示，点“开始批量计算”
run_3d.cmd --view-results "results3d\run_..."  :: 只读打开已保存结果
```

完整命令行（以 `RollingStone3D.exe --help` 为准）：

```text
RollingStone3D: SI units, x downslope / y across / z up.
--config scene.ini --batch N --seed integer --preview 0 --headless [--output path]
--config scene.ini --single [--headless] [--output path]
--config scene.ini --batch N --preview 2   (real GUI previews, then background)
--view-results run_directory [--headless] [--diagnostic-results]  (read-only history)
--replay run_directory --trial id --headless
--self-test
--write-config scene.ini [--throughput-scene]
GUI: [--config scene.ini] [--view-results dir] [--tab scene|run|analysis] [--demo] [--scale 1.25] [--size 1366x768]
     [--terrain-tex NAME | --procedural-terrain] (default: bundled assets/textures/terrain; env RS3D_TERRAIN_TEX)
     [--tour --shots dir] (automated walkthrough with screenshots) [--duration seconds] [--single|--batch N start immediately]
No arguments opens the 3D app.
```

| 参数 | 说明 |
| --- | --- |
| `--config scene.ini` | 场景配置（INI，schema=2，兼容 schema=1）；`configs3d\` 有现成示例 |
| `--batch N` / `--single` | 批量 N 次 / 单次试验；GUI 模式下表示打开后立即开始 |
| `--seed` | 批次主种子（无符号 64 位整数） |
| `--preview 0\|2` | 批量前预览的试验数；0 为全部后台计算，2 为先实时播放前两次 |
| `--headless` | 不打开窗口，命令行直接计算或读取 |
| `--output 路径` | 输出根目录，默认 `results3d\`；支持中文和空格 |
| `--view-results 目录` | 只读打开已保存的结果（也可在界面用“打开结果…”选择） |
| `--replay 目录 --trial id` | 按记录的初始条件重新积分某次试验并与结果比对（无窗口） |
| `--self-test` | 启动构建或发布包里附带的自检程序 |
| `--write-config scene.ini` | 写出默认配置；`--throughput-scene` 写出吞吐基准场景 |
| `--tab scene\|run\|analysis` | 启动时打开的页签 |
| `--demo` | 演示模式（也可按 F5） |
| `--scale 1.25` / `--size 宽x高` | 界面缩放 / 窗口大小 |
| `--terrain-tex NAME` | 指定地形纹理；默认是内置的 `assets\textures\terrain`；`procedural` 等同下一项 |
| `--procedural-terrain` | 使用程序生成的地形外观（环境变量 `RS3D_TERRAIN_TEX=procedural` 同效） |
| `--tour --shots 目录` | 自动走一遍界面并截图，用于验收；`--duration` 限定时长 |

纹理加载失败时自动回到程序化地形，不会黑屏。调试用环境变量：`RS3D_NO_VIZ`（关闭增强着色器）、`RS3D_NO_SHADOW`（关阴影）、`RS3D_NOVSYNC`（关垂直同步测帧率）、`RS3D_TERRAIN_TEX`（等同 `--terrain-tex`）。

`configs3d\` 中的示例：

| 文件 | 内容 |
| --- | --- |
| `default.ini`、`batch.ini` | 参数化山坡（55.6 m 高），无挡板、无障碍，批量 N=100 |
| `parameterized_elastic.ini` | 参数化山坡 + 36 m 宽 × 8 m 高弹性挡板（`run_elastic_3d.cmd` 使用） |
| `obstacles_heatmap_timeline.ini` | 弹性挡板 + 固定障碍，用于热图和时间轴演示（`run_obstacles_3d.cmd` 使用；schema=2） |
| `with_barrier.ini` | 参数化山坡 + 刚性挡板（偏转 15°） |
| `elastic_barrier.ini` | 平地 + 弹性挡板 |
| `batch_throughput.ini` | 平地、稳定姿态释放、1000 次的吞吐基准 |
| `user_case_10_20_7_20.ini` | 一组给定的 k = 10–20、r = 7、V = 20 的用例 |
| `natural_preview.ini` | 12×12 m 平地、k = 30，预览 3 块石头的形状 |

## 5. 界面说明

三个页签（`Tab` / `Shift+Tab` 切换）：

1. **场景**：左侧编辑地形、石块、挡板、障碍等参数。修改先标为"待应用"，点底部"应用"（`Ctrl+Enter`）后生效，非法值会在字段旁说明原因；`R` 换一块预览石块；`Ctrl+S` 保存场景。
2. **运行**：`运行单次` 用预览石块实时观察；`开始批量计算` 按批次数 N 和批次种子做蒙特卡洛，可暂停（空格）、取消；可选先实时播放前两次，其余后台全速。
3. **分析**：路径频率、首次触板、坡脚 xy / xz 分布等热图；逐次试验回放，拖动时间轴，`E` 跳到首次触板瞬间，`← →` 逐帧，`↑ ↓` 换试验，空格播放 / 暂停；`Ctrl+O` 打开已保存结果。

三维视图：左键拖动旋转，右键或中键拖动平移，滚轮缩放；`1` 透视 · `2` 俯视 · `3` 侧视 · `4` 跟随石块。右上角导航轴可点轴球对齐视角，拖动圆盘旋转（x 沿坡向下，y 横向，z 向上，与输出文件一致）。"三维视图显示"里可开关轨迹线、热图贴地、坡脚统计带、等高线。`F1` 使用指南，`F5` 演示模式，`F12` 保存当前画面，`Ctrl + 加减 / 0` 缩放界面。

**坡脚 10 m 统计带**：x 取地形末端 40 m（或整个坡脚平台）、y 取全宽、z 取坡脚高程（默认坡脚中心）以上 10 m；在三维视图里画成半透明盒子，另有一个收集截面（x = 终点 − 10 m）。

热图读法：频率 = H / N；"条件概率"= H / B（仅在已触板的试验里）；批次未完成或有失败 / 超时时，只给出下界和上界，不是置信区间。首次触板图表示位置，不表示冲击力。

![演示模式：全部试验轨迹、坡脚统计带和收集截面](docs/3d/images/ui_demo.png)

## 6. 输出与 CSV

每个批次一个独立目录：`results3d\run_<日期>_<时间>_<进程号>_<序号>\`。

- `manifest.ini`：配置和身份（schema=3，状态 `status`、计数、依赖版本、源码哈希）；批次是否有效以它为准。
- `FILES_COMPLETE.txt`：写盘全部成功后才出现的文件清单。目录名带 `.pending` 或含 `INCOMPLETE.txt` 表示事务没提交，**不能使用其中的概率**。

| 文件 | 内容 |
| --- | --- |
| `scene.ini`、`trials.csv` | 场景配置；每次试验的状态与结果 |
| `trajectory.csv`、`motion.bin` + `motion_index.csv` | 轨迹点；逐帧运动记录（回放读取） |
| `collector_summary.csv`、`collector_yz.csv` | 收集截面统计与穿越点 |
| `route_heatmap_xy.csv`、`voxel_probability.csv` | 全路径热图；三维体素概率 |
| `projection_xy.csv`、`projection_xz.csv` | xy / xz 投影（按试验去重，不能相加成三维概率） |
| `barrier_trials.csv`、`barrier_trajectory.csv` | 每次试验的挡板状态；挡板位移轨迹 |
| `barrier_first_contact.csv`、`_heatmap.csv`、`_points.csv` | 首次触板记录、热图和点 |
| `shape_vertices/faces.csv`、`terrain_vertices/faces.csv`、`obstacles.csv`、`obstacle_vertices/faces.csv` | 石块、地形、障碍的顶点和三角面（供独立重放） |
| `attempt_rejections.csv` | 构造石块时被拒绝的尝试 |

单位 m / s / kg，坐标 x 下坡、y 横向、z 向上，数值使用小数点。列定义和统计口径见 [docs/3d/ANALYSIS_AND_TIMELINE.md](docs/3d/ANALYSIS_AND_TIMELINE.md)。

## 7. 目录结构

```text
RollingStone_ui-rebuild/
  src3d/              程序源码（CMake 工程）
    ui/               界面：应用壳、页签、参数面板、3D 视口(viewport3d)、渲染增强(viz)、控件(widgets)
    cmake/            依赖与构建脚本（Jolt、raylib、观察器）
    observer/         Jolt 接触观察器头文件
    *.cpp / *.h       核心：配置、地形、物理、批处理、统计、结果存档、回放
  tests3d/            测试（21 个 ctest 用例）
  configs3d/          示例场景配置
  scripts3d/          bootstrap / package / vs_cmake 与 audit_* 验收脚本
  assets/             字体 fonts/、地形纹理 textures/terrain/
  licenses/           字体与纹理的授权文件
  docs/3d/            设计、验收与格式文档
  .planning/          开发过程记录
  build_3d.cmd  test_3d.cmd  run_3d.cmd  run_elastic_3d.cmd  run_obstacles_3d.cmd
  build/              本地构建输出（已忽略）    .deps3d/  依赖（已忽略）    results3d/  运行输出（已忽略）
```

## 8. 测试

```bat
test_3d.cmd                    :: 等价于 ctest --test-dir build/3d -C Release --output-on-failure
test_3d.cmd --geometry-full    :: 另跑完整几何验收（耗时长，输出到 results3d\geometry_acceptance）
run_3d.cmd --self-test
```

21 个 ctest 用例：ABI、输入归属、几何（解析 / 形态）、地形网格、物理动力学、统计 DDA、配置快照、静止回归、批处理输出与回放、会话与播放控制、弹性挡板（含组合演示）、接触观察器、固定障碍、全路径与首次触板、运动分块、存档事务、只读回放、界面模型（含障碍场景）。验收记录见 [docs/3d/VALIDATION.md](docs/3d/VALIDATION.md) 和 `docs/3d/evidence/`。

## 9. 素材与授权

- 界面字体：Noto Sans SC（SIL OFL 1.1），`assets/fonts/`，授权 `licenses/NotoSansSC-OFL.txt`。
- 地形纹理：Poly Haven 的 [cliff_side](https://polyhaven.com/a/cliff_side) 和 [dry_ground_rocks](https://polyhaven.com/a/dry_ground_rocks)（CC0 1.0），`assets/textures/terrain/`，说明见 `licenses/terrain-textures-CC0.txt`。
- 代码依赖：Jolt Physics（MIT）、raylib 与 raygui（zlib/libpng）；发布包的 `licenses\` 附带各自许可原文。

## 10. 已知限制

- 首次触板图表示接触位置，不表示冲击力；小样本只给经验频率，不代表统计稳定。
- 石块在全景里很小（半径约 2 m 对 60 m 的坡）；纹理为 1K，近景略软。
- 地形纹理的平铺、亮度、饱和度等参数写在 `assets/textures/terrain/terrain.cfg`，修改后重启生效，界面里没有滑块。
- `scripts3d/` 里的 `audit_gui.py`、`verify_deployment.py`、`audit_performance.py`、`benchmark_analysis.py` 依赖已不存在的命令行选项（如 `--capture`、`--font`、`--exercise-view`），当前不能直接运行，仅作历史记录保留。

## 11. 小组成员

| 贡献者（GitHub） | 主要工作 |
| --- | --- |
| Shellmega | 凸多边形刚体与接触求解 |
| MerQOMG | 地形、前端搭建 |
| percy07071077-art | 弹性挡板及数值记录 |
| 1394989349-cmd | 批量蒙特卡洛模拟与 CSV 统计 |

## 12. 文档索引

- [docs/3d/README.md](docs/3d/README.md)：统计口径、几何与采样器、配置和结果格式的技术补充。
- [docs/3d/ANALYSIS_AND_TIMELINE.md](docs/3d/ANALYSIS_AND_TIMELINE.md)：热图、时间轴与结果文件格式。
- [docs/3d/DEPENDENCIES.md](docs/3d/DEPENDENCIES.md)：依赖、编译环境与许可。
- [docs/3d/SHAPE_SAMPLER.md](docs/3d/SHAPE_SAMPLER.md)、[docs/3d/ELASTIC_BARRIER.md](docs/3d/ELASTIC_BARRIER.md)：石块采样器、弹性挡板模型。
- [docs/3d/VALIDATION.md](docs/3d/VALIDATION.md)、[docs/3d/OBSTACLES_HEATMAP_TIMELINE_VALIDATION.md](docs/3d/OBSTACLES_HEATMAP_TIMELINE_VALIDATION.md)：验收记录。
- 开发过程记录：`docs/3d/PROGRESS.md`、`docs/3d/IMPLEMENTATION_PLAN.md`、`.planning/`。
