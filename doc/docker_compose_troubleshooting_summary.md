# Docker Compose 安装与 Docker Hub 网络问题排障总结

## 1. 背景

在 Ubuntu 22.04 / WSL2 环境中学习 Docker Compose 时，先后遇到了以下问题：

1. `docker compose` 命令不存在
2. Docker CLI 已安装，但 Docker daemon 未启动
3. `docker compose up -d` 拉取镜像失败
4. `curl` 能访问 Docker Hub，但 `docker pull` 超时
5. 最终通过给 Docker daemon 配置代理解决

本文记录完整排查过程，方便以后复用。

---

## 2. 问题一：`docker compose` 命令不存在

执行：

```bash
docker compose version
```

报错：

```text
docker: unknown command: docker compose
```

### 2.1 检查 Docker 来源

执行：

```bash
docker --version
which docker
apt policy docker-ce docker.io docker-compose-plugin
```

当时输出显示：

```text
Docker version 29.1.3, build 29.1.3-0ubuntu3~22.04.2
```

并且：

```text
docker.io:
  Installed: 29.1.3-0ubuntu3~22.04.2
```

同时：

```text
docker-ce:
  Installed: (none)
```

以及：

```text
N: Unable to locate package docker-compose-plugin
```

### 2.2 原因

当前安装的是 Ubuntu 仓库中的：

```text
docker.io
```

而不是 Docker 官方仓库中的：

```text
docker-ce
docker-ce-cli
docker-compose-plugin
docker-buildx-plugin
```

Docker Engine 和 Docker Compose 是不同组件。

即使：

```bash
docker --version
```

能够正常工作，也不代表：

```bash
docker compose
```

一定存在。

---

## 3. 解决：切换到 Docker 官方仓库

### 3.1 卸载 Ubuntu 自带 Docker

```bash
sudo apt remove docker.io
```

> 一般不会自动删除 `/var/lib/docker` 中已有的镜像、容器和 Volume，但在重要环境中仍建议提前备份。

---

### 3.2 安装基础工具

```bash
sudo apt update
sudo apt install -y ca-certificates curl
```

---

### 3.3 添加 Docker 官方 GPG Key

```bash
sudo install -m 0755 -d /etc/apt/keyrings

sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
  -o /etc/apt/keyrings/docker.asc

sudo chmod a+r /etc/apt/keyrings/docker.asc
```

---

### 3.4 添加 Docker 官方软件源

```bash
sudo tee /etc/apt/sources.list.d/docker.sources <<EOF
Types: deb
URIs: https://download.docker.com/linux/ubuntu
Suites: $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}")
Components: stable
Architectures: $(dpkg --print-architecture)
Signed-By: /etc/apt/keyrings/docker.asc
EOF
```

然后：

```bash
sudo apt update
```

检查：

```bash
apt policy docker-ce docker-compose-plugin
```

此时应该能够看到可安装版本，而不是：

```text
Candidate: (none)
```

---

### 3.5 安装 Docker CE、Buildx 和 Compose

```bash
sudo apt install -y \
  docker-ce \
  docker-ce-cli \
  containerd.io \
  docker-buildx-plugin \
  docker-compose-plugin
```

验证：

```bash
docker --version
docker compose version
docker buildx version
```

---

## 4. 问题二：Docker daemon 未启动

安装完成后执行：

```bash
docker version
```

出现：

```text
Client: Docker Engine - Community
...
Cannot connect to the Docker daemon at unix:///var/run/docker.sock.
Is the docker daemon running?
```

这说明 Docker CLI 已正常安装，但后台的 Docker daemon 没有运行。

### 4.1 启动 Docker 服务

```bash
sudo systemctl start docker
```

检查状态：

```bash
sudo systemctl status docker
```

设置为开机自动启动：

```bash
sudo systemctl enable docker
```

再次验证：

```bash
docker version
```

正常情况下应该同时看到：

```text
Client:
...

Server:
...
```

---

## 5. WSL2 下检查 systemd

如果使用 WSL2，可以先检查 PID 1：

```bash
ps -p 1 -o comm=
```

如果输出：

```text
systemd
```

说明可以直接使用：

```bash
sudo systemctl start docker
```

如果不是 `systemd`，可编辑：

```bash
sudo nano /etc/wsl.conf
```

加入：

```ini
[boot]
systemd=true
```

然后在 Windows PowerShell 中执行：

```powershell
wsl --shutdown
```

重新进入 WSL 后再执行：

```bash
sudo systemctl start docker
sudo systemctl enable docker
```

---

## 6. 问题三：Compose 拉取镜像超时

执行：

```bash
docker compose up -d
```

出现：

