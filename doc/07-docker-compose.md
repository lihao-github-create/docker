# 第七部分：Docker Compose 与多服务管理

本节对应 [学习指南](../learn_guide.md) 中的“第六阶段：Docker Compose”。通过 Nginx、Redis、Redis 客户端和 C++ 程序，学习服务配置、容器通信、数据持久化、启动依赖，以及使用 Dockerfile 构建镜像。

练习文件为 [phase6-compose/compose.yaml](../phase6-compose/compose.yaml)，C++ 构建复用 [第五阶段的 Dockerfile](../phase5-dockerfile/Dockerfile)。下文按实验过程记录配置变化；练习文件目前保存的是本节末尾的完整配置。

## 1. Dockerfile 与 Compose 的职责

```text
Dockerfile → docker build → 镜像
                              │
compose.yaml → docker compose up
                              ↓
                  服务容器、网络、挂载等运行配置
```

- Dockerfile 描述如何构建镜像，例如安装依赖、复制源码、编译程序。
- Compose 描述应用包含哪些服务、使用或构建哪些镜像，以及端口、环境变量、命令、挂载和依赖关系。
- 一个服务是一份运行配置，可以对应一个或多个容器；本次每个服务只运行一个实例。

使用 `docker compose` 命令，中间有空格。练习前可用 `docker compose version` 检查是否可用。

本节宿主机命令均在以下目录执行：

```bash
cd ~/cpp_study/docker/phase6-compose
```

## 2. 实验一：统一启动 Nginx 与 Redis

初始 `compose.yaml`：

```yaml
services:
  web:
    image: nginx:stable
    ports:
      - "8083:80"

  redis:
    image: redis:7
```

YAML 使用空格缩进，不使用 Tab。`web` 和 `redis` 是服务名，`nginx:stable` 和 `redis:7` 是镜像名。

```bash
docker compose config
docker compose up -d
docker compose ps
docker compose exec redis redis-cli ping
docker compose logs --tail=20 redis
```

| 命令                     | 作用                                      |
| ------------------------ | ----------------------------------------- |
| `config`               | 解析并输出配置，帮助检查格式和实际配置    |
| `up -d`                | 准备并启动配置中的服务，后台运行          |
| `ps`                   | 查看服务容器的状态和端口                  |
| `exec redis ...`       | 在 Redis 服务已有且运行中的容器里执行命令 |
| `logs --tail=20 redis` | 查看 Redis 服务日志的最后 20 行           |

本次实际创建了 `phase6-compose_default` 网络，并启动了 `web` 和 `redis` 容器。`ps` 中两个服务均为 `Up`，Redis 的 `ping` 返回 `PONG`。

### 端口显示的区别

| 本次 ps 显示             | 含义                                         |
| ------------------------ | -------------------------------------------- |
| `0.0.0.0:8083->80/tcp` | Nginx 容器端口 80 映射到宿主机端口 8083      |
| `6379/tcp`             | Redis 镜像声明了容器端口，没有发布宿主机端口 |

网页访问地址为 `http://localhost:8083`，预期显示 Nginx 欢迎页面。本次提供的验证输出确认了容器运行和 Redis 响应，未提供浏览器访问结果。

Redis 日志中的 overcommit 提示涉及后台保存等操作，IPv6 监听也出现提示；后续显示 `Ready to accept connections`，并成功返回 `PONG`。这些提示没有阻止本次已验证的连接，本节未修改宿主机内核配置。

## 3. 实验二：通过服务名进行跨容器通信

在 `services` 下添加客户端：

```yaml
  client:
    image: redis:7
    command: ["sleep", "infinity"]
```

相同镜像可以承担不同角色：`redis` 使用默认命令运行服务器，`client` 使用 `command` 覆盖默认命令，运行 `sleep infinity`，供我们执行镜像自带的 `redis-cli`。

```bash
docker compose up -d client
docker compose exec client redis-cli -h redis -p 6379 ping
docker compose exec client redis-cli -h redis SET lesson compose-network
docker compose exec redis redis-cli GET lesson
```

