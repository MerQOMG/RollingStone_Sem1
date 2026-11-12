# RollingStone3D 执行记录

2026-11-12，单 agent；P0–P5首版门槛全部通过，P6交付审计完成。包含本记录的提交就是本任务唯一交付提交，提交号见Git历史及最终回复；不另做追补提交，不push。

## 约束与起点

- 起始 HEAD `838923b`，分支 `master`。最终交付前仅一次提交，不 push。
- 用户已有修改保持原样：`RollingStone.sln` SHA256 `B17D44CC220737BF23A8ED698F2DD6603330E4F6CE0B1F5D79A8B623B22B5907`；`docs/3d/IMPLEMENTATION_PLAN.md` SHA256 `12336A7CEC804FD4F5F2FDC1CF20CF8D8A07E855217DB3ECD581756164D02205`。
- 已完整阅读 IMPLEMENTATION_PLAN / GOAL_PROMPT。没有发现适用 AGENTS.md。
- 用户确认修订口径：生产采样 k=10–100，默认20–50、r=2、V=7；4–9作为内核回归；5000个生产样本覆盖10–100，先1100个 k=10..20 / r=7 / V=20 样本。
- 沿用已有计划与本记录，简化 planning-with-files 流程，不创建重复的根目录计划。

## 阶段

| 阶段 | 状态 | 下一项 |
|---|---|---|
| P0a | 已通过 | ABI/零圆角/命令行证据见VALIDATION |
| P0b | 已完成原型门槛 | 实际渲染/原生控件+自动相机序列；完整边界与性能门槛见VALIDATION |
| P1 | 已通过 | 1100用户样本及5000完整覆盖、G2-G5；见VALIDATION |
| P2 | 已通过 | D1–D7及P4显示/碰撞读回复验通过 |
| P3 | 已通过无窗口门槛 | S1–S5数值/文件、N2/3/1000和7.5；真实GUI预览另属P4 |
| P4 | 已通过 | 原生交互、真实预览/后台、GUI数值独立性 |
| P5 | 已通过 | 固定/真实负载、普通cmd部署、ACL、两模式与二维回归 |
| P6 | 交付审计通过 | 包含本记录的唯一提交交付，不push |

## 构建基线

- VS2022 Enterprise：`C:/Program Files/Microsoft Visual Studio/2022/Enterprise`。
- 使用VS附带CMake3.31.6；生成器固定VS2022 x64 v143。
- 官方远程tag和本地下载HEAD一致，具体SHA见DEPENDENCIES；三库编译、ABI消费者与图形原型均已实际运行。

## 失败记录

已修复：CMD参数拆分、CMake依赖变量冲突；本机沙箱多节点MSBuild停滞与PDB C1902，改用单节点与/Z7。详见VALIDATION。

## 2026-11-11续跑

- 上一轮为实质进展：Release/Debug构建成功，两套ABI消费者退出0；36组UI投影往返与最近拾取/遮挡自动测试退出0。原GetScreenToWorldRayEx在此远近裁面组合下出现浮点反投影误差，换用解析透视逆投影后通过同阈值测试。
- 用户暂停后恢复，权限为full-access。没有遗留编译或GUI进程，已核对原用户文件SHA256不变。
- 真实OpenGL帧缓冲图显示中文、地形/多面体/挡板及轨迹正确；窗口捕获接口返回白屏或旧画面。现已使用原生窗口输入、真实帧缓冲和事件日志完成控件焦点/选择/非法输入验证；连续拖拽用实际相机更新函数的多帧序列验收，没有把工具合并的终点点击当作原生连续拖拽。
- `results3d/capture_check/framebuffer.png` 是程序真实帧缓冲截屏（不是生成图）。`results3d/ui-self-test/events.txt` 为坐标自动测试记录。
- 两轮完整固定负载基准均结束，v2含339软件响应探针；初始100ms最大响应草案失败，完整基线之后、正式物理之前固定250ms最大/P95 50ms，帧P95 33.334ms/P99 50ms，内存512MiB。详见VALIDATION实测表，后续不能再放宽。
- P1开始；本任务尚未Git提交。用户原文件hash再次匹配起点。

## P1结果

