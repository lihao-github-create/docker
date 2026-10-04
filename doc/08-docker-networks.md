# 第八部分：Docker 桥接网络、服务发现与通信隔离

本节对应 [学习指南](../learn_guide.md) 的“9. Docker 网络只需要先理解这个层次”。在 Compose 实验基础上，观察网络成员、服务名解析，并分别验证名称解析范围和直接 IP 通信隔离。

实验使用 [phase6-compose/compose.yaml](../phase6-compose/compose.yaml) 和 [phase7-network/compose.yaml](../phase7-network/compose.yaml)。网络练习文件目前是客户端同时加入两个网络的最终版本；复现最初的隔离状态，需要先让客户端只加入 `frontend`。

## 1. 核心概念

| 概念         | 含义                                               |
| ------------ | -------------------------------------------------- |
| Docker 网络  | 容器连接及通信所使用的网络资源                     |
| bridge 驱动  | 在同一 Docker 主机上创建桥接网络，供容器连接       |
| 网络成员     | 加入该网络的容器；一个容器可以加入多个网络         |
| 子网 Subnet  | 网络使用的 IP 地址范围                             |
| 网关 Gateway | 容器在该网络上的网关地址                           |
| 容器 IP      | 容器在某个网络中的地址，不应假定重建后保持不变     |
| 服务发现     | 通过服务名解析出同一网络中目标容器的 IP            |
| 端口发布     | 将容器端口映射到宿主机端口，提供经宿主机访问的路径 |

### 两种 bridge 网络

| 网络                     | 典型来源                                          | 名称解析                         |
| ------------------------ | ------------------------------------------------- | -------------------------------- |
| Docker 自带的 `bridge` | 普通 `docker run` 未指定网络时使用              | 默认不提供容器名称的自动解析     |
| 用户自定义 bridge 网络   | Compose 创建，或用 `docker network create` 创建 | 提供网络内的容器名称或服务名解析 |

`bridge` 既是驱动名称，也是 Docker 自带网络的名称。多个不同网络可以使用同一种 `bridge` 驱动，但仍然是独立的网络。

指南中的 `docker0` 是 Docker 默认桥接网络的示意。Compose 创建的用户自定义桥接网络通常使用另外的网桥，不应理解成所有容器都连接同一个 `docker0`。

## 2. 实验一：检查 Compose 默认网络

在宿主机执行：

```bash
cd ~/cpp_study/docker/phase6-compose
docker compose up -d web redis
docker network ls
docker network inspect phase6-compose_default
```

未显式配置网络时，Compose 为项目创建默认网络。本次项目名为 `phase6-compose`，所以网络名为 `phase6-compose_default`；如果修改项目名，默认资源名称也会变化。

本次实际检查结果：

| 字段                     | 实际值            | 解读                             |
| ------------------------ | ----------------- | -------------------------------- |
| `Driver`               | `bridge`        | 使用桥接驱动                     |
| `EnableIPv4`           | `true`          | 开启 IPv4                        |
| `EnableIPv6`           | `false`         | 未开启 IPv6                      |
| `IPAM.Config.Subnet`   | `172.18.0.0/16` | 子网前缀长度为 16 位             |
| `IPAM.Config.Gateway`  | `172.18.0.1`    | 网关地址                         |
| Redis 的 `IPv4Address` | `172.18.0.2/16` | Redis 在该网络上的地址及前缀长度 |
| Nginx 的 `IPv4Address` | `172.18.0.3/16` | Nginx 在该网络上的地址及前缀长度 |

```text
phase6-compose_default：172.18.0.0/16
├── 网关：172.18.0.1
├── redis：172.18.0.2
└── web：172.18.0.3
```

这些地址是本次运行结果，不是固定配置。复现时读取自己当前的 `inspect` 输出即可。

## 3. 实验二：服务名解析为容器 IP

第六阶段的 `client` 是一次性读取任务，已执行完毕，因此使用临时容器进行查询：

```bash
docker compose run --rm --no-deps client getent hosts redis
```