本次实际结果依次为：

```text
PONG
OK
"compose-network"
```

```text
client 容器中的 redis-cli
          │ redis:6379
          │ Docker 内部 DNS 解析服务名
          ↓
redis 容器中的 Redis 服务器
```

默认情况下，本项目的服务加入同一个 Compose 网络，可以通过服务名互相访问。跨容器连接使用容器端口，不需要发布宿主机端口。

在 `client` 容器中，`localhost` 指向客户端自身，不指向 Redis 容器。客户端只运行 `sleep`，因此连接 Redis 应使用 `-h redis`。

服务名、容器名和 IP 是不同概念：

- `redis` 是配置中的服务名，也是本例连接使用的名称。
- `phase6-compose-redis-1` 是 Compose 生成的容器名。
- 容器 IP 可能在重建后变化，连接时不应依赖某个固定 IP。

## 4. 实验三：命名数据卷与 Redis 持久化

给 Redis 添加挂载，并在顶层声明数据卷：

```yaml
services:
  redis:
    image: redis:7
    volumes:
      - redis_data:/data

volumes:
  redis_data:
```

这是局部配置示意，应用时保留其他服务。两个 `volumes` 的含义不同：

- 顶层声明项目使用的命名卷 `redis_data`。
- 服务内将该卷挂载到 Redis 数据目录 `/data`。

首次添加这个新卷不会自动迁移前一个实验的数据，因此先应用配置，再重新写入练习键：

```bash
docker compose up -d
docker compose exec client redis-cli -h redis SET lesson compose-volume
docker compose exec redis redis-cli SAVE
docker compose exec redis ls -l /data
```

### 卷保留文件，Redis 仍需把内存数据写入文件

`SET` 修改 Redis 内存数据；`SAVE` 同步生成磁盘快照。本次少量练习数据用它来明确确认保存完成，不将其作为大型数据库的常规保存方案。

本次 `SAVE` 返回 `OK`，文件列表显示：

```text
-rw------- 1 redis redis 117 Oct 4 02:21 dump.rdb
```

```text
Redis 内存中的 lesson
          │ SAVE
          ↓
    /data/dump.rdb
          │ 挂载
          ↓
    命名数据卷 redis_data
```

仅挂载卷不意味着每次内存修改都已经写入磁盘，还需考虑应用的持久化机制。

### 删除并重建容器

```bash
docker compose down
docker volume ls
docker compose up -d
docker compose exec redis redis-cli ping
```

本次 `down` 删除了三个容器及默认网络，卷列表仍包含 `phase6-compose_redis_data`。重新启动后，Redis 返回 `PONG`。

随后读取：

```bash
docker compose exec client redis-cli -h redis GET lesson
```

实际输出：

```text
"compose-volume"
```

新 Redis 容器挂载了原来的卷，并加载保存的快照，完成了数据恢复验证。

默认项目名来自练习目录，所以本次卷名带有 `phase6-compose_` 前缀。若修改项目名，将使用另一组默认资源名。

`down` 默认保留命名卷；添加 `-v` 会删除配置中声明的非外部命名卷及附着的匿名卷。本次保留数据的实验不使用 `-v`。匿名卷默认也不会被 `down` 删除，但后续 `up` 不会自动重新挂载它们，因此持久化应使用明确的命名卷或绑定挂载。

## 5. 实验四：健康检查与启动依赖

容器已经启动，不代表其中的服务已经能响应请求。`depends_on` 简写控制启动顺序；等待服务就绪，需要健康检查和 `service_healthy` 条件。

Redis 添加检查，客户端改为启动后读取一次数据：

```yaml
  redis:
    image: redis:7
    volumes:
      - redis_data:/data
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 2s
      timeout: 1s
      retries: 5

  client:
    image: redis:7
    command: ["redis-cli", "-h", "redis", "GET", "lesson"]
    depends_on:
      redis:
        condition: service_healthy
```