- 实现geometry/math/random/engine_geometry；核心不依赖窗口。优先Jolt归一化拓扑，double支撑/闭合/积分校验，必要时增量double凸包后备；引擎统一COM/r归一化、零圆角、ScaledShape还原米制及质量读回。
- 首1100样本：每k10..20共100个、r7/V20，全部首尝试成功。随后5000样本首轮634个边界输入因浮点舍入误拒绝（未进入构造）；原证据保留results3d/geometry_5000。
- 输入上下界比较增加8*DBL_EPSILON舍入保护，并验证偏离1e-12的真实越界仍拒绝；没有改变r/V或几何/引擎误差阈值。完整重跑results3d/geometry_5000_v2：5000成功、0最终失败、0重试，21个最终拓扑使用double后备。独立coverage_audit.json确认1092个生产边界组合全覆盖。
- G2/G3显式正四面体/立方体/八面体及非对称旋转张量、G4非法参数/顶点、k4..9内核回归、种子金标准、同k注入重试/64次耗尽/取消均通过。Debug与Release快测通过。
- 下一步P2：参数化地形及精确格线、Jolt完整刚体/接触/阻力/刚性挡板/唯一Trial时间步；执行D1-D7。尚无三维物理、正式GUI或CSV。

## P2当前现场

- 新增terrain.cpp/h，复用二维参数公式但不改动/include二维main；共享网格包含所有折点和精确末端，x/z剖面+横坡/沟槽，FNV1a64内容哈希。height读取实际分片网格；toe_center/toe_min分开。
- supportHeight对岩体底面与地形三角形XY投影做多边形裁剪，在交叠顶点求最大竖向分离，避免只看石块顶点或z+r。
- Release/Debug terrain_tests均退出0：默认x_end=99.06656562、h(30)=34.00858642、11.25折点、边界/面绕序、横坡/沟槽基准、两种对角线支撑一致、坡脊接触石块底面内部的反例通过。默认8080三角形、hash fnv1a64:1611be23d4ad29ba。
- 当前Release CTest 4/4通过（ABI/input/geometry/terrain）；绝非D/S/I正式全套。暂无physics.cpp/trial.cpp。没有活动GUI/基准/几何进程，无任务提交，用户两文件hash未变。
- 下一项：基于Jolt JobSystemSingleThreaded及ContactListener实现PhysicsWorld/Trial，默认零引擎阻尼、Z重力、4碰撞子步、接触对材质、slop/CCD、阻力/受限滚阻；先D1–D6，再有限挡板D7。固定版本相关头已读，后续继续以本地源码核对API。

## P2数值验收完成

- 新增physics/trial，PhysicsWorld::advance私有且只由Trial::step调用；Jolt单线程JobSystem、每步4子步、无引擎阻尼、开启陀螺力。COM与形状原点分开，独立显示快照；双精度几何质量和实际引擎质量严格区分。
- 按计划设置slop、恢复阈值0.15、CCD；地面/挡板静态体材料由显式combine回调返回。接触监听记录包括speculative manifold，不能把回调个数当冲量/接触力；滚阻只使用接近实际接触且未快速分离的地面法线，受惯性限制的角冲量不反转原切向分量、不增转动能，离地不施加。
- Release/Debug physics_tests 9组均通过（D1-D7+dissipation+termination），详见VALIDATION和physics-release/debug日志。修复的是测试观测：原先只统计已穿透接触漏掉提前处理的碰撞；原偏心挡板case的COM仍在接触斑内，可以无转矩，现COM移到挡板边外但保留真实重叠条带，验证正确角动量方向。没有放宽D阈值。
- 50m/s旋转薄体碰地/挡板、无耗散能量、接缝、固定4子步收敛、复杂坡面步长敏感性、实际摩擦差异均已运行。初始石块-地形/挡板重叠明确拒绝；默认挡板按覆盖区域最高地形支撑放置。
- 下一步P3：S1半开体素DDA/截面/投影去重，UTF-8 INI配置与种子流、批次状态/输出事务/重放；实现统计后先50次分段吞吐，再N1000完整验收。GUI仍是P0原型，尚无正式概率CSV或预览批次，任务未提交。

## P3当前现场