```text
Error Get "https://registry-1.docker.io/v2/":
net/http: request canceled while waiting for connection
(Client.Timeout exceeded while awaiting headers)
```

并且后续出现：

```text
Error response from daemon: No such image: nginx:stable
```

### 6.1 原因判断

`No such image` 只是后续连锁错误。

真正的问题是：

```text
Docker daemon 无法访问 Docker Hub
```

---

## 7. 单独测试镜像拉取

执行：

```bash
docker pull redis:7
```

仍然报：

```text
Client.Timeout exceeded while awaiting headers
```

说明不是 Compose 配置问题，而是 Docker daemon 的网络问题。

---

## 8. 用 curl 检查 Docker Hub

执行：

```bash
curl -I https://registry-1.docker.io/v2/
```

结果：

```text
HTTP/1.1 200 Connection established

HTTP/2 401
...
www-authenticate: Bearer realm="https://auth.docker.io/token"
```

### 8.1 如何理解 `401`

这里的：

```text
HTTP/2 401
```

是正常现象。

Docker Registry 的 `/v2/` 接口需要认证，因此返回 401 说明：

```text
网络已经成功连接到 Docker Hub
```

更关键的是：

```text
HTTP/1.1 200 Connection established
```

这通常说明 `curl` 正在通过 HTTP/HTTPS 代理访问外网。

---

## 9. 检查 Shell 代理

执行：

```bash
env | grep -i proxy
```

实际输出中存在：

```text
https_proxy=http://127.0.0.1:7890
HTTPS_PROXY=http://127.0.0.1:7890
HTTP_PROXY=http://127.0.0.1:7890
http_proxy=http://127.0.0.1:7890
```

说明当前 Shell 已配置代理：

```text
127.0.0.1:7890
```

因此：

```text
curl
  ↓
Shell HTTP_PROXY / HTTPS_PROXY
  ↓
127.0.0.1:7890
  ↓
Docker Hub
  ↓
成功
```

---

## 10. 检查 Docker daemon 是否有代理

执行：

```bash
sudo systemctl show docker --property=Environment
```

得到：

```text
Environment=
```

这说明：

```text
Shell 有代理
Docker daemon 没有代理
```

因此：

```text
curl 可以访问 Docker Hub
docker pull 却访问超时
```

---

## 11. Shell 代理与 Docker daemon 代理的区别

Docker 的网络访问可以分成多个层次：

```text
Shell / curl
  │
  └── HTTP_PROXY / HTTPS_PROXY

Docker CLI
  │
  └── 向 dockerd 发请求

Docker daemon (dockerd)
  │
  └── 真正负责 docker pull
```

因此：

```bash
export HTTP_PROXY=...
export HTTPS_PROXY=...
```

只保证 Shell 中的程序能使用代理。

它不会自动让 `dockerd` 使用相同代理。

---

## 12. 解决：给 Docker daemon 配置代理

创建 systemd override 目录：

```bash
sudo mkdir -p /etc/systemd/system/docker.service.d
```

创建配置文件：

```bash
sudo nano /etc/systemd/system/docker.service.d/http-proxy.conf
```

写入：

```ini
[Service]
Environment="HTTP_PROXY=http://127.0.0.1:7890"
Environment="HTTPS_PROXY=http://127.0.0.1:7890"
Environment="NO_PROXY=localhost,127.0.0.1,172.16.0.0/12,192.168.0.0/16,10.0.0.0/8"
```

---

## 13. 重新加载 systemd 配置

执行：

```bash
sudo systemctl daemon-reload
sudo systemctl restart docker
```

检查：

```bash
sudo systemctl show docker --property=Environment
```

正常情况下应该能看到类似：

```text
Environment=HTTP_PROXY=http://127.0.0.1:7890
HTTPS_PROXY=http://127.0.0.1:7890
...
```

---

## 14. 再次测试镜像拉取

执行：

```bash
docker pull redis:7
docker pull nginx:stable
```

如果成功，则说明 Docker daemon 的代理已经生效。

---

## 15. 再次启动 Compose

执行：

```bash
docker compose up -d
```

最终成功：

```text
[+] up 3/3
 ✔ Network phase6-compose_default   Created
 ✔ Container phase6-compose-web-1   Started
 ✔ Container phase6-compose-redis-1 Started
```

查看：

```bash
docker compose ps
```

结果：

```text
NAME                     IMAGE          SERVICE   STATUS   PORTS
phase6-compose-redis-1   redis:7        redis     Up       6379/tcp
phase6-compose-web-1     nginx:stable   web       Up       0.0.0.0:8083->80/tcp
```

---

## 16. 最终网络结构

当前 Compose 大致结构：

