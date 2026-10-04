# 第九部分：host 网络与共享宿主机网络

本节对应 [学习指南](../learn_guide.md) 的“10. 你非常值得额外学：host 网络”。通过 Redis 实验，验证 host 模式下的回环地址、容器 IP、端口发布、端口冲突，并将配置迁移到 Compose。

练习文件为 [phase8-host/compose.yaml](../phase8-host/compose.yaml)。前一章的桥接网络对照实验使用 [phase7-network/compose.yaml](../phase7-network/compose.yaml)。本次所有命令都在 WSL Linux 终端执行，Docker Engine 运行在该 WSL 环境中。

## 1. 核心概念：共享网络命名空间

Linux 上，host 模式让容器共享 Docker 宿主机的网络命名空间，不再拥有独立的网络栈，也不单独分配容器 IP。

```text
bridge 模式：
容器独立网络 → 桥接网络 → 宿主机网络

host 模式：
容器中的程序 → 与宿主机共享的网络
```

| 项目 | bridge 模式 | host 模式 |
|---|---|---|
| 网络命名空间 | 容器独立 | 与宿主机共享 |
| 容器 IP | 单独分配 | 不单独分配 |
| 容器内 localhost | 容器自身的回环地址 | 共享宿主机网络的回环地址 |
| 端口发布 | 使用 `-p` 将容器端口发布到宿主机 | `-p` 不生效，程序直接监听宿主机网络中的端口 |
| 服务名发现 | 用户自定义桥接网络提供网络范围内的名称解析 | 不提供 Compose 桥接网络的服务名发现机制 |
| 同地址、同端口的竞争 | 独立网络中可分别监听；发布到宿主机时仍需避免冲突 | 共享网络中的进程会竞争相同监听地址和端口 |

**共享网络不等于取消全部隔离。** 文件系统、进程等其他隔离仍然保留，host 模式也不要求使用 `--privileged`。

这里的“宿主机”指运行 Docker daemon 的系统。本次是 WSL Linux，不应直接等同于 Windows 网络。Docker Desktop 的 host 功能需要单独启用，且实现限制与原生 Linux Engine 不完全相同；本节实际结果针对当前 WSL 中运行的 Linux Engine。

## 2. 实验一：host 模式下的 localhost

### 启动 Redis

重做实验前，先确认端口未被其他程序占用，且练习容器名没有被占用：

```bash
ss -lnt 'sport = :6381'
```

只有表头、没有监听记录时，启动练习容器：

```bash
docker run -d --rm \
  --name redis_host_test \
  --network host \
  redis:7 \
  redis-server --bind 127.0.0.1 --port 6381
```

| 参数 | 作用 |
|---|---|
| `--network host` | 共享 Docker 宿主机网络 |
| `--bind 127.0.0.1` | Redis 监听共享网络中的回环地址 |
| `--port 6381` | Redis 自身监听 6381 端口 |
| `-d` | 后台运行 |
| `--rm` | 退出后自动删除练习容器 |

本例没有 `-p`。检查宿主机监听：

```bash
ss -lnt 'sport = :6381'
```

实际输出包含：

```text
LISTEN 0 511 127.0.0.1:6381 0.0.0.0:*
```

host 模式不会改变程序选择的监听地址。本例只监听回环地址，不会因为使用 host 就自动监听所有网卡。

### 从另一个 host 容器连接

```bash
docker run --rm --network host \
  redis:7 \
  redis-cli -h 127.0.0.1 -p 6381 ping
```

实际输出：

```text
PONG
```

两个容器中的程序访问同一个共享网络的回环地址：

```text
host 客户端容器
       │ 127.0.0.1:6381
       ↓
共享宿主机网络中的 Redis 监听
       ↑
host Redis 容器
```

### 从 bridge 容器访问相同地址

前一章的 `phase7-network_frontend` 网络需要仍然存在。执行：

```bash
docker run --rm \
  --network phase7-network_frontend \
  redis:7 \
  redis-cli -h 127.0.0.1 -p 6381 ping
```

实际输出：