- 新增statistics.h/cpp：半开体素（最外边界闭合）、闭线段DDA穿越、逐试验体素/xy/xz去重、首次正向collector截面、正常完成计数C、不完整批正式概率空值和[H/N,(H+N-C)/N]。精确角点本身也计入其半开格归属；混合方向穿角可以多出一个仅瞬时访问的owner格。
- 初版DDA按容差合并很近的过格时刻，独立逐格区间oracle发现漏格/多格；去掉容差后，普通double除法也会把不同过格时刻舍入为同值，甚至反向路径不同。已实现本地ExactParameter：普通比较快速路径，近相等时用error-free差/和与显式FMA余项组成短浮点展开，比较原始线段的交叉乘积。不是新增第三方几何库。
- C++ Release/Debug S1测试均通过：手工边界/角/反复/区外夹具，4000条固定seed线段和其反向，独立逐格区间求交对照；collector也用参数比较避免插值舍入到相邻格。Python标准库Fraction对4000条原始binary-double输入完全独立核验通过，results3d/dda_oracle.audit.json记录输入SHA256。
- 当前Release CTest 6/6通过（ABI/input/geometry/terrain/physics/statistics）。这只是统计内核的S1/S4概率公式/部分S5投影验证，S2/S3/S4完整失败输出/S5重放和吞吐均未实现，不能宣称S全套通过。
- 续跑下一项：config.h/cpp和版本化UTF-8 INI（固定/区间及协议消费顺序）、RunConfig冻结/hash、BatchRunner与Output事务；再50次分段基准和1000稳定批。没有活动测试进程或GUI，未Git提交/push，二维及用户原改动保持不变。

## P3配置、批次、输出与重放完成

- config严格解析schema=1 UTF-8 INI，拒绝重复/未知键、NaN/Inf/尾随文本、无效范围；保存用同目录临时文件及宽字符原子替换。RunConfig深拷贝草稿、持有只读地形并记录hash，区间与固定字段均按协议消耗一个u。中文/空格路径、toe_center/toe_min和种子0测试通过。
- BatchRunner为同步数值入口，GUI可由一个受控工作线程调用；BatchControl用条件变量在完整步/生成迭代边界暂停取消。每步统计，域出口裁剪最后一段；只聚合正常trial。preview observer=0/2的输入、末态、顶点/轨迹/四类概率CSV逐字节一致，N2恰好2预览事件、N3为2预览+1后台；这不代替真实GUI播放时钟。
- Output事务写入唯一.pending目录，flush/close所有文件和清单后原子提交。正式/不完整批均记录分母和上下界；中途及最终概率写盘失败保留io_error，后者清除可能部分写出的正式概率。实测ACL拒绝创建子目录返回code5、退出1；原ACL已恢复。失败/超时/取消不补抽。
- S5读取实际shape_vertices/faces、姿态、物性和terrain CSV，独立重积质量属性并核对地形内容hash，不靠seed重新采样。测试终态误差0；用户case k10–20/r7/V20的实际单次6.828s静止，CLI重放误差0。
- Release/Debug CTest 9/9通过；增加late I/O与暂停中取消后，两模式batch_tests均通过。当前Release/Debug均包含图形原型和无窗口核心，编译命令审计覆盖新config/batch/settling目标及主程序的Jolt PUBLIC宏。证据见VALIDATION。
- 吞吐首50随机姿态24超时；定位滚阻只检查接触斑首点并错误使用COM分离速度，改为全部实际锚点及含omega叉乘的接触点速度，D1–D7及耗散回归通过。原样重测仍5超时：静止开始太晚，未满足连续2s，诚实保留。
- 7.5未指定初始姿态，稳定专用夹具显式采用最近COM支撑面朝下（配置orientation=stable_face），保持默认r/V/k、e0、零速度/间隙、原2s静止和10s截断，普通配置不变。50次0失败/超时、0.8864744s；预测1000为17.729488s。1000实际0失败/超时、16.8902934s，1,025,134 Update/4,100,536内部子步；输出写盘计入总墙钟。独立Python审计完整CSV及失败首50CSV通过。
- 下一项P4：复用P0 RenderTexture/InputRouter/相机，把正式RunConfig和实际Polyhedron/PhysicsScene接入；参数分页/应用校验、选点释放/挡板编辑、配置文件对话框；单次冻结独立preview_seed样本，正式批次前2次按1倍模拟时钟、之后仅进度。GUI主线程保留图形API，1个工作线程，暂停/取消/关闭受控清理。然后真实窗口I1–I5，P5 I6/I7与二维回归，最后唯一任务提交。
- 本任务仍未commit/push。用户两文件SHA256与起点一致；没有活动GUI/数值进程。

