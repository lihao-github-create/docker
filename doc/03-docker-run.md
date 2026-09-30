# 第三部分：重点理解 docker run

本节对应 [学习指南](../learn_guide.md) 中的“第二阶段：重点理解 docker run”。通过一次性命令、后台循环和 Nginx 服务，理解启动命令、交互终端、后台运行与主进程的关系。

除明确标注外，以下命令均在 WSL 宿主机终端执行。实验容器名称如果已被占用，需换一个名称，或先处理已有容器；停止的容器也会占用名称。

## 1. 命令结构与常用参数

```bash
docker run [Docker 选项] 镜像 [容器内命令] [命令参数]
```

例如：

```bash
docker run -it --name ubuntu_test ubuntu:22.04 bash
```

| 部分 | 作用 |
|---|---|
| `run` | 创建并启动一个新容器 |
| `-i` | 保持标准输入打开 |
| `-t` | 分配伪终端，方便交互 |
| `-d` | 后台运行，启动后返回宿主机终端 |
| `--name ubuntu_test` | 指定容器名称 |
| `ubuntu:22.04` | 使用的镜像 |
| `bash` | 显式指定的启动命令 |

`-it` 是 `-i` 与 `-t` 的组合。`-d` 和 `-it` 不是互斥选项，但它们解决不同问题：是否保持交互输入、分配终端，以及客户端是否持续连接容器。

Docker 选项放在镜像名称之前。镜像名称后面的内容属于容器命令及参数；实际启动方式还受镜像的 `ENTRYPOINT` 配置影响，后续学习 Dockerfile 时再展开。

## 2. 实验一：执行一次命令后退出

```bash
docker run --name hello_once ubuntu:22.04 echo "hello docker"
docker ps -a --filter name=hello_once
```

实际观察：

- 终端输出 `hello docker`，随后返回宿主机提示符。
- 容器状态为 `Exited (0)`。
- `--filter name=hello_once` 按名称筛选，方便查看本次实验。

本例中主进程是 `echo`，它执行完毕后正常退出，容器随之停止。这个任务不需要交互输入，因此没有使用 `-it`。

## 3. 实验二：加上 -d，也不会让一次性命令持续运行

```bash
docker run -d --name hello_background ubuntu:22.04 echo "hello docker"
docker logs hello_background
docker ps -a --filter name=hello_background
```

实际观察：

| 检查项 | 结果 |
|---|---|
| `docker run -d` 的输出 | 一串容器 ID |
| `docker logs` 的输出 | `hello docker` |
| 容器状态 | `Exited (0)` |

两次实验的主进程相同，都会很快结束。区别是：

- 默认前台连接时，客户端连接容器输出，等待程序结束。
- 使用 `-d` 时，客户端在启动后返回终端，程序输出可通过日志查看。

**`-d` 不会让已结束的程序继续存活，也不保证容器一直处于运行状态。**

## 4. 实验三：持续运行的后台容器

使用 Ubuntu 镜像启动一个持续输出的循环：

```bash
docker run -d --name ticker ubuntu:22.04 \
  bash -c 'while true; do echo "tick"; sleep 3; done'
```

| 部分 | 含义 |
|---|---|
| `bash -c '...'` | 让 Bash 执行引号中的脚本 |
| `while true; do ...; done` | 持续循环 |
| `echo "tick"` | 输出一行文本 |
| `sleep 3` | 等待 3 秒 |

命令中的行末反斜杠用于续行；其后不要再加空格。单引号将脚本作为一个参数传给容器中的 Bash。

查看状态与实时日志：

```bash
docker ps --filter name=ticker
docker logs -f ticker
```

实际看到容器为 `Up`，日志每隔约 3 秒新增一行 `tick`。`-f` 表示持续跟踪日志。

按 `Ctrl+C` 结束日志跟踪后，再执行：

```bash
docker ps --filter name=ticker
```

容器仍然为 `Up`。

**这里结束的是宿主机上的 `docker logs -f`，容器内的主 Bash 仍在执行循环。** 这一结论针对日志跟踪命令，不应推广为“任何场景按 Ctrl+C 都不会影响容器”。

## 5. 实验四：后台主进程与交互式检查可以同时存在

保持 `ticker` 运行，执行：

```bash
docker exec -it ticker bash
```

在容器内执行：

```bash
echo "这是第二个 Bash"
exit
```

回到宿主机后查看最近三行日志：

```bash
docker logs --tail 3 ticker
```

