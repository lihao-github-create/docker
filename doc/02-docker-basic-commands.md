# 第二部分：Docker 基础命令与容器生命周期

本节对应 `learn_guide.md` 中的“第一阶段：只学 10 个命令”。通过 Ubuntu 容器练习下载镜像、创建、查看、进入、停止、启动和删除容器。

## 1. 命令速查

以下命令在宿主机的 WSL 终端执行；进入容器后的操作会单独标注。

| 命令                                                    | 作用                                   |
| ------------------------------------------------------- | -------------------------------------- |
| `docker version`                                      | 查看客户端和服务端版本，检查服务端连接 |
| `docker images`                                       | 查看本地镜像                           |
| `docker ps`                                           | 查看运行中的容器                       |
| `docker ps -a`                                        | 查看所有容器，包括已停止的             |
| `docker pull ubuntu:22.04`                            | 下载指定镜像                           |
| `docker run -it --name ubuntu_test ubuntu:22.04 bash` | 创建并启动新容器，交互运行 Bash        |
| `docker exec -it ubuntu_test bash`                    | 在运行中的容器里启动新的 Bash          |
| `docker logs ubuntu_test`                             | 查看容器的标准输出和错误日志           |
| `docker stop ubuntu_test`                             | 停止容器                               |
| `docker rm ubuntu_test`                               | 删除已停止的容器                       |
| `docker start -ai ubuntu_test`（补充）                | 启动已有容器，并连接输入、输出         |

## 2. 检查 Docker：客户端与服务端

```bash
docker --version
docker version
```

两条命令不同：

- `docker --version` 只显示客户端版本，通常只有一行，不检查服务端连接。
- `docker version` 分别显示 Client 和 Server 信息；连接失败时会显示错误。

```text
用户输入 docker 命令
        ↓
Client：发送请求
        ↓
Server（dockerd）：管理镜像和容器
        ↓
返回结果
```

本次练习中，Client 和 Server 最终都显示版本 `29.1.3`，且普通用户可以连接服务端。其余组件版本暂时不需要记忆。

## 3. 下载并查看镜像

在宿主机执行：

```bash
docker pull ubuntu:22.04
docker images
```

- `ubuntu` 是镜像名称，`22.04` 是标签。
- `Pull complete` 表示对应镜像层下载完成。
- `Digest: sha256:...` 是内容摘要，用来标识和校验镜像内容。
- 下载镜像不会创建或启动容器。

本次 `docker images` 的输出使用 `IMAGE`、`ID`、`DISK USAGE`、`CONTENT SIZE` 等列。不同版本可能使用 `REPOSITORY`、`TAG`、`IMAGE ID`、`SIZE` 等布局，不必要求输出与教程逐字一致。

## 4. 创建并启动第一个容器

在宿主机执行：

```bash
docker run -it --name ubuntu_test ubuntu:22.04 bash
```

| 部分                   | 含义                 |
| ---------------------- | -------------------- |
| `run`                | 创建并启动一个新容器 |
| `-i`                 | 保持标准输入打开     |
| `-t`                 | 分配终端，便于交互   |
| `--name ubuntu_test` | 指定容器名称         |
| `ubuntu:22.04`       | 使用的镜像           |
| `bash`               | 容器启动时运行的程序 |

进入容器后的提示符示例：

```text
root@241e24c9a2c1:/#
```

`root` 是容器内用户，`241e24c9a2c1` 是默认主机名（通常为容器 ID 的前 12 位），`/` 是当前目录。

在容器内执行：

```bash
cat /etc/os-release
```

本次输出为 `Ubuntu 22.04.2 LTS`。这描述的是容器的用户空间系统文件；Linux 容器共享运行它们的 Linux 内核。

## 5. 查看运行状态，理解退出与停止

保持容器终端打开，在另一个相同 WSL 环境的宿主机终端执行：

```bash
docker ps
```

本次看到容器 `241e24c9a2c1`，名称为 `ubuntu_test`，命令为 `bash`，状态为 `Up`。

回到容器的主 Bash 执行：

```bash
exit
```

再在宿主机执行：

```bash
docker ps
docker ps -a
```

观察结果：

- `docker ps` 不再显示该容器。
- `docker ps -a` 仍显示它，状态为 `Exited (0)`。
- `0` 通常表示正常退出。

本例中 Bash 是容器主进程。主 Bash 退出后，容器停止，但没有被删除。

## 6. 重新启动同一个容器，验证文件保留

在宿主机执行：

```bash
docker start -ai ubuntu_test
```

`start` 启动已有容器；`-a` 连接标准输出和错误输出，`-i` 连接标准输入。

在容器内创建文件并退出：

```bash
echo "hello docker" > /root/hello.txt
cat /root/hello.txt
exit
```

在宿主机再次启动：

```bash
docker start -ai ubuntu_test
```

进入后读取：

```bash
cat /root/hello.txt
```

实际仍输出 `hello docker`，且容器 ID 没变。

**结论：停止再启动保留容器的可写层。`run` 创建新容器，`start` 启动已有容器。**

## 7. 用 exec 打开第二个 Shell

保持主 Bash 运行，在另一个宿主机终端执行：

```bash
docker exec -it ubuntu_test bash
```