## P4工作线程/播放控制现场

- 新增session.h/cpp（无图形依赖）：Idle/Running/Paused/Cancelling/Completed/Failed、Generating/Preview/Background/Finished，只有1个数值工作线程。共享只读场景和最新1份快照，图形API仍留给之后的主线程UI。
- BatchControl增加可中断的播放时钟等待，累计暂停时长；外层step前暂停和播放等待中暂停都不造成恢复后追赶快放。前两trial按sim time实时播放，后续不发布动画快照。没有快照无界堆积。
- `preview.seed`独立保存，排除在正式批次hash/随机流之外；重新随机只改预览，正式批次仍从master seed开始。单次运行冻结当前PhysicsScene/Input/ShapeDiagnostics，manifest中的单次master_seed取preview_seed，与实际输入相符，不重抽形状。
- Release `rs3d_session_tests.exe`退出0（session-release.txt）：N3前2真实时钟播放+1后台，preview模拟总4.014s、墙钟4.11758s；与headless无预览整数统计/末态相同。非法编辑保留原场景、运行及暂停时拒绝apply/randomize、单次冻结预览、暂停不推进显示快照；暂停中取消数值确认47.008ms，工作线程结束。这不是OS输入到绘制延迟，也不是实际窗口I4/I5。
- Release batch/config在新增时钟控制后复测通过。Session尚未接入main/图形UI；Debug新Session目标尚未构建。下一步app.h/cpp：复用P0的字体/RenderTexture/DPI/OrbitCamera/InputRouter，实际地形/石块/挡板模型、最近拾取、参数分页、释放点/挡板编辑、UTF-8配置对话框、单次/批次/暂停/取消/重置；保留--ui-prototype原入口。之后执行窗口I1–I5、Debug、I6/I7与P5。
- 无活动工作线程、测试进程或GUI。本任务仍未提交。

## P4正式窗口接入与原生操作（继续）

- 新增app/app_view/editor/native_dialog；正式GUI现绘制PhysicsScene真实地形/多面体和Jolt姿态，独立preview_seed，单次冻结样本。模型与拾取共享姿态，RenderTexture具深度附件，DPI=1.25实窗验证。
- 正式图形自检：36种尺寸/DPI/视角投影、最近落石/地形/空白/挡板拾取、真实Trial推进后的姿态渲染变换通过；刚增加Jolt实际挡板Body/BoxShape读回与显示网格逐角点核对，待修复版重跑。
- 原生窗口PID26680：中文/单位可读，nan输入拒绝且保留原形状，编辑焦点捕获滚轮，退出编辑后缩放正常；拾取石块/有限挡板、侧视、编辑挡板x=85、选地形释放点、独立相机保存/恢复均实际操作。最大化至2048x1209逻辑窗口后拾取落石正确。
- 原生N3输出run_20261111_231613_26680_2：前2次实时预览、第三次后台，C3/3且complete；暂停原生确认12.7261ms，持续暂停快照不动，参数锁定而相机可用，暂停中取消标记cancelled。运行中关闭窗口也安全取消、worker_finished=1且进程退出。事件保存在results3d/app-native/events.txt，未把数值线程延迟当屏幕响应。
- 实测保存按钮失败FNERR_INVALIDFILENAME(12290)：默认相对路径有正斜杠；现转换绝对Windows路径并检查父目录。另修复旧批次结果遮盖编辑/保存错误、耗时过长小数、控件数值显示（最短可往返小数，uint64种子不经double）。修复版保存/加载尚待实际复验。
- 增加诊断--exercise-view，经应用自身相机控制/resize/最小化/恢复/暂停路径验证真实GUI数值独立性；属于软件自检，不能冒称原生输入操作。--benchmark真实预览循环运行以覆盖3x30s取样，后台使用足量试验。P4尚未全部通过，P5尚未完成，无Git提交。

