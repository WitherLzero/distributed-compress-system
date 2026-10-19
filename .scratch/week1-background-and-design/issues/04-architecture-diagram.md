# 04 — 架构图重绘

- **Status**: pending
- **Type**: task

## 背景

导师原图（`docs/compress-system.drawio`）是投屏手绘的，导师自己说"画得太丑了"、"这个原版图我可以发你"、"你可能得改改"。重绘是 W1 交付物之一。

## 要求

- 用 **draw.io** 重画，替换/新增文件于 `docs/`。
- 保持**信息不丢失**：原图已有元素与文字标注都要保留。
- 规范布局：分层清晰（存储集群 / master / 离线集群）、箭头方向明确、标注贴边。
- 建议补充原图缺失的部分（箭头或注记）：
  - `scheduler` 与 master/server 的关系（原图未画）
  - 判死/反压/重试的路径（可用虚线或注记）
  - 控制面 vs 数据面的视觉区分（颜色或线型）

## 原图元素清单（重绘时的对照）

**图形**
- `存储集群（HDFS）` 分组框 + `datanode` ×2 + 各内含 `compress_client`
- `compress_master`（在两个分组框之外/上方）
- `离线系统(MR、spark)` 分组框 + `compress_server` ×8

**箭头（含标签）**
- ① `compress_server` → `compress_master`：`1. 上报负载、IP`
- ② `compress_client` → `compress_master`：`2. get_workers_info`
- ③ `compress_client` → `compress_server`：`3. send data`
- ④ `compress_server` → `compress_client`：`4. response data`

**文字标注**
- client 侧：`1. 发送数据、接收压缩后的数据` / `2. server繁忙退避` / `3. orgin len/compress len`
- master 侧：`1. servers信息维护（IP， cpu\mem\队列长度）` / `2. servers心跳超时判死`
- server 侧：`1. 心跳上报` / `2. 物理信息采集` / `3. 队列长度采集` / `4. 采集信息上报`

> ⚠️ 原图拼写：`orgin len`（应为 `origin len`）、`repsonse data`（应为 `response data`）——重绘时修正。

## Acceptance

- 图能独立讲清系统：不看文字也能理解三组件关系与控制面/数据面。
- 与 `docs/research/external-systems.md` 的时序描述一致。

## Comments

- 2026-10-19：创建。原图已在 `docs/` 下并被 git 追踪。