- `run` 根据客户端服务配置创建新容器，并加入其配置中的网络。
- `--rm` 在任务完成后删除临时容器。
- `--no-deps` 不启动依赖服务，本例 Redis 已经运行。
- `getent hosts redis` 查询名称 `redis` 对应的地址。

实际输出：

```text
172.18.0.2      redis
```

查询结果与网络检查中的 Redis 容器地址一致，验证了服务发现：

```text
redis 服务名 → Docker 内置 DNS → Redis 容器 IP
```

可以补充查看客户端的解析配置：

```bash
docker compose run --rm --no-deps client cat /etc/resolv.conf
```

在这类用户自定义网络中，通常能看到 `nameserver 127.0.0.11`，这是 Docker 内置 DNS 的地址。

容器中的 `localhost` 指向该容器自身。连接另一个服务时，本例使用 `redis:6379`，而不是 `localhost:6379`。

## 4. 实验三：不同网络中的名称解析范围

在新的练习目录中创建初始配置：

```bash
cd ~/cpp_study/docker
mkdir -p phase7-network
cd phase7-network
```

```yaml
services:
  redis:
    image: redis:7
    networks:
      - backend

  client:
    image: redis:7
    command: ["sleep", "infinity"]
    networks:
      - frontend

networks:
  frontend:
    driver: bridge
  backend:
    driver: bridge
```

- 顶层 `networks` 声明网络资源。
- 服务内的 `networks` 指定该服务加入哪些网络。
- 两个服务都显式配置了网络，此例不再依赖隐式的默认网络。

```text
frontend 网络                 backend 网络
┌─────────────┐              ┌─────────────┐
│   client    │              │    redis    │
└─────────────┘              └─────────────┘
             没有共同网络
```

启动并检查 Redis 本身：

```bash
docker compose config
docker compose up -d
docker compose exec redis redis-cli ping
```

确认 Redis 返回 `PONG` 后，可以检查网络成员：

```bash
docker network inspect phase7-network_frontend
docker network inspect phase7-network_backend
```

配置对应的预期成员是：`frontend` 中只有客户端，`backend` 中只有 Redis。

从客户端查询并尝试连接：

```bash
docker compose exec client getent hosts redis
docker compose exec client redis-cli -h redis ping
```

本次名称查询没有地址输出，连接实际报错：

```text
Could not connect to Redis at redis:6379: Name or service not known
```

这是名称解析失败。它本身尚不能证明使用目标 IP 也无法连接，需要下一组直接 IP 实验进一步验证。

### 让客户端加入 backend

将客户端的网络配置改为：

```yaml
  client:
    image: redis:7
    command: ["sleep", "infinity"]
    networks:
      - frontend
      - backend
```

应用配置并测试：

```bash
docker compose up -d client
docker compose exec client getent hosts redis
docker compose exec client redis-cli -h redis ping
```

实际结果：

```text
172.20.0.2      redis
PONG
```

客户端加入 `backend` 后，与 Redis 共享网络，可以发现并连接该服务。名称解析的网络范围不是整个 Compose 项目，而是容器共享的网络。

```text
frontend ── client ── backend ── redis
```

客户端可以访问两个网络中的服务，但不会仅因加入多个网络，就自动为其他容器转发流量。

## 5. 实验四：绕过 DNS，验证直接 IP 通信隔离

仍在 `phase7-network` 目录操作。此时 Redis 只加入 `backend`，没有发布宿主机端口。

本次 Redis 的实际 IP 为 `172.20.0.2`。以下命令保留实验地址；复现时应替换为当前地址，不能假定一定相同。

### 从 backend 网络连接

```bash
docker run --rm \
  --network phase7-network_backend \
  redis:7 \
  redis-cli -h 172.20.0.2 -p 6379 ping
echo $?
```

本次实际输出：

```text
PONG
0
```

### 从 frontend 网络连接相同 IP

```bash
docker run --rm \
  --network phase7-network_frontend \
  redis:7 \
  timeout 3s redis-cli -h 172.20.0.2 -p 6379 ping
echo $?
```

本次没有 Redis 响应输出，3 秒后退出，退出码为：

```text
124
```