## P4验收补齐 / P5开始

- 正式保存/加载原生往返已通过：results3d/app-native-v2/场景 round trip.ini，先保存N3，再改N2运行，最后加载恢复N3及原hash。FNERR_INVALIDFILENAME已修复。N2暂停/继续与最小化期间正常完成C2；通过原生Win+Up恢复后观察完整状态。
- 原生菜单展开/滚轮隔离、弹出背景对比度、r=0错误就近提示/底部全文且保留输入及原场景，在app-native-v3复验通过。数字控件为最短可往返小数；64位种子测试保持精确。
- --app-self-test实际Jolt挡板BoxShape/Body读回与渲染网格最大差2.749963e-6m；actual stepped pose/DPI/nearest tests通过。D7数值碰撞与同一场景来源的渲染核对分列记录。
- scripts3d/audit_gui.py实际启动5种模式：headless、GUI preview0、GUI preview2在30/60/0帧率，并由应用自身控制器执行旋转、resize、最小化/恢复、暂停/恢复、关闭轨迹。所有试验物理字段（排除耗时）相同，11个几何/轨迹/概率/诊断CSV逐字节一致，轨迹差0；每种GUI2均见两次真实预览和第三次后台。证据results3d/gui_matrix_v1/audit.json，不冒称这段为OS原生鼠标序列。
- GUI N1000稳定夹具在两次真实预览后完成1000/1000，updates1,025,134/internal4,100,536，与headless基准一致；约22.7s含4s实时预览。独立CSV审计通过。
- 原生后台取消N10000在第2140槽结束，C2139、余7860未开始，正式概率空值；输入到worker_finished确认47.719ms，未超过P0冻结最大250ms。几何迭代取消另由生成器/Session/Batch自动测试验证，不把偶然捕获“正在生成”的UI帧当作精确命中迭代的证据。
- Release与Debug CTest10/10通过（ctest-release/debug-p4），两模式15目标编译命令审计通过。二维build_msvc.cmd --test退出0，原几何/弹性挡板/批量自检通过。用户两文件hash仍与起点相同。
- P4交互/运行门槛通过；P5已开始固定原型负载复测，之后实际预览/后台3x30s及中文路径非开发环境部署；性能门槛未更改。未commit/push。


- P5原型压力复测第一轮P95约44–46ms失败；仅缓存三角函数及批量CPU提交的第二轮仍47ms失败。两套原始证据保留p5_prototype_benchmark/p5_prototype_cached，门槛未变。
- 现用StaticLines把固定点/线段一次上传GPU，系统opengl32的glDrawArrays(GL_LINES)提交（这是raylib原本已依赖的系统OpenGL，无新第三方库）。保持1×2000/50×2000点、1999/99950线段、全部地形面与深度，动态物理/统计未改。初次编译因rlSetVertexAttribute的offset在锁定版是int失败，修正0后Release构建退出0；p5_static_lines_views是误用了此前缓存版的图，不能作为新代码证据，真正新实现截图为p5_static_lines_views_v2，shader6/7编译成功。
- 新实现斜视/侧视/近裁剪实际帧缓冲已检查，完整基准进行中；前两压力轮P95=24.5835/24.1273ms，尚待最终全套审计。仅CPU缓存失败、静态GPU上传改善，支持上传/同步开销是主要原因的推断；并未测出单项GPU时间，不声称精确归因比例。
- 已准备package.ps1（完整依赖许可、系统字体不打包）与verify_deployment.py（普通cmd+仅system32 PATH，中文空格目录/字体/导出，实际GUI和headless、缺字体/配置/错误输出路径）。尚未执行部署验证。app错误退出码和duration严格校验刚修正，待当前原型基准结束重建；不能把当前运行二进制当作包含这两处末段修改。
- P5原型静态GPU版完整六轮已通过，360软件探针max52.1654ms；真实预览三轮已通过，180探针max29.4189ms。最新Release/Debug及编译命令审计通过，app退出码/duration修复已包含。正在运行实际后台性能，随后执行中文路径部署和最后审计；没有任务提交。


## P5最终结果与P6交付

