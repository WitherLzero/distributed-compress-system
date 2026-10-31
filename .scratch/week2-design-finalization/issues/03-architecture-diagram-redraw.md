# 03 — 架构图重绘

- **Status**: completed
- **依据**: W1 issues/04 顺延（第 2 课确认：完稿后随设计文档整体提交，不单独回传确认——#20 已删除）。

## 内容

1. 按 v0.1→定稿后的设计文档重绘 `docs/compress-system.drawio`：规范化布局，三方组件 + master/server/client 交互箭头（get_workers_info / heartbeat / compress attachment）。
2. 调度器按外部环境标注（Shell 脚本模拟，图中不作为系统组件）。
3. 新增流程图：压缩主流程时序（§4.2）、读取解压流程（§4.3）——是否独立出图待定稿时视版面决定。

## Acceptance

- 图与定稿文档的模块划分、接口命名完全一致。
- 随文档定稿整体发群。

## Comments

- 2026-10-31：全部图定稿并 commit `26b5acf`——架构图（docs/diagrams/compress-system.drawio，与定稿口径一致：random 选点/5s·20s 判死/调度器外部环境标注）、§4.2 压缩主流程泳道图、§4.4 失效判定流程图（4.5 负载反馈按定稿决议不加小节，图只保留 4.4）。正文 §1.3/4.2/4.4 已插图（图 1/2/3）；原 ASCII 时序图删除。§4.3 读取解压为纯本地 4 步，表格足够，不单独出图。统一收口 docs/diagrams/。