| 配置                           | 含义                                                         |
| ------------------------------ | ------------------------------------------------------------ |
| `test`                       | 在 Redis 容器内执行检查；这里的 `CMD` 是直接执行命令的标记 |
| `interval: 2s`               | 检查间隔为 2 秒                                              |
| `timeout: 1s`                | 单次检查超时上限为 1 秒                                      |
| `retries: 5`                 | 连续失败 5 次后标记为 `unhealthy`                          |
| `condition: service_healthy` | 等待依赖的健康检查通过，再启动客户端                         |

```text
启动 Redis → 健康检查通过 → 启动 client → 读取数据 → client 退出
```

```bash
docker compose config
docker compose up -d
docker compose ps -a
docker compose logs --tail=10 client
```

本次实际观察：

| 服务       | 状态或日志                    |
| ---------- | ----------------------------- |
| `web`    | `Up`                        |
| `redis`  | `Up ... (healthy)`          |
| `client` | `Exited (0)`                |
| 客户端日志 | `client-1 \| compose-volume` |

`ps -a` 包含已退出的容器。客户端只执行一次读取任务，主进程完成后退出，退出码 `0` 表示成功。

`service_healthy` 控制启动时的等待。Redis 以后发生故障，不会仅因为这个条件就自动重启客户端；长期运行的应用仍需处理连接失败和重连。

## 6. 实验五：使用 Compose 构建 C++ 镜像

添加 `hello` 服务，复用第五阶段的 Dockerfile：

```yaml
  hello:
    image: cpp-hello:compose
    build:
      context: ../phase5-dockerfile
      args:
        DEFAULT_GREETING: "Hi"
    environment:
      GREETING: "Welcome"
    command: ["LiHao"]
```

| 配置              | 本例作用                                              |
| ----------------- | ----------------------------------------------------- |
| `build.context` | 相对于本例 Compose 文件目录，指定构建上下文           |
| `build.args`    | 给 Dockerfile 的 `ARG DEFAULT_GREETING` 传入 `Hi` |
| `image`         | 将构建的镜像命名为 `cpp-hello:compose`              |
| `environment`   | 在服务容器中设置 `GREETING=Welcome`                 |
| `command`       | 替换默认 `CMD`，作为参数传给原来的 `ENTRYPOINT`   |

```bash
docker compose config
docker compose build hello
docker compose up -d hello
docker compose ps -a hello
docker compose logs --tail=10 hello
docker run --rm cpp-hello:compose
```

指定服务名可只操作该服务。本次提供的运行输出确认 `hello` 为 `Exited (0)`，并验证了两种运行方式：

| 运行方式                   | 实际输出            |
| -------------------------- | ------------------- |
| Compose 的 hello 服务      | `Welcome, LiHao!` |
| 直接 docker run 同一个镜像 | `Hi, Docker!`     |

原因是两套配置处于不同阶段：

```text
构建时：DEFAULT_GREETING=Hi → 镜像默认 GREETING=Hi
                            镜像默认 CMD 为 Docker

Compose 运行时：GREETING=Welcome，command 为 LiHao
                            ↓
                     Welcome, LiHao!
```

直接运行镜像不会应用 Compose 文件中的运行配置，所以使用镜像的默认环境变量和参数。

## 7. 实验六：更新配置与运行临时任务

### 修改运行环境，无需重新编译

将 `hello.environment.GREETING` 改为 `你好`：

```yaml
    environment:
      GREETING: "你好"
```

```bash
docker compose up -d hello
docker compose logs --tail=10 hello
```

实际日志：

```text
hello-1 | 你好, LiHao!
```

程序在运行时读取环境变量，因此这次变化无需重新编译，也没有修改镜像。`up` 应用新配置，必要时重建服务容器。

### 根据服务配置创建临时容器

```bash
docker compose run --rm hello ZhangSan
```

实际创建了名称带有 `hello-run-` 的新容器，并输出：

```text
你好, ZhangSan!
```

它使用 `hello` 服务的环境配置，用 `ZhangSan` 替换配置中的 `command`，传给 `/workspace/hello`，结束后自动删除该临时容器。

### 常用命令的区别