进入第二个 Bash 后执行：

```bash
cat /root/hello.txt
exit
```

回到宿主机后执行：

```bash
docker ps
```

实际结果：文件可以读取，退出第二个 Bash 后，容器仍是 `Up`。

```text
同一个容器 ubuntu_test
├── 主 Bash：容器启动时运行
└── 第二个 Bash：docker exec 创建
```

| 操作            | 结果                                      |
| --------------- | ----------------------------------------- |
| 退出第二个 Bash | 只结束这个 Bash，主进程仍在，容器继续运行 |
| 退出主 Bash     | 容器停止，第二个 Bash 也随之终止          |

练习中曾先退出主 Bash，导致第二个终端也返回宿主机。再次保持主 Bash 打开、只退出第二个 Bash 后，验证了两者的区别。

`docker exec` 要求目标容器正在运行；它创建新进程，不创建新容器。

## 8. 查看日志与停止容器

在宿主机执行：

```bash
docker logs ubuntu_test
docker stop ubuntu_test
docker ps -a
```

### 日志

`docker logs` 读取 Docker 为容器记录的标准输出和标准错误。当前交互式 Bash 的日志中，可以看到提示符、终端回显和命令输出，也能看到同一容器多次启动产生的历史输出。

它不会自动读取容器内所有日志文件，`docker exec` 会话的输出也不会自动进入这份日志。

### 停止和退出码

`docker stop` 请求容器停止；如果超时仍未退出，会强制终止。

本次结果为 `Exited (137)`：`137 = 128 + 9`，通常表示进程被 `SIGKILL` 结束。结合操作过程，很可能是主 Bash 没有及时退出，随后被强制终止；本次没有进一步核查原因，不能仅凭 `137` 认定是内存不足。

停止后，容器和可写层仍保留。

## 9. 删除并重建，验证镜像与容器的区别

本步骤会删除练习容器及其可写层中的 `/root/hello.txt`。

在宿主机执行：

```bash
docker rm ubuntu_test
docker ps -a
docker images
```

实际结果：容器列表为空，`ubuntu:22.04` 镜像仍然存在。

再创建同名新容器：

```bash
docker run -it --name ubuntu_test ubuntu:22.04 bash
```

进入后执行：

```bash
cat /root/hello.txt
```

实际输出：

```text
cat: /root/hello.txt: No such file or directory
```

旧容器 ID 是 `241e24c9a2c1`，新容器 ID 是 `8cb1dc913ce2`。

**结论：同名不代表同一个容器。删除容器不会删除镜像，也不会把容器的修改写回镜像。新容器不继承旧容器的可写层。**

## 10. 本次 WSL2 环境排障记录

### 已安装客户端，但服务端无法连接

最初 `docker version` 报错：

```text
/var/run/docker.sock: connect: no such file or directory
```

这表示客户端无法通过该 Unix socket 连接服务端，不代表必须重新安装 Docker。

随后进行了只读检查：

```bash
systemctl status docker --no-pager
uname -r
ps -p 1 -o comm=
command -v dockerd
```

确认当时运行于 WSL2，PID 1 不是 systemd，且 `/usr/bin/dockerd` 已存在。因此 `systemctl` 当时无法管理服务。

手动启动方式：

```bash
sudo dockerd
```

它在前台运行，需要保持终端打开。在另一个终端执行 `docker version` 后，成功看到 Server 信息。

systemd 便于管理服务，但并非手动运行 Docker 的必要条件。若要启用，官方配置是在 `/etc/wsl.conf` 中的 `[boot]` 节设置 `systemd=true`，再重启 WSL。

参考：[wsl2-docker-安装指导.md](wsl2-docker-安装指导.md)。

### 服务端正常，但拉取镜像失败

最初 `docker pull ubuntu:22.04` 连接 Docker Hub 被拒绝；WSL 中使用 `curl` 直连仓库也超时。

需要区分：

```text
客户端 → 本机 dockerd：docker version 验证
本机 dockerd → 镜像仓库：docker pull 验证
```

代理和镜像加速器是不同配置。需要代理时，应让发起下载的 `dockerd` 使用代理；终端或 Windows 浏览器能上网不代表服务端已经配置好代理。

用户参考教程调整配置后，最终成功拉取镜像。由于没有提供最终生效的配置及代理端口，本节只记录“下载成功”，不将解决原因归结为某个特定镜像加速器、DNS 或代理设置。

参考：[wsl2-docker-安装指导.md](wsl2-docker-安装指导.md)。

## 11. 本节总结

```text
镜像 ──docker run──→ 新容器（运行）
                       │
                  docker stop
                  或主进程退出
                       ↓
                   容器（停止）
                       ├── docker start → 同一个容器再次运行
                       └── docker rm    → 容器及其可写层删除
```

记住三点：

1. `run` 创建新容器，`start` 启动已有容器，`exec` 在运行中的容器里创建新进程。
2. 停止保留可写层，删除容器会删除可写层；原镜像不会因此改变。
3. 主进程决定容器是否继续运行，退出第二个 Shell 不等于退出主进程。

练习结束后，可在新容器中输入 `exit`，停止并保留它。

下一部分：深入理解 `docker run`，学习后台运行 `-d` 与交互运行 `-it`。
