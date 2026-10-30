# 01 — 外部系统认知调研

- **Status**: completed
- **Type**: research

## 问题

本系统依赖的运行时外部环境——**存储集群**、**离线计算集群（MR/Spark）**、**调度器**——分别是什么？与我们系统的对接形态是什么？本地开发时如何复现？

## 产出

| 文件 | 内容 |
|---|---|
| `docs/research/external-systems.md` | 详细版：四名词定性、三种对接形态、C++ lib 无法嵌入 Java datanode 的推论、生产/本地部署形态、控制面数据面分离、完整时序、注册与心跳、master 行为特征、选 server 的决策位置 |
| `GLOSSARY.md` | 精炼版：系统边界、三组件、外部系统表、控制面/数据面示意、术语表、关键约束 |
| `docs/adr/0001-local-mock-external-clusters.md` | 决策：本地以 mock 替代真实外部集群（Status: Proposed） |

## 关键结论

1. **三种对接形态完全不同**：存储集群走 **API 对接**；MR/Spark **无 API 对接**（只是"集群忙时的来源"，我们是邻居）；调度器走**管理接口**。
2. **C++ 动态库无法嵌入 Java datanode** → 本地必须自造 mock；也据此**推断**原系统 datanode 是 C++ 自研（待确认）。
3. **控制面/数据面分离**是理解全系统的总钥匙——master 永不碰真实数据，大流量走 client↔server 直连。
4. **master 是 reactive 的**（图无出向箭头），但拥有决策权；判死依据是"最后一次心跳距今多久"。

## 遗留

- 上述第 2 条的**推断**需向导师确认 → 见 `05-mentor-clarifications.md` #19。
- xtime 来源、跨集群网络、scheduler 与 master 的关系等 → 见 `05`。

## Comments

- 2026-10-19：完成。方法：逐元素解析 `docs/compress-system.drawio` 的 XML（取 source/target 精确方向），结合第 1 课会议记录；并用 web 检索核实 libhdfs 的 JNI 性质与 YARN REST API 的存在。