| 命令                                    | 用途                                           |
| --------------------------------------- | ---------------------------------------------- |
| `docker compose up -d`                | 应用服务配置并后台启动，必要时重建容器         |
| `docker compose up -d --build hello`  | 先构建镜像，再更新并启动指定服务               |
| `docker compose restart hello`        | 重启已有容器，不应用文件中的环境变量等配置变化 |
| `docker compose exec redis ...`       | 在已有且运行中的服务容器内执行命令             |
| `docker compose run --rm hello ...`   | 根据服务配置创建新容器，执行一次任务后删除     |
| `docker compose ps -a`                | 查看运行中和已退出的服务容器                   |
| `docker compose logs --tail=10 hello` | 查看指定服务最近的日志                         |
| `docker compose down`                 | 停止并删除项目容器和网络，默认保留命名卷及镜像 |

`hello` 执行后已退出，不能再对它执行 `exec`；再次运行任务可以使用 `run`。`run` 默认不发布服务配置中的端口，本节的 `hello` 不需要端口映射。

## 8. 最终 compose.yaml

```yaml
services:
  web:
    image: nginx:stable
    ports:
      - "8083:80"

  redis:
    image: redis:7
    volumes:
      - redis_data:/data
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 2s
      timeout: 1s
      retries: 5

  client:
    image: redis:7
    command: ["redis-cli", "-h", "redis", "GET", "lesson"]
    depends_on:
      redis:
        condition: service_healthy

  hello:
    image: cpp-hello:compose
    build:
      context: ../phase5-dockerfile
      args:
        DEFAULT_GREETING: "Hi"
    environment:
      GREETING: "你好"
    command: ["LiHao"]

volumes:
  redis_data:
```

如果在没有练习数据的新环境中复现最终配置，客户端读取 `lesson` 不会得到 `compose-volume`。先启动 Redis，待它能响应后写入并保存，再启动读取任务：

```bash
docker compose build hello
docker compose up -d web redis
docker compose exec redis redis-cli ping
# 确认返回 PONG 后继续
docker compose exec redis redis-cli SET lesson compose-volume
docker compose exec redis redis-cli SAVE
docker compose up -d client hello
docker compose ps -a
docker compose logs --tail=10 client hello
```

`web` 提供网页，`client` 访问 Redis，`hello` 输出问候语；本次没有让 Nginx 或 C++ 程序访问 Redis。

## 9. 本节结论

1. 服务名与镜像名不同，Compose 用服务名组织容器及相关资源。
2. 默认网络中的服务可通过服务名和容器端口通信，无需发布宿主机端口。
3. 容器内的 `localhost` 指向该容器自身。
4. 命名卷保留磁盘文件，应用仍需将内存数据保存到磁盘。
5. `depends_on` 配合健康检查，可以等待依赖就绪再启动服务。
6. `build.args` 配置镜像构建，`environment` 和 `command` 配置容器运行。
7. `up` 应用配置变化，`restart` 只重启已有容器，`exec` 使用运行中的容器，`run` 创建新容器执行任务。
8. 一次性服务显示 `Exited (0)` 可以是正常完成，不代表故障。

后续学习：Docker 网络，进一步理解 bridge、host 模式、DNS 与多容器通信。

官方参考：[Compose 应用模型](https://docs.docker.com/compose/intro/compose-application-model/)、[Compose 网络](https://docs.docker.com/compose/how-tos/networking/)、[命名数据卷](https://docs.docker.com/reference/compose-file/volumes/)、[启动依赖与健康检查](https://docs.docker.com/compose/how-tos/startup-order/)、[构建配置](https://docs.docker.com/reference/compose-file/build/)、[服务配置](https://docs.docker.com/reference/compose-file/services/)、[down](https://docs.docker.com/reference/cli/docker/compose/down/)、[restart](https://docs.docker.com/reference/cli/docker/compose/restart/)、[run](https://docs.docker.com/reference/cli/docker/compose/run/)、[Redis SAVE](https://redis.io/docs/latest/commands/save/)。
