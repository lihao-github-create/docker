# 第十一部分：ROS2 Docker 多容器通信

本节对应 [学习指南](../learn_guide.md) 第十一章的“ROS2 Docker”小项目。使用两个容器运行 C++ 演示节点，在同一个自定义 bridge 网络上验证 DDS 发现、Domain ID、发布订阅和消息传输，再迁移到 Compose。

实际配置为 [phase10-ros2/compose.yaml](../phase10-ros2/compose.yaml)。本次命令在 WSL Linux 终端执行，Docker Engine 运行在该 WSL 环境中。

## 1. 核心概念

```text
Docker 网络 ros2_lab（自定义 bridge）
┌─────────────────────────────────────────────────────────┐
│ talker 容器                         listener 容器        │
│ ROS_DOMAIN_ID=42                    ROS_DOMAIN_ID=42     │
│ /talker ── /chatter（String 消息）──→ /listener           │
└─────────────────────────────────────────────────────────┘
```

| 概念 | 本例中的作用 |
|---|---|
| Docker 网络 | 为两个容器提供通信路径 |
| DDS 发现 | ROS2 中间件发现节点和发布订阅端点，无需手写对方 IP |
| `ROS_DOMAIN_ID` | 选择 DDS 域，不同域的节点不会互相发现 |
| 节点 | `/talker` 发布消息，`/listener` 接收消息 |
| 话题 | 两个节点通过 `/chatter` 交换消息 |
| 消息类型 | `/chatter` 使用 `std_msgs/msg/String` |
| QoS | 控制可靠性、持久性等通信策略；发布订阅双方必须兼容 |

相同 Docker 网络和相同 Domain ID 并不自动保证所有 ROS2 应用都能通信，还需要话题、消息类型、QoS 和实际网络条件满足要求。本次演示节点的配置经过实际收发验证。

本次实验使用 bridge 模式，没有使用 `--network host`、`-p` 或手动填写容器 IP。它验证了当前环境中的跨容器发现和消息传输；没有通过抓包分析底层发现报文。

## 2. 实验一：两个容器在同一个域通信

### 准备镜像和网络

```bash
docker pull osrf/ros:humble-desktop
docker network create ros2_lab
```

镜像选择包含演示节点的 `osrf/ros:humble-desktop`。基础镜像 `ros:humble` 的包集合不同，不能直接假设其中包含 `demo_nodes_cpp`。

准备时可以检查演示程序：

```bash
docker run --rm osrf/ros:humble-desktop \
  ros2 pkg executables demo_nodes_cpp
```

预期包含 `talker` 和 `listener`。复现实验时，如果 `ros2_lab` 已存在，直接复用即可，无需重复创建。

### 在第一个终端启动发布者

```bash
docker run --rm -it \
  --name ros2_talker \
  --network ros2_lab \
  -e ROS_DOMAIN_ID=42 \
  osrf/ros:humble-desktop \
  ros2 run demo_nodes_cpp talker
```

实际持续输出，节选：

```text
[talker]: Publishing: 'Hello World: 22'
[talker]: Publishing: 'Hello World: 23'
[talker]: Publishing: 'Hello World: 24'
```

### 保持发布者运行，在第二个终端启动订阅者

```bash
docker run --rm -it \
  --name ros2_listener \
  --network ros2_lab \
  -e ROS_DOMAIN_ID=42 \
  osrf/ros:humble-desktop \
  ros2 run demo_nodes_cpp listener
```

实际持续收到对应消息，节选：

```text
[listener]: I heard: [Hello World: 22]
[listener]: I heard: [Hello World: 23]
[listener]: I heard: [Hello World: 24]
```

两个容器属于同一个网络，两个节点属于 DDS 域 42。节点的发现与数据通信交给 ROS2 中间件处理；这里没有通过 Docker 服务名指定对方。

`--rm` 在容器退出后删除容器。前台节点持续运行时不会立即退出；按 `Ctrl+C` 结束本次节点运行。

## 3. 实验二：只改变 Domain ID

保持 talker 在域 42 中运行。在 listener 终端按 `Ctrl+C`，原容器删除后，将 listener 改为域 43：

```bash
docker run --rm -it \
  --name ros2_listener \
  --network ros2_lab \
  -e ROS_DOMAIN_ID=43 \
  osrf/ros:humble-desktop \
  ros2 run demo_nodes_cpp listener
```

观察约 5～10 秒，再按 `Ctrl+C`，使用上一节的域 42 命令重新启动 listener。

