# Findings
- 当前 HEAD 1b5404e，分支 codex/rollingstone-3d；PR #8 已开放。
- 三份原有脏文件仍为 RollingStone.sln、IMPLEMENTATION_PLAN.md、OBSTACLES_HEATMAP_TIMELINE_PLAN.md。
- 布局的实际入口为 freezeConfig -> freezeObstacles；正式 obstacle_tests 可测试这个入口。
- 现有设置含 relative_to_r / absolute_m 和 automatic_region；兼容旧配置与已存实际障碍几何需要随修复核查。
- 真实 freezeConfig + SceneEditor 失败回归：上游挡板 x=25 时碎石 x=6.4277..22.9693，地形末端99.0666；默认40块、最大长1.5936m/露出高0.7971m；编辑 r/V 改变碎石尺寸。
- 原100次演示（固定种子）95次碰碎石、17次最终 COM 在坡脚 x>=79.0666；全部完成，无失败/超时。最终位置不等同曾经过坡脚，报告须说明统计口径。
- 修复采用 v2 全坡面分层抽样、逐ID独立尺寸流，默认24块、米制长宽.20–.50m/露出高.06–.18m/间距.75m。v1算法、随机流及旧配置hash需保持可读/可重积分。