```text
Could not connect to Redis at 127.0.0.1:6381: Connection refused
```

bridge 客户端有自己的网络命名空间，`127.0.0.1` 指向客户端自身，它没有监听 6381 的 Redis。这个结果验证了回环地址的范围，不表示 bridge 容器不能通过其他合适的地址访问宿主机服务。

## 3. 实验二：查看网络模式与 IP

```bash
docker inspect redis_host_test \
  --format '{{.HostConfig.NetworkMode}}'

docker inspect redis_host_test \
  --format '{{json .NetworkSettings.Networks}}'
```

本次实际结果：

- 网络模式是 `host`。
- 网络信息中包含 `host` 项。
- `IPAddress`、`Gateway`、`MacAddress` 为空，没有单独分配之前桥接网络中的 `172.x.x.x` 地址。

`IPAddress` 为空不代表无法联网；容器中的程序使用的是共享宿主机网络。

## 4. 实验三：端口发布被忽略，监听发生冲突

保持第一个 Redis 运行，尝试启动另一个监听相同地址和端口的 Redis：

```bash
docker run --rm \
  --network host \
  -p 16381:6381 \
  redis:7 \
  redis-server --bind 127.0.0.1 --port 6381
echo $?
```

实际输出中的关键部分：

```text
WARNING: Published ports are discarded when using host network mode
Could not create server TCP listening socket 127.0.0.1:6381: bind: Address already in use
Failed listening on port 6381 (tcp), aborting.
1
```

这里的因果关系是：

```text
-p 16381:6381 被忽略
         ↓
程序仍直接监听共享网络中的 127.0.0.1:6381
         ↓
同一地址和端口已被第一个 Redis 占用
         ↓
监听失败，退出码为 1
```

`echo $?` 要紧接着被检查的命令执行，插入其他命令会改变退出码。本次日志还包含 Redis overcommit 提示，但导致这次启动失败的明确原因是监听地址和端口已经被占用。

### 修改程序真正监听的端口

先检查另一个端口及尝试发布的端口：

```bash
ss -lnt '( sport = :6382 or sport = :16382 )'
```

本次只有表头，没有监听记录。随后启动第二个 Redis：

```bash
docker run -d --rm \
  --name redis_host_second \
  --network host \
  -p 16382:6382 \
  redis:7 \
  redis-server --bind 127.0.0.1 --port 6382
```

实际仍出现端口发布被忽略的警告，但容器启动成功。连接程序实际监听的端口：

```bash
docker run --rm --network host \
  redis:7 \
  redis-cli -h 127.0.0.1 -p 6382 ping
```

实际输出为 `PONG`。解决冲突的是 Redis 的 `--port 6382`，不是 `-p` 左侧的 16382。

还可以补充用下面的命令查看监听，预期只看到本例 Redis 的 6382，而不是发布参数中的 16382；本次未提供启动后的这条检查输出：

```bash
ss -lnt '( sport = :6382 or sport = :16382 )'
```

## 5. 实验四：在 Compose 中使用 host 模式

先释放前面练习容器占用的端口；如果它们尚在运行，执行：

```bash
docker stop redis_host_test redis_host_second
```

它们带有 `--rm`，停止后自动删除。确认 6381 没有监听记录，再准备 Compose 目录：

```bash
ss -lnt 'sport = :6381'
cd ~/cpp_study/docker
mkdir -p phase8-host
cd phase8-host
```

### 完整 compose.yaml

```yaml
services:
  redis-host:
    image: redis:7
    network_mode: host
    command:
      - redis-server
      - --bind
      - 127.0.0.1
      - --port
      - "6381"
    healthcheck:
      test: ["CMD", "redis-cli", "-p", "6381", "ping"]
      interval: 2s
      timeout: 1s
      retries: 5

  client:
    image: redis:7
    network_mode: host
    command: ["redis-cli", "-h", "127.0.0.1", "-p", "6381", "ping"]
    depends_on:
      redis-host:
        condition: service_healthy
```

配置要点：

