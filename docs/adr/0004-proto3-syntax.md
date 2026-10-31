# ADR-0004: 接口描述采用 Protobuf proto3 语法

- **Status**: Accepted（2026-10-31 学员定稿）
- **Date**: 2026-10-31
- **Deciders**: 林昕祺（学员）

## Context

内部 RPC 接口（§6.1）以 Protobuf 描述。brpc 官方示例多用 proto2 语法（`required`/`optional` 标签），初稿文档因此沿用 proto2。但 Google Style Guide 已弃用 `required`：一旦消息带上 required 字段，旧数据缺失该字段即解析失败，字段永远无法安全演进——Google 自己承认这是设计错误，proto3 将其废除。切换时点在编码开始前（W3 前），此时迁移成本为零（仅改文档文本）；W3 编码后再迁移则需动桩代码与实现并回归。

## Decision

全部接口描述采用 **proto3** 语法：

1. 消息字段不再标注 `required`/`optional`；
2. proto3 无必填校验机制，关键必填字段（`header`、`server_id`、`data_id` 等）由服务端入口自查，缺失即拒绝请求；
3. 默认值语义随 proto3 统一为零值，不使用自定义默认值。

## Rationale

- proto3 是 Google 现行推荐语法与长期演进方向，brpc 完全支持；
- 本系统消息字段均为简单标量（无 required 依赖、无自定义默认值需求），迁移无语义损失；
- proto2 无任何 proto3 不具备的、本项目需要的能力，沿用无收益。

## Consequences

**正面**

- 与业界现状及 Style Guide 一致，后续接入新工具链/代码生成无历史包袱；
- 决策时点在编码前，零迁移成本。

**负面 / 代价**

- 失去编译期必填检查，必填约束改为运行时自查（服务端入口需显式校验）；
- proto3 的字段存在性语义（presence）与 proto2 不同，未来若需区分"未设置"与"零值"需使用 `optional` 单数字段语法（proto3 支持 message 字段天然 presence，标量需显式 `optional`）。

## 关联

- 接口定义：设计文档 §6.1
- 不依赖 ADR-0003（C ABI 接口决策），二者互不影响。