`timeout 3s` 限制连接尝试的运行时间，`124` 表示命令被超时终止。`echo $?` 必须紧接着被检查的命令执行，插入其他命令会改变所检查的退出码。

| 临时容器所在网络 | 目标                  | 实际结果           |
| ---------------- | --------------------- | ------------------ |
| `backend`      | Redis IP 的 6379 端口 | `PONG`，退出码 0 |
| `frontend`     | 同一个 IP 和端口      | 超时，退出码 124   |

同镜像、同目标 IP、同端口，只改变客户端加入的网络。这组结果验证了当前默认桥接配置下，两个网络之间的直接容器 IP 通信隔离。


## 6. 容器端口、宿主机端口与隔离范围

前面的 Compose 实验中，Nginx 发布了端口，Redis 没有发布端口：

```text
浏览器 → 宿主机 localhost:8083 → 端口映射 → web 容器:80

客户端 → redis:6379 → 网络内名称解析与连接 → Redis 容器:6379
```

- 共享网络的容器通信使用容器端口，本例为 6379。
- 通过宿主机发布端口访问时，使用宿主机端口，本例为 8083。
- 镜像声明或 `ps` 显示 `6379/tcp`，不等于端口已发布到宿主机。
- `EXPOSE` 或端口声明不会替代网络成员配置，也不会自动把两个网络连接起来。

本次隔离结论针对直接访问容器 IP 的路径。端口发布提供经宿主机访问的另一条路径，不能把不同桥接网络理解为任意配置下都绝对无法通信；自定义路由和防火墙设置也会影响结果。

## 7. 排查顺序与错误含义

```text
是否共享网络 → 名称能否解析 → 目标端口是否正确 → 服务是否就绪
```

| 检查             | 命令示例                                                       |
| ---------------- | -------------------------------------------------------------- |
| 网络列表         | `docker network ls`                                          |
| 网络成员与 IP    | `docker network inspect phase7-network_backend`              |
| 名称解析         | `docker compose exec client getent hosts redis`              |
| 服务容器内部响应 | `docker compose exec redis redis-cli ping`                   |
| 从客户端建立连接 | `docker compose exec client redis-cli -h redis -p 6379 ping` |

区分三个排障方向：

- 名称解析错误：先检查网络成员和服务名，不能仅据此判断 IP 路径是否可达。
- 连接被拒绝：检查目标端口、监听地址及服务是否运行，也可能涉及拒绝连接的防火墙规则。
- 连接超时：检查路由、防火墙、网络隔离及目标状态，超时本身不能唯一确定原因。本次通过同目标的对照实验确认了网络因素。

## 8. 最终配置与本节结论

最终 [phase7-network/compose.yaml](../phase7-network/compose.yaml)：

```yaml
services:
  redis:
    image: redis:7
    networks:
      - backend

  client:
    image: redis:7
    command: ["sleep", "infinity"]
    networks:
      - frontend
      - backend

networks:
  frontend:
    driver: bridge
  backend:
    driver: bridge
```

记住六点：

1. 网络名称和网络驱动是两个概念，使用同一驱动不意味着处于同一网络。
2. 用户自定义桥接网络支持网络范围内的名称解析，连接服务应使用名称而非记住某次分配的 IP。
3. Compose 服务可以显式加入一个或多个网络，是否属于同一项目不决定它们一定能互相发现。
4. 容器内的 `localhost` 指向自己；Docker 内置 DNS 的地址与目标服务 IP 也是不同概念。
5. 分别验证名称解析与直接 IP 连接，才能区分发现失败和通信隔离。
6. 本例多个网络隔离直接容器 IP 通信，发布宿主机端口则提供另一条访问路径。

本节尚未学习 `host` 模式，也未验证 ROS2、DDS、广播或组播通信。下一章对应指南的“10. 你非常值得额外学：host 网络”，观察共享宿主机网络后 IP、`localhost` 和端口发布的变化。

官方参考：[Compose 网络与服务发现](https://docs.docker.com/compose/how-tos/networking/)、[bridge 网络驱动](https://docs.docker.com/engine/network/drivers/bridge/)。