- `network_mode: host` 设置网络模式，服务不能同时配置 `networks`，也不应配置 `ports`。
- 两个服务均使用 host 模式，客户端通过共享回环地址连接 Redis。
- `depends_on` 中的 `redis-host` 是 Compose 服务名，用于启动依赖，不是客户端连接使用的 DNS 名称。
- `healthcheck` 检查 6381 端口的 Redis 响应，客户端等待检查通过再启动。
- 两个服务都使用 host 模式，本例不会创建项目默认桥接网络。

### 启动与验证

```bash
docker compose config
docker compose up -d
docker compose ps -a
docker compose logs --tail=10 client
```

本次实际验证结果：

| 项目 | 实际结果 |
|---|---|
| `redis-host` | `Up ... (healthy)` |
| `client` | `Exited (0)` |
| 客户端日志 | `client-1 \| PONG` |

客户端只是一次性执行 `ping`，成功后正常退出。清理项目：

```bash
docker compose down
```

本次删除了两个容器，没有删除项目桥接网络的步骤，因为配置没有创建这种网络。Docker 自带的 `host` 网络仍然存在。

## 6. 与 ROS2、DDS 和 WSL 的关系

ROS2 通过底层中间件进行自动发现；同一个 ROS domain 中的节点可以发现彼此，建立相应通信还需要兼容的 QoS 等条件。

从网络结构推导，host 模式可以减少 Docker 独立容器网络、地址转换和端口发布这一层配置因素。因此它是后续 ROS2/DDS 网络实验值得比较的模式，但本节 Redis 的 TCP 成功结果不能替代 ROS2、DDS、广播或组播验证。

在当前环境中，需要区分两层：

```text
容器程序
    │ host 模式共享
    ↓
WSL Linux 网络
    │ 仍受 WSL 网络模式、Windows 网络与防火墙等因素影响
    ↓
Windows 与局域网
```

host 模式不会自动消除 WSL 到局域网这一层网络差异。微软将组播支持列为 WSL mirrored 网络模式的特性之一；本节没有修改 WSL 网络模式，也没有验证跨机器发现。

后续 ROS2 实验应分别确认节点发现、数据传输、ROS domain、QoS 及网络配置，依据实际结果判断。

## 7. 本节结论与检查方法

1. Linux host 模式共享 Docker 宿主机网络命名空间，不单独分配容器 IP。
2. host 容器中的 `localhost` 指向共享宿主机网络；bridge 容器中的 `localhost` 指向自己的网络。
3. host 模式的 `-p` 不生效，程序自己的监听地址和端口决定访问方式。
4. 两个 host 容器的程序不能像独立网络那样分别占用同一个监听地址和端口；本例必须修改程序端口来避免冲突。
5. Compose 使用 `network_mode: host`，不配置服务的 `networks` 和 `ports`；启动依赖和健康检查仍可使用。
6. 共享网络不取消文件系统、进程等其他隔离，在本次环境中共享的是 WSL Linux 网络。

| 检查目标 | 命令 |
|---|---|
| 宿主机监听端口 | `ss -lnt 'sport = :6381'` |
| 容器网络模式 | `docker inspect redis_host_test --format '{{.HostConfig.NetworkMode}}'` |
| 容器网络信息 | `docker inspect redis_host_test --format '{{json .NetworkSettings.Networks}}'` |
| Compose 状态，包括一次性任务 | `docker compose ps -a` |
| 客户端输出 | `docker compose logs --tail=10 client` |

检查命令应在对应练习容器仍然存在时执行；完成 `--rm` 容器清理或 `compose down` 后，再检查已删除容器会报不存在。

官方参考：[host 网络驱动](https://docs.docker.com/engine/network/drivers/host/)、[Compose network_mode](https://docs.docker.com/reference/compose-file/services/#network_mode)、[Compose 启动依赖](https://docs.docker.com/compose/how-tos/startup-order/)、[ROS2 官方发现机制说明源码](https://github.com/ros2/ros2_documentation/blob/humble/source/Concepts/Basic/About-Discovery.rst)、[WSL 网络与 mirrored 模式](https://learn.microsoft.com/en-us/windows/wsl/networking#mirrored-mode-networking)。