实际确认的结果：

| talker 域 | listener 域 | Docker 网络 | 结果 |
|---|---|---|---|
| 42 | 42 | 都为 `ros2_lab` | 正常接收 |
| 42 | 43 | 都为 `ros2_lab` | 停止接收 |
| 42 | 改回 42 | 都为 `ros2_lab` | 恢复接收 |

这个对照实验只改变 Domain ID，Docker 网络配置保持一致。它说明底层网络提供通信路径，DDS 域还决定节点能否加入同一组发现与通信。

Domain ID 默认值为 0。本次显式使用 42；它不是密码，也不应当作为访问控制机制。

## 4. 实验三：Compose 与已有的外部网络

先在两个终端分别按 `Ctrl+C` 停止手动启动的节点，再准备目录：

```bash
cd ~/cpp_study/docker
mkdir -p phase10-ros2
cd phase10-ros2
```

实际 [compose.yaml](../phase10-ros2/compose.yaml)：

```yaml
services:
  talker:
    image: osrf/ros:humble-desktop
    environment:
      ROS_DOMAIN_ID: "42"
    command: ["ros2", "run", "demo_nodes_cpp", "talker"]
    networks:
      - ros2_lab

  listener:
    image: osrf/ros:humble-desktop
    environment:
      ROS_DOMAIN_ID: "42"
    command: ["ros2", "run", "demo_nodes_cpp", "listener"]
    networks:
      - ros2_lab

networks:
  ros2_lab:
    external: true
    name: ros2_lab
```

| 配置 | 含义 |
|---|---|
| `environment` | 为两个服务分别设置 Domain ID |
| `command` | 覆盖镜像默认命令，指定要运行的节点 |
| `networks` | 两个服务都连接 `ros2_lab` |
| `external: true` | 使用已有网络，由 Compose 项目之外管理 |
| `name: ros2_lab` | 使用确切网络名，不添加 Compose 项目前缀 |

`external` 网络必须事先存在。这里没有配置 `depends_on`：演示节点可以持续参与 DDS 发现，listener 晚启动也能接收后续消息，无需依靠固定启动顺序。

启动并检查：

```bash
docker compose up -d
docker compose ps
docker compose logs --tail=5 talker
docker compose logs --tail=5 listener
```

实际 `ps` 中两个容器均为 `Up`，名称分别是 `phase10-ros2-talker-1` 和 `phase10-ros2-listener-1`。日志中 talker 持续发布，listener 持续接收。

Docker 容器名、Compose 服务名和 ROS2 节点名属于不同层次：例如 `phase10-ros2-talker-1` 是容器名，`talker` 是 Compose 服务名，`/talker` 是 ROS2 节点全名。

## 5. 镜像入口与 exec 的环境加载

ROS 镜像的入口脚本先加载 `/opt/ros/$ROS_DISTRO/setup.bash`，再通过 `exec "$@"` 执行传入命令。因此 `docker run ... ros2 ...` 和 Compose 的节点命令可以直接启动。

进入已经运行的 talker 容器时，`exec` 不会重新执行镜像入口脚本，需要在新的 shell 中加载环境：

```bash
docker compose exec talker bash
source /opt/ros/humble/setup.bash
```

这个 shell 继承容器配置中的 `ROS_DOMAIN_ID=42`。之后的 ROS2 查询就在同一 DDS 域内进行。

结束查询后执行 `exit`，只退出本次交互 shell，不会停止容器中原有的 talker 节点。

## 6. 实验四：查看节点、话题和端点

以下命令在已加载 ROS2 环境的容器 shell 内执行。

### 节点列表

```bash
ros2 node list
```

实际输出：

```text
/listener
/talker
```

命令在 talker 容器中执行，也能发现另一个容器中的 listener。查询结果来自 ROS2 发现到的图，不局限于本容器进程。

### 话题与消息类型

```bash
ros2 topic list -t
```

实际输出：

```text
/chatter [std_msgs/msg/String]
/parameter_events [rcl_interfaces/msg/ParameterEvent]
/rosout [rcl_interfaces/msg/Log]
```

`/chatter` 是本次演示消息的话题；另外两个话题分别用于参数事件和 ROS 日志。

### 发布订阅端点与 QoS

```bash
ros2 topic info /chatter --verbose
```

实际主要结果：