- 原型六轮、真实预览/后台各三轮全部通过冻结预算。CSV格式化末段修复后最终预览P95<=24.832ms，后台<=24.1468ms；各180软件探针最大45.9976/31.6505ms。早期压力失败证据和边界完整保留VALIDATION。
- Debug最终复测曾9/10，取消写盘超250ms；先计时定位概率表格式化，流复用无明显改善，改std::to_chars/general/17位。未改阈值或漏输出格；最终两配置10/10，Debug暂停取消额外三轮46.3/62.7/46.8ms。GUI/headless五模式再通过，优化前后11个确定性CSV SHA256一致。
- 中文空格独立包在普通cmd/system32-only PATH中启动并实际导出，缺字体/配置、文件型输出根均正确失败。真实NTFS拒写code5/exit1，测试新目录原ACL已恢复。实际GUI帧缓冲中文/单位/模型已查看；编辑挡板Jolt读回误差1.90735e-6m。
- 最终N1000稳定批全部正常、0最终失败/超时，18.8216657s；逐步DDA和正常输出完整，1,025,134 Updates/4,100,536子步。独立CSV审计通过。首次50预测、历史1000及最终复跑分列，普通随机姿态超时未隐藏。
- 交付源码内容SHA256 d94c71c62463cd49bdfbebdcc28987e8a8b0f42ff4993b7a6e1886a7615cb1e8；已验证exe SHA256 69a13a3bd8ac779a90f03fcec072ccc38fe6a6cf54a6894dce84718db0a60172。最终发行包仅更新配套文档，沿用相同exe；提交前构建的旧source_head与源码内容身份在VALIDATION明确区分。
- P6只暂存本任务文件。原两用户文件hash与起点一致，二维源码不变，缓存/输出/字体不入Git，不派子agent，不push。唯一提交保存这份冻结交付记录；以后自查`git log -1`取得提交号。


## 2026-11-12 追加：自然不规则落石（首版提交 d591a06 之后）

- 用户指出双锥外形并要求自然不规则落石，随后明确同意r改为最大外包半径、允许顶点在球内。本次落实为以实体COM为中心的最大顶点距离；V/k、支持域、种子域、64次预算、原验收容差保持。保留原计划文件，不覆盖用户修改。
- 旧采样器的两极/赤道参考拓扑导致双锥；球面分散点v2仍有球壳切块偏置，实际看图后未采用。v3使用全方向间隔采样、保凸顶点内缩和各向形变联合求r/V；实际网格已导出并查看。48个固定k30样本有33种度序列、无双锥，指标只防拓扑退化，不证明地质真实性。
- 先1100用户case通过；v3初次5000有23个高体积边界最终失败，保留原数据。加入顶点位移体积回退后重测5000全部成功，7个trial重试、12次拒绝、1次最终double fallback；覆盖审计通过。证据natural_v3_5000_fixed。
- 新形状暴露稳定吞吐样本325/850对水平面网格对角线敏感，两者10s超时。更换支撑面无效且已撤回，关闭滚阻也无效；禁用body-pair缓存仅修好一个，未保留。关闭Jolt接触流形归并后两者均2s静止。保留完整接触，未改网格、静止阈值、dt或子步；两种对角线纳入回归。旧滚阻36顶点故障固定保存，仍用旧接触设置覆盖。
- Release最终11/11通过（9.73s）。50次2.1214293s，预测1000次42.428586s；实际1000次43.3881825s，N=C1000、0失败/超时、1000000 Updates/4000000子步，CSV独立审计通过。旧v1与新v3实际CSV回放位置误差都为0；旧manifest缺失该设置时恢复旧manifold reduction=true。
- 当前继续Debug、实际GUI五模式、真实预览/后台性能及独立包验证；尚未进行本追加任务提交，不push。原v1证据保留，不将历史18.82s冒称当前吞吐。

