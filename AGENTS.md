# Repository Guidelines

## 项目结构与模块组织

本仓库包含 Docker 学习笔记和独立实验。

- `learn_guide.md`：学习路线及详细笔记链接。
- `doc/`：中文概念说明、命令、实际结果和排障记录。新增章节总结采用编号命名，例如 `11-ros2-docker.md`。
- `phase5-dockerfile/`：独立的 C++ 问候程序及其 Dockerfile。
- `phase6-compose/`、`phase7-network/`、`phase8-host/`：分别对应 Compose、桥接网络隔离和 host 网络实验。
- `phase9-cpp-dev/`：C++17/CMake 项目，包含 `src/` 源码、开发镜像和 Compose 配置。
- `phase10-ros2/`：ROS2 talker/listener 服务。
- `nginx-html/`：挂载到 Nginx 的静态资源。

各实验应保持独立；仓库没有统一的构建系统或测试目录。

## 构建、测试与开发命令

在仓库根目录构建并运行问候程序：

```bash
docker build -t cpp-hello:guide phase5-dockerfile
docker run --rm cpp-hello:guide LiHao
```

使用容器开发 C++ 项目：

```bash
cd phase9-cpp-dev
printf 'DEV_UID=%s\nDEV_GID=%s\n' "$(id -u)" "$(id -g)" > .env
docker compose build dev
docker compose run --rm dev
```

这些命令先构建工具链镜像，再配置、编译并运行挂载的项目。生成 `.env` 时使用本机当前用户的 UID/GID。

在相应 Compose 实验目录中，使用 `docker compose config` 检查配置，使用 `docker compose up -d` 启动服务，通过 `docker compose ps -a` 和 `docker compose logs` 查看结果。ROS2 实验要求已有 `ros2_lab` 网络；若不存在，执行 `docker network create ros2_lab` 创建。

## 代码风格与命名约定

C++ 使用四空格缩进，YAML 使用两空格缩进；Dockerfile 指令使用大写，启动命令采用 exec 形式。实验目录沿用 `phaseN-topic/` 命名。学习笔记使用中文，代码块标明语言，新增总结需在 `learn_guide.md` 中添加引用。仓库尚未配置格式化或静态检查工具。

## 测试指南

仓库没有自动化测试框架或覆盖率要求。修改实验后，进行相关的基本功能验证，例如检查 C++ 输出、Redis `PING` 或 ROS2 listener 收到的消息。修改文档时，检查相对链接、代码块以及示例与实际文件的一致性，并明确区分预期输出和实际结果。提交前执行 `git diff --check`。

## 提交与 Pull Request 指南

现有提交使用 `docs:` 前缀和简洁的中文说明，例如 `docs: 补充 Docker 网络概念与隔离实验记录`。每次提交应围绕明确的主题。PR 应说明涉及的实验、修改内容及验证命令和结果；存在相关 issue 时附上链接。

## 配置说明

按照 `.gitignore` 的配置，不跟踪 `.env` 和生成的 `build/` 内容。host 网络实验应在文档说明的 Linux/WSL 环境中运行。执行 `docker compose down` 后，外部网络仍会保留；示例和清理说明需明确这一行为。
