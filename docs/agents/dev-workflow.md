# 开发工作流约定：Windows + WSL2 双副本协作

> 本文件是**持续生效的工程约定**（人机共同遵守），不是一次性决策。
> 涉及的底层原理（9P 性能、文件系统差异、agent 操作 WSL 实测）见
> `docs/research/external-systems.md` §5；环境搭建实操见
> `docs/research/wsl-build-environment.md`（issue 04 交付物）。

## 1. 总模型

```
            GitHub（私有 remote，唯一权威）
              ↕  push / pull        ↕  push / pull
    E:\ 这份（Windows）         ~/ 这份（WSL2 ext4）
    文档 + issue + ADR           代码开发 + 编译 + 测试
    （agent 原生文件工具         （agent 经 wsl.exe 操作；
      直接协作的地方）            人用 VS Code Remote-WSL）
```

- **不存在自动同步**：Windows 与 WSL 文件系统互相可见（`\\wsl.localhost\...` ↔ `/mnt/...`）但那是协议挂载、不是副本；两边一致**只靠 git**。
- **各侧活跃文件放各侧文件系统**：跨边访问慢 5~10 倍（9P），且 NTFS 上 Linux 权限位/软链接/可执行位不保真。**Linux 的 git 不得在 `/mnt/...` 上操作仓库**。

## 2. 分工约定

| 内容 | 放哪侧 | 谁操作 |
|---|---|---|
| `docs/`（设计、调研、ADR）、`.scratch/`（issue/spec）、`GLOSSARY.md` | Windows（E 盘仓库） | DSH agent 原生工具直接读写 |
| 代码（W3 起 `src/`、`mock/`、`proto/`、`CMakeLists.txt` 等） | WSL（`~/projects/...`） | DSH agent 经 `wsl.exe -d Ubuntu-22.04 -- bash -lc "..."` 读写、编译、测试 |
| 查看 WSL 侧代码（人） | — | VS Code **Remote - WSL** 插件（体验同 Remote-SSH）；或资源管理器 `\\wsl.localhost\Ubuntu\...` 偶尔浏览 |

## 3. git 纪律

- 同一时刻**不要两侧同时改同一批文件**；任一侧 push 前确认工作区干净，另一侧先 pull。
- 代码开发走分支（W3 脚手架落地时定具体分支命名）；文档/issue 小改动可直接 main。
- 仓库尚未建 GitHub remote（截至 2026-11-01）；remote 建好后两边都以它为权威源。

## 4. 环境备忘（WSL 侧）

- 发行版：**Ubuntu 22.04**（gcc 11 / cmake 3.22 / protobuf 3.12，BRPC 官方支持区间内）。
- agent 免密执行：`wsl.exe -d Ubuntu-22.04 -- bash -lc "<cmd>"`。
- `wsl.exe` 管理类输出（`--status`/`--list`）是 UTF-16LE，需 `iconv -f UTF-16LE -t UTF-8`；Linux 侧命令输出是 UTF-8，无此问题。

## 5. 已知坑速查

| 坑 | 规避 |
|---|---|
| `/mnt/e` 上 chmod/软链接/可执行位无效，git 权限噪音 | git 仓库不跨边操作（§1） |
| ext4 大小写敏感 vs NTFS 不敏感 | 代码文件命名不区分大小写（不依赖 `Foo.h`/`foo.h` 并存） |
| WSL 的 inotify 对 `/mnt/...` 文件不生效 | 编辑活动在哪侧就住哪侧（§2） |
| WSL 虚拟盘（`.vhdx`）只增不减 | 偶尔 `Optimize-VHD` 压缩 |
