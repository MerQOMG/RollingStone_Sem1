# 碎石分布与尺寸独立性修复
## Goal
修复自动分布过于局部、碎石随滚石尺寸联动，以及默认碎石过大过密的问题；保留同批固定、旧结果读取与真实物理碰撞。
## Next Step
交付 build/3d/package/RollingStone3D-rubble-fix-20261112；修复与证据由现有PR #8承载，合并由用户决定。
## Phases
- 复现与基线：complete
- 修复布局/尺寸默认值及兼容性：complete
- 回归、实际批次与 GUI 验证：complete
- 文档与程序包核验：complete
## Constraints
单 agent；沿用编译器/锁定依赖；保留三份原有脏文件；不自动合并 PR。
## Errors
- PowerShell 下 rg 的文件通配参数未展开；后续使用明确文件名。
- 加强到实际体积比较后发现位置重试重抽yaw；v2将yaw也固定到ID独立流，回归转绿。