| 字段 | 发布端 | 订阅端 |
|---|---|---|
| 消息类型 | `std_msgs/msg/String` | `std_msgs/msg/String` |
| 端点数量 | 1 | 1 |
| 节点名 | `talker` | `listener` |
| 节点命名空间 | `/` | `/` |
| 端点类型 | `PUBLISHER` | `SUBSCRIPTION` |
| Reliability | `RELIABLE` | `RELIABLE` |
| History (Depth) | `UNKNOWN` | `UNKNOWN` |
| Durability | `VOLATILE` | `VOLATILE` |
| Lifespan | `Infinite` | `Infinite` |
| Deadline | `Infinite` | `Infinite` |
| Liveliness | `AUTOMATIC` | `AUTOMATIC` |
| Liveliness lease duration | `Infinite` | `Infinite` |

`RELIABLE` 使用可靠传输策略，必要时重传；它不等同于应用层使用 TCP。`VOLATILE` 不为晚加入的订阅者保存并补发历史消息。

`History (Depth): UNKNOWN` 表示这条查询没有提供历史策略与队列深度信息，不能由此判断队列为空或程序配置错误。本次实际收发正常，也无需为了这个显示值修改配置。

QoS 要求兼容，并非所有字段必须完全相同。例如可靠发布者可以匹配 best-effort 订阅者，而 best-effort 发布者不能满足 reliable 订阅者的可靠性要求。当前实验双方的可靠性与持久性配置一致。

### 查看实际消息

```bash
ros2 topic echo /chatter
```

实际输出：

```yaml
data: 'Hello World: 342'
---
data: 'Hello World: 343'
---
data: 'Hello World: 344'
---
```

这验证了消息本身已经传到查询进程。`echo` 从启动后的新消息开始接收，符合本例的 volatile 配置。按 `Ctrl+C` 只结束 `echo`，原来的 listener 仍在运行。

`echo` 本身也会创建一个订阅者。补充观察时，可以保持 `echo` 运行，在另一个已加载环境的查询进程中执行 `ros2 topic info /chatter`，订阅数量通常会从 1 增加到 2。结束 `echo` 后再观察，会恢复到 1；这个数量对照尚未提供实际输出。

## 7. 补充验证：外部网络的生命周期

以下收尾命令已在引导中给出，尚未提供实际输出。

先在容器 shell 中退出：

```bash
exit
```

在宿主机的 `phase10-ros2` 目录执行：

```bash
docker compose down
docker network inspect ros2_lab --format '{{.Name}}'
```

预期删除两个服务容器，但查询仍输出 `ros2_lab`。因为该网络声明为 `external`，Compose 不会在 `down` 时删除它。

## 8. 学习结果与参考

本次实际验证了同域跨容器通信、不同域停止接收、恢复原域后恢复通信、Compose 复用外部网络，以及 ROS2 节点、话题、QoS 和消息查询。结合 [C++ Docker 开发实验](10-cpp-docker-development.md)，完成了第十一章本次新增的两个小项目。

- [ROS2 Docker 演示文档源码](https://github.com/ros2/ros2_documentation/blob/humble/source/How-To-Guides/Run-2-nodes-in-single-or-separate-docker-containers.rst)：演示镜像和 talker/listener。
- [Domain ID 文档源码](https://github.com/ros2/ros2_documentation/blob/humble/source/Concepts/Intermediate/About-Domain-ID.rst)：DDS 域与发现。
- [节点查询文档源码](https://github.com/ros2/ros2_documentation/blob/humble/source/Tutorials/Beginner-CLI-Tools/Understanding-ROS2-Nodes/Understanding-ROS2-Nodes.rst)：节点与 ROS 图。
- [话题查询文档源码](https://github.com/ros2/ros2_documentation/blob/humble/source/Tutorials/Beginner-CLI-Tools/Understanding-ROS2-Topics/Understanding-ROS2-Topics.rst)：话题类型、端点与 echo。
- [QoS 文档源码](https://github.com/ros2/ros2_documentation/blob/humble/source/Concepts/Intermediate/About-Quality-of-Service-Settings.rst)：策略与兼容性。
- [ROS 镜像入口脚本](https://github.com/osrf/docker_images/blob/master/ros/humble/ubuntu/jammy/ros-core/ros_entrypoint.sh)：环境加载与命令执行。
- [Compose 网络说明](https://docs.docker.com/compose/how-tos/networking/)：复用外部网络。
- [Compose down](https://docs.docker.com/reference/cli/docker/compose/down/)：容器和网络的生命周期。
