# RollingStone3D

RollingStone3D 使用固定 r/V 随机凸多面体、真实三维刚体、参数化地形和刚性/弹性挡板。新增同批固定随机障碍、全部 trial 轨迹、全路径与首次触板热图，以及从保存帧读取的全 trial 时间轴。操作、统计口径、格式和兼容说明见 [障碍与分析回放](ANALYSIS_AND_TIMELINE.md)；本次验收证据见 [F0–F5验收记录](OBSTACLES_HEATMAP_TIMELINE_VALIDATION.md)。原版、采样器与弹性挡板的历史验证保留在 VALIDATION.md。构建、运行、命令行参数和界面说明见 [根目录 README](../../README.md)。

构建、运行和命令行参数见 [根目录 README](../../README.md)：先 `scripts3d/bootstrap.ps1` 取依赖，再 `build_3d.cmd`、`test_3d.cmd`、`run_3d.cmd`。

几何完整验收可运行 `test_3d.cmd --geometry-full`，顺序重跑先1100用户case、再补足5000样本；日志在`results3d/geometry_acceptance`。另用 `python scripts3d/audit_geometry.py results3d/geometry_acceptance`独立核对样本数、边界覆盖和记录误差。生产支持k10–100，默认20–50/r2/V7；k4–9仅为显式几何内核回归。当前使用凸包顶点内缩与各向形变的自然形状采样器（convex-retreat-com-radius-v3），仍不是所有定体积凸多面体上的均匀分布；最终5000样本中7个trial发生重试、0最终失败；0/5000不证明所有种子均成功。

生产支持域为 `10 <= k_min <= k_max <= 100`、`0.2 <= r <= 10 m`、`V/r^3 >= 0.02`，且 `V <= 0.9 * min(V_ref(k,r))`（对区间内所有k取最小值）。这里 `V_ref=(k-2)*sin(2*pi/(k-2))*r^3/3`，沿用首版的保守容量限制，不是所有球内多面体的数学体积上限；程序会拒绝范围外输入，不自动改变r/V。按用户确认的新定义，r为实体质心到最远顶点的距离，其他顶点允许位于球内；不是最小包围球半径。随机有间隔方向、保持极点的逐顶点内缩及各向形变生成不同棱角和长宽厚，再联合求解r/V。详见 [自然落石采样器说明](SHAPE_SAMPLER.md)。

新版规范几何使用实体质心为原点的double顶点和三角面，旧数据重放保留其原坐标系，闭合实体积分得到体积、COM与完整惯量张量。Jolt使用 `(顶点-COM)/r` 的零圆角凸形状，再以ScaledShape恢复米制；引擎float顶点/质量的误差单独读回校验。渲染以同一规范网格减去COM，使用实际Jolt形状原点和四元数变换，显示质心另取Jolt实际COM；二者不混淆。CSV保留规范顶点与变换，误差容差与5000样本实测值见VALIDATION。挡板直接由同一场景的有限盒生成，另从实际Jolt Body/BoxShape读回与显示网格核对。

核心验收入口还有`build/3d/bin/rs3d_physics_tests.exe`（D1-D7数值测试；可带D1等名称单测）及`rs3d_statistics_tests.exe results3d/dda_oracle.csv`。后者保存4000条线段计数，`python scripts3d/audit_dda.py`用独立的精确有理数求交复核，不依赖Jolt或图形。

`build_3d.cmd Debug` 编译Debug到 `build/3d/bin/debug`；Release到 `build/3d/bin`。`build_3d.cmd Release --core` 不读取或链接图形依赖。离线源码路径见DEPENDENCIES.md。

已可运行的无窗口入口（`--headless`不初始化图形窗口）：

```powershell
./run_3d.cmd --config configs3d/default.ini --batch 100 --seed 20261111 --preview 0 --headless
./run_3d.cmd --config configs3d/user_case_10_20_7_20.ini --single --headless
./run_3d.cmd --config configs3d/batch_throughput.ini --batch 1000 --headless
./run_3d.cmd --replay "results3d/run_实际目录" --trial 1 --headless
./run_3d.cmd --self-test
python scripts3d/audit_batch.py "results3d/run_实际目录"
```

组合演示和独立只读浏览：

组合预设已启用弹性挡板，顶部显示当前和峰值位移。当前固定种子下第4次有真实撞击，约11.87s位移1.383m、峰值1.384m；前3次未触板。完成至少4次后选择trial 4查看回弹，详见 [组合演示修复](ELASTIC_DEMO_FIX.md)。

```powershell
./run_3d.cmd --config configs3d/obstacles_heatmap_timeline.ini
./run_3d.cmd --config configs3d/obstacles_heatmap_timeline.ini --batch 100 --preview 2
./run_3d.cmd --view-results "results3d/run_实际目录"
./run_3d.cmd --view-results "results3d/run_实际目录" --headless
```

窗口先应用“固定障碍”参数，再点击“批量”。“分析”打开全路径/首次触板/坡脚图层；选 trial 后拖下方时间轴，首次触板热格可筛选试验并跳到事件。“打开结果”选择结果目录中的 `manifest.ini`，不会重新积分；原 `--replay` 仍用于数值重积分验证。完整度、未知数和两种分母必须随图一起阅读，小样本可看经验频率，不表示统计稳定性。

