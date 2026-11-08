# 09: 工程卫生收口 + 干净 clone 复验

**Parent**: 03 — 工程脚手架与公共/基础类

**What to build**: W3 脚手架的最后收口：格式规范落地、全库警告清零、README 让导师/面试官 clone 后 10 分钟内完成"构建 → 跑起来 → 看懂结构"，并在 WSL 侧做一次干净 clone 的从零复验，最后按周节奏把功能分支合回主干。收口后 issue 03 的验收线全绿。

**Blocked by**: 05、06、07、08。

**Status**: ready-for-agent

- [ ] 格式化规范文件落地并全库执行一遍
- [ ] `-Wall -Wextra` 全库零警告（含三张前置工票的代码）
- [ ] README：环境依赖（指向环境搭建文档）、一键构建命令、三二进制与动态库的产物说明、目录结构导读
- [ ] WSL 侧干净 clone 从零构建 + 全部测试通过，结果贴回 issue 03
- [ ] 功能分支合并回主干，提交信息遵循 Conventional Commits
- [ ] issue 03 Acceptance 六条全绿并关闭，父 issue 状态由学员终审后更新
