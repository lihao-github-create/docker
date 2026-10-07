# 第零部分：安装 Docker 环境

本节对应 [学习指南](../learn_guide.md) 的准备阶段，目标是安装 Docker Engine、Docker CLI、Buildx 和 Docker Compose，并通过实际运行容器确认环境可用。

本文提供三条安装路线：

| 操作系统 | 推荐方案 | 适用场景 |
|---|---|---|
| Ubuntu | Docker 官方 APT 仓库中的 Docker Engine | Linux 服务器、Linux 开发机 |
| Windows | Docker Desktop + WSL 2 | Windows 本地开发 |
| macOS | Docker Desktop | macOS 本地开发 |

不要混用多条路线。例如，在 Windows 上使用 Docker Desktop 的 WSL 2 后端时，不要再在同一个 WSL 发行版中安装一套 `docker.io` 或 Docker Engine，否则可能出现命令、上下文和守护进程相互冲突的问题。

> 本文命令以 Bash 为例。安装软件需要管理员权限和可访问 Docker 官方软件源的网络。生产服务器安装前还应确认组织的版本、代理、防火墙和安全策略。

## 1. 安装前先理解组件

完成安装后会用到以下组件：

| 组件 | 作用 | 常用检查命令 |
|---|---|---|
| Docker Engine | 负责创建和运行容器，其中 `dockerd` 是后台守护进程 | `docker info` |
| Docker CLI | 向 Docker Engine 发送命令 | `docker version` |
| containerd | 管理容器生命周期和镜像 | 通常不直接操作 |
| Buildx | 提供扩展的镜像构建能力 | `docker buildx version` |
| Compose | 用 YAML 文件管理一组容器 | `docker compose version` |

注意 Compose v2 的命令是：

```bash
docker compose
```

不是旧版独立程序使用的 `docker-compose`。

## 2. Ubuntu：安装 Docker Engine