实际输出：

```text
tick
tick
tick
```

两个进程的关系：

```text
ticker 容器
├── 主 Bash：持续循环输出 tick
└── exec 创建的 Bash：供交互检查，exit 后结束
```

- `run -d` 创建容器，让主进程在后台运行。
- `exec -it` 在同一个运行中的容器内创建新的交互进程。
- 退出新增 Bash 不会结束主 Bash。
- 新增 Bash 的输出显示在交互终端中，不会自动进入主进程的日志。
- `--tail 3` 限制本次显示的日志行数，不会删除历史日志。

实验结束后，可在宿主机停止循环：

```bash
docker stop ticker
```

## 6. 实验五：使用镜像默认命令启动 Nginx

```bash
docker run -d --name nginx_demo nginx
```

未指定标签时，默认使用 `nginx:latest`。本次本地没有这个镜像，Docker 先完成下载，再创建并启动容器。

随后检查：

```bash
docker ps --filter name=nginx_demo
docker logs --tail 10 nginx_demo
```

本次观察到：

| 项目 | 结果 |
|---|---|
| 容器 ID | `2fe2d7a32d13` |
| 状态 | `Up` |
| 启动命令 | 显示为 `nginx -g 'daemon of…` |
| 端口列 | `80/tcp` |
| 日志 | 没有输出 |

### 为什么不写 bash 也能运行？

镜像可以定义默认启动程序。没有提供容器命令时，Docker 使用镜像中的默认启动配置。本次 Nginx 镜像会启动 Nginx 服务。

对比：

```bash
# 显式指定运行 Bash
docker run -it --name ubuntu_shell ubuntu:22.04 bash

# 使用镜像的默认启动配置
docker run -d --name nginx_demo nginx
```

以上是行为对比，不需要重复创建已经存在的 `nginx_demo`。

### Docker 后台运行与 Nginx 前台运行

Nginx 的 `daemon off;` 配置让它在容器内保持前台运行。

| 设置 | 作用层次 |
|---|---|
| Docker 的 `-d` | 容器启动后，宿主机终端返回提示符 |
| Nginx 的 `daemon off;` | 服务在容器内前台运行，保持主进程存活 |

两者并不矛盾。后台运行容器时，仍需要一个持续运行的主进程。

### 日志为空和端口列分别意味着什么？

- 日志为空，表示本次查询没有可显示的日志，不能据此判定启动失败。
- `Up` 说明容器正在运行；服务能否响应请求，还需要实际访问验证。
- `80/tcp` 表示容器声明了该端口，不代表已经建立宿主机端口映射。本次启动没有使用 `-p`。

## 7. 进入 Nginx 容器检查配置

在宿主机执行：

```bash
docker exec -it nginx_demo sh
```

`sh` 可用于提供该 Shell 的镜像；部分镜像没有 Bash，因此进入前应注意镜像内有哪些程序。

进入后执行：

```sh
nginx -t
exit
```

本次配置测试输出：

```text
nginx: the configuration file /etc/nginx/nginx.conf syntax is ok
nginx: configuration file /etc/nginx/nginx.conf test is successful
```

`nginx -t` 检查配置语法，并尝试打开配置引用的文件。它不等于完成了 HTTP 访问测试。

退出后，在宿主机执行：

```bash
docker ps --filter name=nginx_demo
```

实际仍为 `Up`，再次验证退出检查用的 Shell 不会结束 Nginx 主进程。

## 8. 本节总结

| 需求 | 常用方式 |
|---|---|
| 执行一次任务 | `docker run 镜像 命令 参数` |
| 创建交互式 Shell | `docker run -it 镜像 bash`（镜像需包含 Bash） |
| 后台启动服务 | `docker run -d --name 名称 镜像` |
| 进入运行中的容器检查 | `docker exec -it 名称 sh` |
| 持续观察日志 | `docker logs -f 名称` |
| 只看最近几行日志 | `docker logs --tail 10 名称` |

记住四点：

1. `run` 创建新容器，`exec` 在运行中的容器内启动新进程。
2. `-i`、`-t` 和 `-d` 分别控制输入、终端和后台运行方式。
3. 容器运行多久，取决于主进程运行多久；加 `-d` 不会改变这一点。
4. 未提供命令时使用镜像默认启动配置；`Up`、日志和实际服务响应需要分别判断。

下一部分学习端口映射 `-p`：建立“宿主机 8080 → 容器 80”的转发，并从浏览器访问 Nginx。
