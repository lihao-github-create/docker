如果你的目标是**快速达到“能用 Docker 做开发、部署、调试”**，不需要一开始把所有底层原理学完。建议用“20% 概念 + 80% 动手”的方式，按下面这条路线走。

# 0. 准备阶段：安装 Docker 环境

安装教程：[第零部分学习总结：安装 Docker 环境](doc/00-docker-environment-installation.md)。

教程分别说明 Ubuntu、Windows（Docker Desktop + WSL 2）和 macOS 的安装方法，并包含权限配置、Docker Engine、Buildx、Compose 验证以及常见故障排查。安装完成后，先确认以下命令能够正常执行：

```bash
docker version
docker compose version
docker run --rm hello-world
```

---

## 1. 先建立 Docker 的核心认知

学习笔记：[第一部分学习总结：Docker 的核心认知](doc/01-docker-core-concepts.md)。

先只搞懂这 5 个词：

- **Image 镜像**：程序运行环境的模板
- **Container 容器**：镜像运行后的实例
- **Dockerfile**：描述如何构建镜像
- **Volume**：容器和宿主机之间持久化数据
- **Port Mapping**：把容器端口暴露给宿主机

可以先把关系理解成：

```text
Dockerfile
    │
    │ docker build
    ↓
Image
    │
    │ docker run
    ↓
Container
```

其中：

```text
宿主机目录 ←→ Volume ←→ Container
宿主机端口 ←→ Port   ←→ Container
```

把这张关系图搞清楚，Docker 已经理解一半了。

---

# 2. 第一阶段：只学 10 个命令

学习笔记：[第二部分学习总结：Docker 基础命令与容器生命周期](doc/02-docker-basic-commands.md)。

先不要背几十个 Docker 命令。

掌握下面这些就够了：

```bash
docker version
docker images
docker ps
docker ps -a
docker pull
docker run
docker exec
docker logs
docker stop
docker rm
```

建议你直接练。

例如：

```bash
docker pull ubuntu:22.04
```

查看镜像：

```bash
docker images
```

启动 Ubuntu：

```bash
docker run -it ubuntu:22.04 bash
```

这时候你已经进入容器：

```bash
root@xxxx:/#
```

执行：

```bash
cat /etc/os-release
```

退出：

```bash
exit
```

然后：

```bash
docker ps -a
```

你会看到刚才的 Ubuntu container。

这一组实验非常重要，因为它可以帮你建立：

```text
image != container
```

这个最基本的认识。

---

# 3. 第二阶段：重点理解 docker run

学习笔记：[第三部分学习总结：重点理解 docker run](doc/03-docker-run.md)。

Docker 学习中最值得深入理解的命令其实就是：

```bash
docker run
```

例如：

```bash
docker run -it --name ubuntu_test ubuntu:22.04 bash
```

逐项理解：

```text
docker run
│
├── -i         保持 stdin
├── -t         创建终端
├── --name     指定容器名字
├── ubuntu:22.04
└── bash       容器启动后运行 bash
```

然后练习后台运行：

```bash
docker run -d --name nginx nginx
```

查看：

```bash
docker ps
```

日志：

```bash
docker logs nginx
```

进入容器：

```bash
docker exec -it nginx bash
```

如果没有 bash：

```bash
docker exec -it nginx sh
```

这几个命令以后会天天用。

---

# 4. 第三个重点：端口映射

学习笔记：[第四部分学习总结：端口映射](doc/04-docker-port-mapping.md)。

例如运行 nginx：

```bash
docker run -d \
  --name nginx_test \
  -p 8080:80 \
  nginx
```

这里：

```text
8080:80

宿主机       容器
8080   →     80
```

浏览器访问：

```text
http://localhost:8080
```

数据路径实际上是：

```text
Browser
   │
   ↓
Windows/Linux :8080
   │
   ↓
Docker
   │
   ↓
Container :80
   │
   ↓
nginx
```

以后 ROS、Web 服务、数据库都会大量使用 `-p`。

---