以下流程使用 [Docker 官方 Ubuntu 安装文档](https://docs.docker.com/engine/install/ubuntu/) 推荐的 APT 仓库，而不是 Ubuntu 自带的 `docker.io` 软件包。其他 Linux 发行版应在 [Docker Engine 安装入口](https://docs.docker.com/engine/install/) 中选择对应系统，不要直接照抄 Ubuntu 的软件源地址。

### 2.1 检查系统信息

执行：

```bash
cat /etc/os-release
dpkg --print-architecture
uname -r
```

确认当前系统是 Docker 官方支持的 64 位 Ubuntu 版本，CPU 架构也在支持范围内。Ubuntu 的衍生发行版可能可以使用同一套步骤，但不一定属于 Docker 官方支持范围。

### 2.2 移除冲突软件包

如果系统从未安装过 Docker，可以执行下面的命令；APT 提示软件包未安装是正常现象：

```bash
for package in docker.io docker-compose docker-compose-v2 docker-doc docker-buildx podman-docker containerd runc; do
  sudo apt remove -y "$package"
done
```

这一步只移除可能与官方 Docker Engine 冲突的软件包，不会自动删除已有的镜像、容器、数据卷和网络。已有 Docker 数据的机器应先备份重要数据，不要手动删除 `/var/lib/docker` 或 `/var/lib/containerd`。

### 2.3 添加 Docker 官方 APT 仓库

安装下载和证书工具：

```bash
sudo apt update
sudo apt install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
```

下载 Docker 官方签名密钥，并保证 APT 可以读取：

```bash
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
  -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
```

添加 DEB822 格式的软件源配置：

```bash
sudo tee /etc/apt/sources.list.d/docker.sources > /dev/null <<EOF
Types: deb
URIs: https://download.docker.com/linux/ubuntu
Suites: $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}")
Components: stable
Architectures: $(dpkg --print-architecture)
Signed-By: /etc/apt/keyrings/docker.asc
EOF
```

刷新软件包索引，并确认能看到来自 Docker 官方仓库的软件包：

```bash
sudo apt update
apt-cache policy docker-ce
```

`docker-ce` 的候选版本不应显示为 `(none)`。如果这里失败，应先解决网络、代理、系统版本代号或签名密钥问题，再继续安装。

### 2.4 安装 Docker 和 Compose

安装最新版稳定软件包：

```bash
sudo apt install -y \
  docker-ce \
  docker-ce-cli \
  containerd.io \
  docker-buildx-plugin \
  docker-compose-plugin
```

Ubuntu 和 Debian 系统通常会在安装后自动启动 Docker。检查服务状态：

```bash
sudo systemctl status docker --no-pager
sudo systemctl is-enabled docker
```

如果服务没有运行，执行：

```bash
sudo systemctl enable --now docker
```

先使用管理员权限运行官方测试镜像：

```bash
sudo docker run --rm hello-world
```

这个命令会下载 `hello-world` 镜像、创建一个容器、输出验证信息，然后因为使用了 `--rm` 而自动删除该容器。镜像会保留在本机缓存中。

### 2.5 允许当前用户执行 Docker 命令

默认情况下，普通用户不能访问 Docker 的 Unix socket。将当前用户加入 `docker` 组：

```bash
sudo usermod -aG docker "$USER"
```

然后注销当前系统会话并重新登录。只想临时刷新当前终端的组身份，也可以执行：

```bash
newgrp docker
```

再次验证，此时命令不应需要 `sudo`：

```bash
docker run --rm hello-world
```

> **安全提醒：** `docker` 组成员可以控制 Docker 守护进程，并能通过容器获得宿主机上的高权限，实际效果接近授予 root 权限。多人共用或安全要求较高的机器应评估 [Rootless 模式](https://docs.docker.com/engine/security/rootless/)，不要随意把用户加入该组。Docker 官方的安装后配置说明也明确提示了这项风险。

如果此前用 `sudo docker` 生成了当前用户无法访问的配置文件，并出现 `~/.docker/config.json: permission denied`，可修复目录所有者：

```bash
sudo chown -R "$USER":"$USER" "$HOME/.docker"
sudo chmod -R g+rwx "$HOME/.docker"
```

## 3. Windows：安装 Docker Desktop 和 WSL 2

Windows 本地开发推荐使用 Docker Desktop 的 WSL 2 后端。Docker Desktop 会管理 Linux 虚拟化环境和 Docker Engine，WSL 发行版中只需要使用它提供的 Docker CLI 集成。

### 3.1 安装或更新 WSL

以管理员身份打开 PowerShell，执行：

```powershell
wsl --install
```

根据系统提示重启 Windows。重启后更新并检查 WSL：

```powershell
wsl --update
wsl --status
wsl --version
```

如果已经安装 Linux 发行版，可确认它使用 WSL 2：

```powershell
wsl --list --verbose
```

若 `VERSION` 显示为 `1`，将发行版转换为 WSL 2，其中 `<发行版名称>` 替换为上一步显示的名称：

```powershell
wsl --set-version <发行版名称> 2
```

WSL 的安装与系统要求以 [Microsoft 的 WSL 安装文档](https://learn.microsoft.com/windows/wsl/install) 为准。

### 3.2 安装并配置 Docker Desktop

1. 从 [Docker Desktop for Windows 官方页面](https://docs.docker.com/desktop/setup/install/windows-install/) 下载安装程序。
2. 运行安装程序并选择 WSL 2 后端。
3. 启动 Docker Desktop，等待界面显示 Docker Engine 正在运行。
4. 打开 **Settings > General**，确认启用了 **Use WSL 2 based engine**。如果当前系统只支持该后端，这个选项可能不会显示。
5. 打开 **Settings > Resources > WSL Integration**，为需要使用 Docker 的 WSL 发行版启用集成，然后应用设置。

进入对应的 WSL 终端验证：

```bash
docker version
docker compose version
docker run --rm hello-world
```

如果 `docker version` 只能显示 Client，不能显示 Server，先确认 Docker Desktop 已启动并完成 WSL Integration 配置。

为了获得更好的文件访问性能，Linux 工具链项目通常放在 WSL 的 Linux 文件系统中，例如 `~/projects/demo`，而不是 `/mnt/c/...`。可以在 WSL 终端中运行 `explorer.exe .`，从 Windows 文件管理器打开当前目录。详细建议见 [Docker Desktop 的 WSL 2 最佳实践](https://docs.docker.com/desktop/features/wsl/best-practices/)。

> Docker Desktop 的个人使用、教育、小型企业和大型组织可能适用不同的许可条件。公司环境部署前应阅读安装页面上的 Docker Desktop 许可说明。

## 4. macOS：安装 Docker Desktop

macOS 不能直接运行 Linux 容器内核，Docker Desktop 会创建并管理一个轻量级 Linux 虚拟机。

先确认 Mac 的处理器架构：

```bash
uname -m
```

- 输出 `arm64`：选择 Apple silicon 版本。
- 输出 `x86_64`：选择 Intel 版本。

安装步骤：

1. 从 [Docker Desktop for Mac 官方页面](https://docs.docker.com/desktop/setup/install/mac-install/) 下载与处理器匹配的 `Docker.dmg`。
2. 打开镜像文件，将 Docker 图标拖入 `Applications`。
3. 从“应用程序”启动 Docker，并按提示完成首次设置。
4. 等待菜单栏中的 Docker 状态显示 Engine 正在运行。

打开终端验证：

```bash
docker version
docker compose version
docker run --rm hello-world
```

如果终端提示找不到 `docker`，重新打开终端，并在 Docker Desktop 的 **Settings > Advanced** 中检查 CLI 工具安装位置。

## 5. 所有平台的完整验证

安装成功不只意味着 `docker --version` 能输出版本号，还要确认客户端能连接守护进程、可以拉取镜像、端口映射正常，并且 Compose 插件可用。

### 5.1 检查各组件

```bash
docker version
docker info
docker buildx version
docker compose version
```

`docker version` 应同时包含 `Client` 和 `Server`。如果只有 Client，说明只安装了命令行工具，或者 Docker Engine 尚未启动、当前 Docker context 指向了不可用的环境。

### 5.2 运行一个 Web 容器

运行 Nginx，并只将端口发布到本机回环地址：

```bash
docker run -d --rm \
  --name docker-install-check \
  -p 127.0.0.1:8080:80 \
  nginx:alpine
```

查看状态和日志：

```bash
docker ps
docker logs docker-install-check
```

用浏览器访问 `http://localhost:8080`，或者执行：

```bash
curl http://localhost:8080
```

看到 Nginx 欢迎页面的 HTML 后，停止容器：

```bash
docker stop docker-install-check
```

因为启动时指定了 `--rm`，容器停止后会被自动删除。可以确认：

```bash
docker ps -a
```

### 5.3 验证 Compose

在本仓库根目录执行：

```bash
cd phase6-compose
docker compose config
```

如果命令能输出规范化后的 Compose 配置且没有报错，说明 Compose 插件已经可用。这里的 `config` 只解析和校验配置，不会启动服务。

## 6. 常见问题排查

### 6.1 `permission denied while trying to connect to the Docker daemon socket`

在原生 Linux 上依次检查：

```bash
id
ls -l /var/run/docker.sock
```

如果当前用户不在 `docker` 组，按 2.5 节添加用户，然后重新登录。不要通过 `chmod 666 /var/run/docker.sock` 绕过权限控制。

### 6.2 `Cannot connect to the Docker daemon`

原生 Linux 检查服务：

```bash
sudo systemctl status docker --no-pager
sudo journalctl -u docker --no-pager -n 100
```

Windows 或 macOS 检查 Docker Desktop 是否已经启动。所有平台还可检查当前上下文：

```bash
docker context ls
docker context show
```

不要在不清楚来源的情况下设置 `DOCKER_HOST`。错误的环境变量会让 CLI 连接到不存在或错误的 Docker Engine。

### 6.3 拉取镜像超时

先区分 DNS、HTTPS 和 Docker Hub 访问问题：

```bash
curl -I https://download.docker.com
curl -I https://registry-1.docker.io/v2/
```

第二条命令返回 `401 Unauthorized` 通常反而说明网络和 Registry 服务可达，因为匿名请求没有携带认证信息。

在公司网络中，应按组织要求为 Docker 守护进程或 Docker Desktop 配置 HTTP/HTTPS 代理。不要随意复制来源不明、可能失效或会记录凭据的公共镜像加速地址。确需 Registry mirror 时，应使用自己组织或云服务账号提供的可信地址，并参考 [Docker daemon 代理配置](https://docs.docker.com/engine/daemon/proxy/) 和 [Registry mirror 配置](https://docs.docker.com/docker-hub/image-library/mirror/)。

### 6.4 端口无法访问或被占用

先检查容器是否运行、端口是否发布：

```bash
docker ps
docker port docker-install-check
docker logs docker-install-check
```

如果提示 `port is already allocated`，说明宿主机的 `8080` 已被占用，可将示例改成 `-p 127.0.0.1:8081:80`。

在 Linux 上，Docker 发布的容器端口可能绕过 UFW 或 firewalld 的常规规则。服务器对外发布端口前，应阅读 [Docker 与防火墙](https://docs.docker.com/engine/network/packet-filtering-firewalls/) 的说明，并通过 `DOCKER-USER` 链实施所需访问控制。

### 6.5 镜像架构不匹配

如果出现 `exec format error`，比较主机和镜像架构：

```bash
uname -m
docker image inspect nginx:alpine --format '{{.Architecture}}/{{.Os}}'
```

Apple silicon 或 ARM Linux 应优先使用提供 `arm64` 版本的多架构镜像。只有确有需要时才通过 `--platform` 使用模拟运行其他架构，模拟运行通常更慢。

## 7. 升级建议

Ubuntu 使用官方 APT 仓库安装后，可通过系统软件包管理器升级：

```bash
sudo apt update
sudo apt install --only-upgrade \
  docker-ce \
  docker-ce-cli \
  containerd.io \
  docker-buildx-plugin \
  docker-compose-plugin
```

生产环境不要在未验证的情况下自动跨大版本升级。应先阅读 Docker Engine 发布说明，在测试环境验证现有镜像、Compose 配置、网络和存储，再安排升级与回滚方案。

Docker Desktop 可通过应用内更新功能升级。升级前应阅读版本说明，并确认操作系统仍在支持范围内。

## 8. 安装完成检查表

- `docker version` 同时显示 Client 和 Server。
- `docker compose version` 能显示 Compose v2 版本。
- `docker run --rm hello-world` 成功输出欢迎信息。
- Nginx 容器能通过 `http://localhost:8080` 访问。
- Linux 普通用户可以在不使用 `sudo` 的情况下执行 Docker 命令，且已理解 `docker` 组的权限风险。
- `docker compose config` 能成功校验本仓库的 Compose 示例。

全部通过后，继续学习 [Docker 的核心认知](01-docker-core-concepts.md)，再进入容器生命周期、端口、存储、Dockerfile 和 Compose 实验。

## 参考资料

- [Docker Engine 官方安装入口](https://docs.docker.com/engine/install/)
- [Ubuntu 安装 Docker Engine](https://docs.docker.com/engine/install/ubuntu/)
- [Linux 安装后配置](https://docs.docker.com/engine/install/linux-postinstall/)
- [Windows 安装 Docker Desktop](https://docs.docker.com/desktop/setup/install/windows-install/)
- [Docker Desktop 的 WSL 2 后端](https://docs.docker.com/desktop/features/wsl/)
- [macOS 安装 Docker Desktop](https://docs.docker.com/desktop/setup/install/mac-install/)