```text
Host
│
│  localhost:8083
│
▼
Nginx Container
web
│
│ phase6-compose_default
│
▼
Redis Container
redis:6379
```

其中：

```text
宿主机 8083
    ↓
容器 web:80
```

Redis 没有映射到宿主机：

```text
redis:6379
```

只在 Compose 内部网络中使用。

---

## 17. Compose 中的服务名解析

Compose 会自动创建默认网络：

```text
phase6-compose_default
```

同一网络中的服务可以直接通过服务名通信。

例如：

```text
redis:6379
```

而不需要写：

```text
172.x.x.x:6379
```

因为 Docker 内置 DNS 会完成：

```text
redis
  ↓
Docker DNS
  ↓
Redis Container IP
```

---

## 18. 常用验证命令

### 查看 Compose 服务

```bash
docker compose ps
```

### 查看全部日志

```bash
docker compose logs
```

### 持续查看 Web 服务日志

```bash
docker compose logs -f web
```

### 测试 Redis

```bash
docker compose exec redis redis-cli ping
```

正常返回：

```text
PONG
```

### 停止并删除 Compose 服务

```bash
docker compose down
```

---

## 19. 完整故障链路总结

### 问题 1

```text
docker compose
     ↓
unknown command
```

原因：

```text
安装的是 Ubuntu docker.io
没有 Compose V2 plugin
```

解决：

```text
切换到 Docker 官方仓库
安装 docker-compose-plugin
```

---

### 问题 2

```text
docker version
     ↓
Cannot connect to Docker daemon
```

原因：

```text
dockerd 没有启动
```

解决：

```bash
sudo systemctl start docker
```

---

### 问题 3

```text
docker compose up
     ↓
Docker Hub timeout
```

测试：

```text
curl Docker Hub
     ↓
成功
```

但：

```text
docker pull
     ↓
失败
```

检查：

```text
Shell:
HTTP_PROXY=http://127.0.0.1:7890

Docker daemon:
Environment=
```

最终定位：

```text
Shell 有代理
dockerd 没代理
```

解决：

```text
systemd 为 docker.service 配置 HTTP_PROXY / HTTPS_PROXY
```

---

## 20. 快速排障流程

以后遇到 `docker pull` 失败，可以按下面顺序检查。

### 第一步：Docker daemon 是否运行

```bash
docker version
```

如果只有 Client，没有 Server：

```bash
sudo systemctl status docker
```

---

### 第二步：直接测试镜像拉取

```bash
docker pull hello-world
```

---

### 第三步：测试 Docker Hub 网络

```bash
curl -I https://registry-1.docker.io/v2/
```

如果得到：

```text
401 Unauthorized
```

说明网络基本正常。

---

### 第四步：查看 Shell 代理

```bash
env | grep -i proxy
```

---

### 第五步：查看 Docker daemon 代理

```bash
sudo systemctl show docker --property=Environment
```

如果 Shell 有代理而 Docker daemon 没代理，就需要单独配置 `dockerd`。

---

### 第六步：查看 Docker 日志

```bash
sudo journalctl -u docker -n 100 --no-pager
```

---

## 21. 推荐记住的核心概念

### Docker CLI 不等于 Docker daemon

```text
docker
  ↓
Docker CLI
  ↓
/var/run/docker.sock
  ↓
dockerd
```

---

### `docker pull` 的网络访问者是 dockerd

```text
docker pull redis:7
      ↓
Docker CLI
      ↓
dockerd
      ↓
registry-1.docker.io
```

因此 Shell 能访问互联网并不代表 `dockerd` 能访问互联网。

---

### Docker Compose 是 Docker CLI 插件

现代 Docker 推荐：

```bash
docker compose
```

而不是旧版：

```bash
docker-compose
```

---

## 22. 本次最终状态

最终环境：

```text
Docker Engine - Community 29.x
Docker Compose V2
Docker Buildx
systemd 管理 dockerd
Docker daemon 已配置代理
```

Compose 服务：

```text
nginx:stable
redis:7
```

启动成功：

```bash
docker compose up -d
```

访问 Nginx：

```text
http://localhost:8083
```

测试 Redis：

```bash
docker compose exec redis redis-cli ping
```

返回：

```text
PONG
```

---

## 23. 建议后续学习方向

完成本次排障后，可以继续学习：

1. Docker Compose 自定义网络
2. Volume 与数据持久化
3. Dockerfile + Compose 联合构建
4. 容器间服务发现
5. 健康检查 `healthcheck`
6. `depends_on`
7. 环境变量与 `.env`
8. ROS2 / DDS 在 Docker 网络中的通信
9. `--network host`
10. Docker 中的组播与 DDS Discovery
