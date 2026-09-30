# 第四部分：端口映射

本节对应 [学习指南](../learn_guide.md) 中的“第三个重点：端口映射”。通过两个 Nginx 容器，验证浏览器访问、端口独立性，以及停止再启动后映射的保留。

## 1. 核心概念：宿主机端口转发到容器端口

```bash
-p 8080:80
#  宿主机端口:容器端口
```

Nginx 仍在容器内监听 `80`，Docker 将到达宿主机 `8080` 的请求转发过去。端口映射不会把 Nginx 的监听端口改成 `8080`。

本次在 WSL2 中运行 Docker，实际验证的访问路径是：

```text
Windows 浏览器：http://localhost:8080
                    ↓
         WSL 中的宿主机端口 8080
                    ↓ Docker 端口映射
          nginx_port 容器的端口 80
                    ↓
                  Nginx
```

WSL 终端中的 `curl` 也可以访问同一入口。本次 Windows 到 WSL 的 localhost 访问已经成功，其他环境还需结合其 WSL 网络配置验证。

## 2. 创建带端口映射的容器

以下命令在 WSL 宿主机终端执行。实验使用的容器名和宿主机端口应未被占用；已经完成实验时，不要重复执行同名 `docker run`。

```bash
docker run -d \
  --name nginx_port \
  -p 8080:80 \
  nginx
```

| 参数 | 作用 |
|---|---|
| `-d` | 后台运行 |
| `--name nginx_port` | 指定容器名称 |
| `-p 8080:80` | 把宿主机的 TCP 8080 端口映射到容器的 TCP 80 端口 |
| `nginx` | 使用 Nginx 镜像的默认启动配置 |

之前没有端口映射的 `nginx_demo` 可以继续运行。两个容器拥有各自的网络环境，都能使用自己的 `80` 端口。

检查新容器：

```bash
docker ps --filter name=nginx_port
```

本次观察到容器 ID 为 `6186d3f4e0cd`，状态为 `Up`，`PORTS` 列为：

```text
0.0.0.0:8080->80/tcp
```

它表示宿主机所有 IPv4 网络接口上的 `8080` 被映射到容器的 TCP `80`。`0.0.0.0` 表示绑定范围；实际访问时使用 `localhost` 或可达的宿主机地址。

对比上一节：

| PORTS 列 | 含义 |
|---|---|
| `80/tcp` | 声明了容器端口，没有显示宿主机端口映射 |
| `0.0.0.0:8080->80/tcp` | 已建立宿主机 8080 到容器 80 的映射 |

## 3. 从 WSL 和 Windows 验证访问

### WSL 中检查 HTTP 响应

```bash
curl -I http://localhost:8080
```

`-I` 请求 HTTP 响应头，不显示网页正文。本次响应包含：

```text
HTTP/1.1 200 OK
Server: nginx/1.13.8
Content-Type: text/html
Content-Length: 612
```

这说明通过映射已经收到 Nginx 的 HTTP 响应。`Server` 的版本号是本次实验输出，不代表其他时间下载的镜像也使用此版本。

### Windows 浏览器访问

在地址栏输入：

```text
http://localhost:8080
```

本次浏览器成功显示 `Welcome to nginx!` 欢迎页，验证了从 Windows 经 WSL 到容器服务的完整访问路径。

## 4. 第二个容器使用不同的宿主机端口

创建另一个 Nginx 容器：

```bash
docker run -d --name nginx_port2 -p 8081:80 nginx
```

查看映射可使用专门的命令：

```bash
docker port nginx_port
docker port nginx_port2
```

对应关系：

```text
localhost:8080 → nginx_port  的 80
localhost:8081 → nginx_port2 的 80
```

两个容器内部都监听 `80`，但宿主机分别使用 `8080` 和 `8081`。在本例相同宿主机绑定地址及 TCP 协议下，不能让两个同时运行的容器都占用 `8080`。

本次访问 `http://localhost:8081` 同样显示 Nginx 欢迎页。两个容器使用相同镜像和默认网页，因此页面相同不代表它们是同一个容器。

## 5. 停止一个容器，另一个仍可访问

在 WSL 执行：

```bash
docker stop nginx_port
curl -I --connect-timeout 3 http://localhost:8080
curl -I --connect-timeout 3 http://localhost:8081
```

`--connect-timeout 3` 将建立连接阶段的等待时间限制为最多 3 秒，不是整个 HTTP 请求的总时限。

本次结果：

| 地址 | 实际结果 |
|---|---|
| `http://localhost:8080` | 连接超时，无法访问 |
| `http://localhost:8081` | `HTTP/1.1 200 OK`，浏览器仍可显示欢迎页 |

停止容器后，原服务不再通过其映射提供访问；具体错误可能因环境表现为超时或连接被拒绝。

**两条映射分别指向自己的容器。停止 `nginx_port` 不会停止 `nginx_port2`。**

## 6. 重新启动，原端口映射保留

```bash
docker start nginx_port
docker port nginx_port
curl -I --connect-timeout 3 http://localhost:8080
```

本次映射输出：

```text
80/tcp -> 0.0.0.0:8080
```

HTTP 响应重新变为：

```text
HTTP/1.1 200 OK
```

**端口映射属于容器配置，停止再启动同一个容器会保留它，不需要重新指定 `-p`。** 启动时对应宿主机端口仍需可用。

`docker start` 不能用来重新指定 `-p`。如果希望修改已有容器的端口映射，通常需要按新配置重新创建容器；创建前应处理需要保留的数据。

## 7. 命令速查与本节结论

| 命令 | 作用 |
|---|---|
| `docker run -d --name nginx_port -p 8080:80 nginx` | 创建带映射的 Nginx 容器 |
| `docker ps --filter name=nginx_port` | 查看运行状态及端口列 |
| `docker port nginx_port` | 查看容器的端口映射 |
| `curl -I http://localhost:8080` | 检查 HTTP 响应头 |
| `docker stop nginx_port` | 停止该容器 |
| `docker start nginx_port` | 启动同一个容器，保留原配置 |

本次完整验证过程：

```text
创建容器并指定 -p 8080:80
          ↓
WSL curl 与 Windows 浏览器访问成功
          ↓
第二个容器使用 8081:80，也可独立访问
          ↓
停止第一个容器 → 8080 无法访问，8081 仍可访问
          ↓
启动第一个容器 → 原映射保留，8080 恢复访问
```

记住五点：

1. `-p` 的顺序是“宿主机端口:容器端口”。
2. 不同容器可以使用相同内部端口，通过不同宿主机端口访问。
3. 声明 `80/tcp` 不等于建立宿主机端口映射。
4. 停止再启动保留映射配置，修改映射通常需要重建容器。
5. 映射只负责转发，容器内必须有服务监听对应端口，且网络允许访问。

本次最后已恢复 `nginx_port`，并确认 `8080` 返回 `200 OK`；`nginx_port2` 也已验证可通过 `8081` 访问。

下一部分：数据卷与绑定挂载。将宿主机网页目录挂载到 Nginx 容器，替换默认欢迎页，并验证删除容器后网页文件仍然保留。
