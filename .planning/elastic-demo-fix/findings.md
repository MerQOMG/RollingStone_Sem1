# Findings
- 实际HEAD c09cf45。组合演示的barrier只写enabled，缺mode；经主程序加载并导出后mode=rigid，失败已复现。
- BarrierSettings默认为rigid以兼容旧场景；独立parameterized_elastic.ini已正确显式mode=elastic。
- 原组合100次输出barrier_trials的elastic=0、位移为0；不需要推测求解器失效。
- 以真实配置加载确定根因，因此省略多假设猜测；仍验证真实命中位移、档案与GUI，排除未碰撞/显示不同步。