# 5. 第四个重点：Volume

学习笔记：[第五部分学习总结：Volume 与绑定挂载](doc/05-docker-volumes-and-bind-mounts.md)。

这个尤其重要。

因为容器本身通常应该被看成：

```text
可删除
可重建
```

真正的数据应该放到 Volume 或宿主机。

例如：

```bash
mkdir test_data
echo "hello docker" > test_data/test.txt
```

运行：

```bash
docker run -it \
  -v "$(pwd)/test_data:/data" \
  ubuntu:22.04 \
  bash
```

进入容器后：

```bash
cat /data/test.txt
```

你会看到：

```text
hello docker
```

关系：

```text
Host
~/test_data
      │
      │ bind mount
      ↓
Container
/data
```

这一点以后做开发环境尤其有用：

```text
源码放宿主机
编译环境放 Docker
```

例如：

```text
Windows / WSL
      │
      │ source code
      ↓
Docker Ubuntu
      │
      ├── gcc
      ├── cmake
      ├── ROS2
      └── CUDA
```

这其实非常适合你的使用场景。

---

# 6. 第五阶段：Dockerfile

学习笔记：[第六部分学习总结：Dockerfile 与 C++ 镜像构建](doc/06-dockerfile.md)。

等你会 `docker run` 后，立即开始 Dockerfile。

例如创建：

```dockerfile
FROM ubuntu:22.04

RUN apt update && \
    apt install -y g++ cmake

WORKDIR /workspace

COPY . .

CMD ["bash"]
```

构建：

```bash
docker build -t cpp-dev .
```

查看：

```bash
docker images
```

运行：

```bash
docker run -it cpp-dev
```

这里最重要的是理解这些指令：

```text
FROM
RUN
WORKDIR
COPY
CMD
ENTRYPOINT
ENV
ARG
```

其中前 5 个最常用。

---

# 7. 一定要理解 Dockerfile 的 Layer

Dockerfile：

```dockerfile
FROM ubuntu:22.04

RUN apt update
RUN apt install -y gcc
RUN apt install -y cmake

COPY . /app
```

可以粗略理解为：

```text
Layer 1   ubuntu
Layer 2   apt update
Layer 3   gcc
Layer 4   cmake
Layer 5   source code
```

Docker 构建时会利用缓存。

例如你只修改代码：

```text
Layer 1 ✓ cache
Layer 2 ✓ cache
Layer 3 ✓ cache
Layer 4 ✓ cache
Layer 5 rebuild
```

这也是为什么 Dockerfile 编写顺序很重要。

---

# 8. 第六阶段：Docker Compose

学习笔记：[第七部分学习总结：Docker Compose 与多服务管理](doc/07-docker-compose.md)。

等 Dockerfile 学完，就学 Compose。

比如你的系统有：

```text
web
 │
 ├── redis
 │
 └── postgres
```

如果全部用：

```bash
docker run ...
```

管理会越来越麻烦。

于是使用：

```yaml
services:

  web:
    image: my-web
    ports:
      - "8080:8080"

  redis:
    image: redis:7

  postgres:
    image: postgres:16
```

然后：

```bash
docker compose up
```

停止：

```bash
docker compose down
```

你可以把 Compose 理解成：

```text
Dockerfile
   ↓
描述一个容器

compose.yaml
   ↓
描述一组容器
```

---

# 9. Docker 网络只需要先理解这个层次

学习笔记：[第八部分学习总结：Docker 桥接网络、服务发现与通信隔离](doc/08-docker-networks.md)。

Docker 默认会给容器创建虚拟网络。

比如：

```text
Host
 │
 └── docker0
       │
       ├── container A
       │    172.x.x.2
       │
       └── container B
            172.x.x.3
```

Compose 里更方便：

```yaml
services:
  app:
    ...

  redis:
    ...
```

`app` 可以直接：

```text
redis:6379
```

访问 redis。

不需要知道 Redis 的容器 IP。

因此：

```text
container name / service name
        ↓
Docker DNS
        ↓
container IP
```