- 追加任务验证完成：Debug11/11（71.62s），两配置16目标编译审计通过；GUI/headless五模式字段和11份CSV一致、误差0。实际新形状窗口及中文外包半径标签已查看，36投影/拾取与真实渲染姿态通过。
- 新版实际预览/后台各三轮性能均通过固定预算，帧P95最高16.5331/2.8846ms，scripted探针173/181、最大33.3084/3.1951ms；后台受控取消C2330/N10000、正式概率空、独立审计通过。原静态原型/原生交互未改路径沿用首版证据，不冒称本次重做。
- 中文空格独立包普通cmd部署首轮末段目录提交Windows5，正确io_error、.pending及无正式概率，来源未确定；同一exe/权限不变换新输出完整复测通过。失败目录和日志保留，未将首轮当通过。独立包11项self-test退出0，实际GUI截图已查看。
- 当前exe SHA256 1b641045b83e111af40c042ee433c23d2fcc93029529288663054aee484adfa7；源内容018e7c95102ca36569feb75bae252b8c2e66c237626b8a0f2065216cf580bb01独立重算一致。发行包RollingStone3D-natural沿用这个二进制，旧v1包保留。仅剩最终文档/文件清单审计与本追加任务的一次提交；不push。
- 最终暂存审计：23个本任务文件，逐项与已验证工作区除CRLF规范化外一致；原sln/计划SHA256保持起点值。自然版发行包155项哈希通过，12个exe与部署/自检候选包完全相同，启动帮助退出0。唯一追加提交保存此记录，提交号以Git历史及最终回复为准；没有push。


## 2026-11-12 追加：补齐弹性挡板

- 用户指出三维缺失二维弹性挡板并要求补齐。本次增加单轴质量—弹簧—阻尼支承的有限刚性板，M/k/c及初始q/v可编辑、保存/加载，动态接触实际推动板后退并回弹，显示/拾取取实际Jolt位姿；旧rigid/off及自然落石保持兼容。未扩展为柔性网或有限元。
- 新增每步板状态与每trial汇总CSV、支承力/子步平均接触力/储能/物理阻尼/独立数值功残差，完整板状态重放及支承静止判据。实际原生M0拒绝、M1500生效、中文路径保存加载、碰撞/关闭均验证；GUI五模式13CSV一致，旧v1/自然版/弹性新版实际回放误差0。
- 边界碰撞首先暴露软弹簧求解、热启动和接触入口问题；修复为每实际子步隐式支承等效接触质量，仅弹性场景80速度迭代/.05m预测距离/关闭热启动。27组M/k/c边界50m/s最大测得穿透.000231096m，门槛.002m；无测得机械能增长；解析振子和2%步长细化门槛通过。失败证据保留，未放宽阈值。
- Debug真实窗口额外发现raylib CUSTOMIZE_BUILD重复MAX_MESH_VERTEX_BUFFERS=9/7导致网格VBO数组越界；项目构建规范为唯一9，固定三库版本和编译器不变。两种实际窗口释放退出0，两模式17目标编译行审计通过。批刷新曾有400–600ms卡顿，三缓冲和捕获会话隔离过程及因果边界详记VALIDATION，无glFinish残留。
- 普通cmd中文空格部署首次遇目录提交Win32=5；失败现场保留，概率无效。真实目录/子文件锁分别复现32/5；最多4次提交、等待10/20/40ms，短锁恢复、持续拒绝仍明确io_error。最终同一应用exe普通cmd部署通过，12项独立包自检通过，原Debug12项后再复测受影响batch/session2/2；取消确认保持250ms预算内。
- 最终同一交付exe真实弹性预览/后台各3轮全部通过：帧P95最高2.9793/2.8981ms，180+180软件响应探针最大5.1308/3.265ms；取消输出独立审计通过。原7.5稳定夹具50次2.2573393s，1000次45.3877607s、N=C1000/无失败超时；不是1000次弹性碰撞吞吐。几何算法未改，完整5000沿用自然版合格证据。
- 最终exe SHA256 7b8555f2ea3e4223c95d838d312f680ee37eceb1b66dbb2f7a808f3a76c73d77，源码字节6936044c6d3199bcc35e789b08bd68fc89dcfd61475072270eb14d26968013cd独立重算。发行包RollingStone3D-elastic包含run_elastic_3d.cmd，保留旧包。唯一追加提交保存本记录，提交号取Git历史/最终回复；不push。用户两受保护文件hash与起点一致，二维源码未改；不纳入用户并行新增的后续障碍物/热图/时间轴规划。