碎石现默认分布在全坡面，24块，长宽0.20～0.50m、露出高0.06～0.18m，按米独立设置，不随滚石 r/V 同步缩放。使用新版 `run_obstacles_3d.cmd` 或随附预设；旧保存方案保留旧口径，可在“固定障碍”改为 `placement=terrain` 与 `size_mode=absolute_m`。实际对照及兼容性说明见 [碎石分布修复记录](OBSTACLE_DISTRIBUTION_FIX.md)。

新配置为 schema=2、兼容 schema=1 的 UTF-8 INI；新结果为 schema=3，数值使用小数点；区间写成`lower,upper`，固定值写一个数。`--output`可覆盖输出根目录，支持中文/空格；Ctrl+C在完整物理步或生成迭代边界取消。每批使用独立目录；`.pending`与`INCOMPLETE.txt`表示事务未提交，不能使用其中的正式概率。写盘全部成功后才提交目录，`FILES_COMPLETE.txt`是文件完成清单，批次是否有效仍须看`manifest.ini`的status。

坐标为x下坡/y横向/z向上，单位m/s/kg。默认参数化坡面和支撑间隙0.05m，用户case释放x=20m；不以外包半径代替实体支撑高度。默认统计盒取地形末40m、全宽和坡脚中心高程上方10m；`toe_min`是显式替代基准，两者均不是局部离地高度。完整批概率为H/N；未进入统计区的正常试验仍在分母。任一最终失败/超时/取消使正式概率留空，诊断范围为[H/N,(H+N-C)/N]，不是置信区间。xy/xz按每次试验去重，不能相加三维概率。截面记录首次正向穿越，另列未穿越和首次穿越窗外。

同k最多64次构造尝试；重试成功只占一个trial，不重抽物性。最终失败保留槽位并继续原定编号，取消或I/O失败停止未开始项。manifest含源码内容SHA256、锁定依赖、配置hash、各计数和耗时；顶点/三角面/质心变换、实际姿态和地形CSV供独立重放。轨迹文件可降采样或关闭，但统计始终逐0.002s逻辑步执行，每步4个碰撞子步。投影、相机和显示帧率不能消费物理随机流。

`batch_throughput.ini`是专用水平面稳定基准：r2/V7/k20–50、e0、默认摩擦、零速度、零间隙、10s上限，显式`orientation=stable_face`将最接近质心的支撑面朝下。该夹具不改变采样几何或随机数，实际四元数写入输出；普通场景`orientation=sampled`保留采样姿态。随机姿态在10s内不一定满足连续2s静止，原失败基准也保存在VALIDATION。滚阻是按重量定标的接触切向角耗散，不含绕法线扭转阻力；空气阻力用pi*r²面积，均未作工程材料标定。

界面布局、视角、显示开关与操作说明见根目录 README 的“界面说明”。

```powershell
./run_elastic_3d.cmd
./run_3d.cmd --config configs3d/with_barrier.ini
./run_3d.cmd --config configs3d/user_case_10_20_7_20.ini --single
./run_3d.cmd --config configs3d/batch.ini --batch 100 --seed 20261111 --preview 2
./scripts3d/package.ps1 -Destination 'build/3d/package/new-release'
```

发布文件夹含exe、自检exe、配置、文档和依赖许可；`run_3d.cmd`在任意当前目录下切换到包目录启动。VS和第三方运行时DLL不属于运行前提，程序使用静态/MT依赖；系统OpenGL及Windows组件仍必需。字体（Noto Sans SC，OFL）和地形纹理（CC0）放在包内 assets 目录。`--self-test`需要随包的测试exe；完整5000几何验收使用仓库测试脚本。

历史独立发行目录为`build/3d/package/RollingStone3D-parameterized-elastic`，双击其中的`run_elastic_3d.cmd`打开参数化坡面与弹性挡板碰撞示例，`run_3d.cmd`打开默认场景；源码入口仍是仓库根目录的同名命令。原平地弹性版、自然形状版和v1发行目录保留。大体积发行包和结果未提交Git，换机器须按本文重建或复制发行目录。真实输出示例在`results3d/natural_v3_final1000/run_20261112_084735_13532_1`；其中manifest、trials、voxel_probability、collector_yz和projection_xy/xz给出配置、状态与概率。

查看自然落石：`run_3d.cmd --config configs3d/natural_preview.ini`。这个12×12m平地场景便于观察形状；点击“重新随机”查看不同样本，默认坡面仍可正常使用。新旧采样器使用相同seed时形状会不同，旧结果请通过--replay读取实际顶点；完整运行身份包含manifest中的sampler和radius_definition，不能只凭参数hash混合统计。

![实际生成的不规则落石](images/natural_rocks.png)

新形状的部分支撑面在原Jolt接触归并设置下会对平面网格对角线敏感。当前保留完整接触流形，manifest记录`manifold_reduction=0`；旧CSV回放缺少此项时恢复旧设置，已验证旧/新结果回放位置误差均为0。该修复不改变dt、子步和静止判据，新增两种网格回归。Release/Debug均通过12项CTest，包括独立形态与弹性支承检查；新增图表是实际网格示例，不能作为地质形态标定。