这部分对你以后玩 **ROS2 / DDS / SOME/IP** 会特别重要，因为 Docker 网络模式会直接影响组播、广播和 DDS discovery。

---

# 10. 你非常值得额外学：host 网络

学习笔记：[第九部分学习总结：host 网络与共享宿主机网络](doc/09-docker-host-network.md)。

Linux 上：

```bash
docker run --network host ...
```

意味着容器直接使用 host 网络。

普通模式：

```text
Container
172.17.0.2
   │
docker0
   │
Host
192.168.1.100
```

host 模式：

```text
Container
     │
     ↓
Host network
192.168.1.100
```

在以下场景非常常见：

```text
ROS2
DDS
SOME/IP
多播
自动驾驶 middleware
```

所以对你的技术方向来说，这部分优先级很高。

---

# 11. Docker 学习过程中建议做 5 个小项目

我建议不要纯看教程，直接完成下面几个实验：

1. **Ubuntu 开发环境**

```text
ubuntu:22.04
gcc
g++
cmake
```

2. **Nginx**

练习：

```text
docker run
port
volume
logs
```

3. **C++ Docker 开发环境**

目录：

```text
cpp-demo/
├── Dockerfile
├── CMakeLists.txt
└── src/
```

用 Docker 编译你的 C++ 项目。

本次实验总结见 [C++、CMake 与 Docker 开发环境](doc/10-cpp-docker-development.md)，涵盖绑定挂载、UID/GID、增量构建，以及 Compose 开发流程。练习目录为 `phase9-cpp-dev/`。

4. **Docker Compose**

跑：

```text
app
redis
```

理解：

```text
service
network
volume
depends_on
```

5. **ROS2 Docker**

本次实验总结见 [ROS2 Docker 多容器通信](doc/11-ros2-docker.md)，涵盖 DDS 发现、Domain ID 对照实验、Compose 外部网络和节点、话题、QoS 查询。练习配置为 `phase10-ros2/compose.yaml`。

使用包含演示节点的镜像，例如：

```dockerfile
FROM osrf/ros:humble-desktop
```

然后：

```bash
docker run -it osrf/ros:humble-desktop
```

运行：

```bash
ros2 run demo_nodes_cpp talker
```

再开一个 container：

```bash
ros2 run demo_nodes_cpp listener
```

研究两个 container 如何进行 DDS discovery。

这个项目非常适合你目前正在学习 ROS2 的阶段。

---

# 12. 推荐你的学习顺序

不要按 Docker 文档从第一页往后看。

我建议：

```text
第 1 天
│
├── image
├── container
├── docker run
├── docker ps
├── docker exec
└── docker logs
        ↓
第 2 天
│
├── -p port
├── -v volume
└── --network
        ↓
第 3 天
│
├── Dockerfile
├── docker build
└── layer/cache
        ↓
第 4 天
│
├── Docker Compose
└── 多容器通信
        ↓
第 5 天
│
├── Docker network
├── bridge
├── host
└── DNS
        ↓
第 6～7 天
│
└── ROS2 + Docker 实战
```

这样一周基本就能达到**日常开发可用**。

---

## 对你而言，优先级可以这样排

考虑到你最近主要在学 **ROS2、DDS、网络、多播，以及 C++**，我会建议：

```text
⭐⭐⭐⭐⭐ docker run
⭐⭐⭐⭐⭐ volume
⭐⭐⭐⭐⭐ network
⭐⭐⭐⭐⭐ --network host
⭐⭐⭐⭐⭐ Dockerfile

⭐⭐⭐⭐  Docker Compose
⭐⭐⭐⭐  image layer/cache

⭐⭐⭐    registry
⭐⭐     Docker Swarm
⭐      Docker 内部实现
```

暂时**不要学 Kubernetes**。

先达到：

> 能自己写 Dockerfile，把一个 ROS2/C++ 项目装进 Docker，并让多个 ROS2 容器正常通信。

做到这一步，你的 Docker 基础就已经比较扎实了。
