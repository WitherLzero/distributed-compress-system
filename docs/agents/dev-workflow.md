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
| 代码（W3 起 `src/`、`mock/`、`proto/`、`CMakeLists.txt` 等） | WSL（`~/projects/distributed-compress-system`） | DSH agent 经 `wsl.exe -d Ubuntu-22.04 -- bash -lc "..."` 读写、编译、测试 |
| 查看 WSL 侧代码（人） | — | VS Code **Remote - WSL** 插件（体验同 Remote-SSH）；或资源管理器 `\\wsl.localhost\Ubuntu\...` 偶尔浏览 |

## 3. git 纪律

- 同一时刻**不要两侧同时改同一批文件**；任一侧 push 前确认工作区干净，另一侧先 pull。
- **push 节奏**（2026-11-08 学员定）：功能分支在本地沉淀多个 commit，攒到「可开 PR / 可给人评审」的进度点才 push 一次；不是每建分支、每完成一个检查点就推远端。
- 代码开发走分支；W3 脚手架分支名为 `feat/w3-scaffold`（2026-11-06 grilling 共识），每周合并回 main；文档/issue 小改动可直接 main。
- 人机协作节奏（票→检查点→四拍循环、学习三层）见 `docs/agents/collaboration-protocol.md`。
- GitHub remote 已建成（2026-11-01）：`https://github.com/WitherLzero/distributed-compress-system`（private），`main` 已推送；两侧都以它为权威源，任一侧 push 后另一侧先 pull。

## 4. 环境备忘（WSL 侧）

- 发行版：**Ubuntu 22.04**（gcc 11 / cmake 3.22 / protobuf 3.12，BRPC 官方支持区间内）。
- agent 免密执行：`wsl.exe -d Ubuntu-22.04 -- bash -lc "<cmd>"`。
- WSL 侧 git 凭据复用 Windows 的 gh：credential helper 已配置为 `!/mnt/c/Users/ADMIN/tools/bin/gh.exe auth git-credential`（gh v2.103.3 绿色版在 `C:\Users\ADMIN\tools\bin`，已加入用户 PATH），WSL 内 clone/push 不再单独授权。
- `wsl.exe` 管理类输出（`--status`/`--list`）是 UTF-16LE，需 `iconv -f UTF-16LE -t UTF-8`；Linux 侧命令输出是 UTF-8，无此问题。

## 5. 已知坑速查

| 坑 | 规避 |
|---|---|
| `/mnt/e` 上 chmod/软链接/可执行位无效，git 权限噪音 | git 仓库不跨边操作（§1） |
| ext4 大小写敏感 vs NTFS 不敏感 | 代码文件命名不区分大小写（不依赖 `Foo.h`/`foo.h` 并存） |
| WSL 的 inotify 对 `/mnt/...` 文件不生效 | 编辑活动在哪侧就住哪侧（§2） |
| WSL 虚拟盘（`.vhdx`）只增不减 | 偶尔 `Optimize-VHD` 压缩 |
| agent 从 git-bash 调 `wsl.exe` 传 `/mnt/...` 路径会被 MSYS 转成 Windows 路径（脚本找不到） | 命令前加 `MSYS_NO_PATHCONV=1` |
| git-bash 里内嵌 PowerShell 命令时 `$变量` 被 bash 展开、反斜杠路径被吃 | 写成 `.ps1` 文件用 `-File` 调，路径用正斜杠 |

## 6. 工具速查

| 工具 | 位置 / 用法 |
|---|---|
| draw.io 桌面版（含导出 CLI） | `D:\drawio\draw.io\draw.io.exe`（自定义安装路径，不在 PATH）。PNG 导出：`draw.io.exe --disable-gpu -x -f png -e -b 10 -o out.png in.drawio`；Electron 单实例，连续导出需逐条串行执行；DSH 受限沙箱下 Electron IPC 会被拦（报 0x5），需 full-access 模式 |
| dsh-wsl 插件（WSL 侧原生读写与命令执行） | 已装于本机 profile，注册 `wsl` / `wsl-read` / `wsl-write` / `wsl-edit` / `wsl-grep` / `wsl-path` / `wsl-env` / `wsl-doctor` / `wsl-term` 十个工具。**WSL 侧一切文件读写一律走 `wsl-read`/`wsl-write`/`wsl-edit`**（发行版内部原子写，临时文件+rename 落地），构建与验收命令走 `wsl`（超时/后台 job/stdin 喂长脚本），长驻交互式工作（W5 服务联调）用 `wsl-term`。旧做法全部退役：edit/write 工具走 `\\wsl$` 会报 `GetFileSecurityW EIO`（DSH 沙箱 ACL 对 9p 共享的已知限制）；pwsh 套 `wsl.exe` 有引号层数与 PS 5.1 加 BOM/转 CRLF 的污染；temp+base64 通道仅在必须保二进制字节时兜底。注意该插件在 DSH 文件沙箱之外运行 |
