# 第五部分：Volume 与绑定挂载

本节对应 [学习指南](../learn_guide.md) 中的“第四个重点：Volume”。通过挂载 Nginx 网页和跨容器读写命名数据卷，理解数据如何独立于容器保存。

## 1. 三种存储方式

| 方式 | 数据位置 | 删除容器后 | 常见用途 |
|---|---|---|---|
| 容器可写层 | 容器自身的可写层 | 随容器删除 | 临时文件 |
| Bind mount（绑定挂载） | 你指定的宿主机目录 | 宿主机文件保留 | 源码、网页、配置文件 |
| Named volume（命名数据卷） | Docker 管理的存储位置 | 卷保留，直到另外删除 | 数据库等应用数据 |

指南中使用宿主机绝对路径的 `-v` 示例属于绑定挂载。`-v` 可以用于这两种挂载，具体取决于左侧是宿主机路径还是卷名：

```text
-v /宿主机绝对路径:/容器路径  → 绑定挂载
-v 卷名:/容器路径            → 命名数据卷
```

挂载不是把文件复制一份到容器可写层，而是让容器通过指定路径访问外部存储。

## 2. 实验一：把宿主机网页交给容器中的 Nginx

以下命令均在 WSL 宿主机的 `~/cpp_study/docker` 目录执行。重做实验前，应确认容器名和端口未被占用；创建网页的命令会覆盖同名文件。

### 创建网页

```bash
mkdir -p nginx-html
cat > nginx-html/index.html <<'HTML'
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <title>Docker 挂载实验</title>
</head>
<body>
  <h1>这是宿主机上的网页</h1>
  <p>由 Docker 容器中的 Nginx 提供服务。</p>
</body>
</html>
HTML
```

### 挂载目录并启动容器

```bash
docker run -d \
  --name nginx_mount \
  -p 8082:80 \
  -v "$(pwd)/nginx-html:/usr/share/nginx/html:ro" \
  nginx
```

| 部分 | 含义 |
|---|---|
| `$(pwd)/nginx-html` | 宿主机目录的绝对路径，`pwd` 输出当前目录 |
| `/usr/share/nginx/html` | 本次 Nginx 默认网页所在的容器目录 |
| `ro` | 容器通过此挂载只能读取，不能写入 |
| `-p 8082:80` | 宿主机端口 8082 映射到容器端口 80 |

```text
宿主机 nginx-html/index.html
               ↓ 绑定挂载
容器 /usr/share/nginx/html/index.html
               ↓ Nginx
浏览器 http://localhost:8082
```

挂载期间，容器该目录原有的文件被遮住，不会因此从镜像中删除。

本次 Windows 浏览器访问 `http://localhost:8082`，成功显示“这是宿主机上的网页”。

## 3. 实验二：修改宿主机文件，无需重启容器

在宿主机执行：

```bash
sed -i 's/这是宿主机上的网页/修改立即可见，无需重启容器/' nginx-html/index.html
```

刷新浏览器后，标题变为“修改立即可见，无需重启容器”。若浏览器仍显示缓存内容，可以使用 `Ctrl+F5` 强制刷新。

也可以从容器中读取挂载文件：

```bash
docker exec nginx_mount cat /usr/share/nginx/html/index.html
```

这是一次性命令，不需要交互输入，因此无需 `-it`。

**本次更新静态网页不需要重建镜像或重启容器，因为 Nginx 读取的是挂载的宿主机文件。** 其他应用是否自动应用文件变化，仍取决于其读取、缓存或重新加载机制。

### 为什么有 ro，宿主机仍能修改？

`:ro` 限制容器通过这个挂载写入，不会把宿主机目录本身改为只读。宿主机上的用户仍按原有文件权限编辑文件。

## 4. 实验三：验证只读挂载

在宿主机执行：

```bash
docker exec nginx_mount sh -c \
  'echo "test" > /usr/share/nginx/html/write-test.txt'
```

实际输出：

```text
sh: 1: cannot create /usr/share/nginx/html/write-test.txt: Read-only file system
```

这验证了容器不能通过该挂载创建文件。

这里用 `sh -c` 并将脚本放在单引号内，让 `>` 重定向由容器中的 Shell 执行；否则重定向可能被宿主机 Shell 处理。

如果省略 `:ro`，挂载默认可读写，但仍需满足文件权限。可读写绑定挂载中，容器修改或删除文件，也会影响宿主机上的同一份文件，因此挂载不是备份。

## 5. 实验四：删除容器，文件仍然保留

停止并删除练习容器：

```bash
docker stop nginx_mount
docker rm nginx_mount
```

在宿主机检查：

```bash
cat nginx-html/index.html
```

实际文件仍存在，标题仍是修改后的内容。停止或删除容器后，浏览器暂时无法访问网页。

再创建一个容器并挂载相同目录：

```bash
docker run -d \
  --name nginx_mount \
  -p 8082:80 \
  -v "$(pwd)/nginx-html:/usr/share/nginx/html:ro" \
  nginx
```

本次新容器 ID 为 `309a2040eed3…`。刷新浏览器后，修改过的网页重新可见。

**文件保留与服务运行是两回事：文件在宿主机上保留，但仍需要运行中的 Nginx 才能通过浏览器提供服务。** 新容器读到的是同一个宿主机目录中的数据。

## 6. 实验五：跨容器使用命名数据卷

绑定挂载由用户指定宿主机路径；命名数据卷只需指定卷名，存储位置由 Docker 管理。

### 创建数据卷

```bash
docker volume create study_data
```

实际返回：

```text
study_data
```

### 用临时容器写入

```bash
docker run --rm \
  -v study_data:/data \
  ubuntu:22.04 \
  sh -c 'echo "hello volume" > /data/message.txt'
```

- `study_data` 是卷名，`/data` 是容器内的挂载目录。
- 未指定 `:ro`，默认可读写。
- `--rm` 在容器退出后自动删除该容器。
- 本例中的命名数据卷 `study_data` 不会随 `--rm` 删除。不要将这一结论推广到匿名卷。

这条命令将输出重定向到文件，所以成功时终端没有文本输出。

### 用另一个新容器读取

```bash
docker run --rm \
  -v study_data:/data:ro \
  ubuntu:22.04 \
  cat /data/message.txt
```

实际输出：

```text
hello volume
```

```text
临时容器 A → 写入 study_data → A 退出并删除
                     ↓ 数据保留
临时容器 B → 挂载同一数据卷 → 读到 hello volume
```

这验证了命名数据卷的生命周期独立于容器。第二个容器使用只读挂载，也可以正常读取先前写入的文件。

## 7. 查看数据卷

可以使用以下命令查看卷列表和详细信息：

```bash
docker volume ls
docker volume inspect study_data
```

- `ls` 列出数据卷。
- `inspect` 显示卷信息；本例使用默认本地卷驱动，`Mountpoint` 表示其在 Docker 服务端系统中的存储位置。
- 日常使用时通过卷名挂载即可，不必直接操作底层存储目录。

以上是补充查看命令，本次没有提供对应输出；已验证的是卷创建与跨容器读写成功。

## 8. C++ 开发中的选择

源码需要在宿主机编辑器中直接修改，适合使用绑定挂载：

```text
宿主机 ~/cpp-demo  ← 编辑器修改源码
        │ 绑定挂载
        ↓
容器 /workspace   ← g++、cmake 编译
```

选择挂载权限时考虑编译输出位置：

- 如果容器要在源码目录下生成构建文件，使用可读写挂载，并保证文件权限允许写入。
- 如果源码以 `:ro` 挂载，编译产物需要写到另一个可写目录。

镜像提供编译工具，挂载提供源码或持久化数据，两者职责不同。

## 9. 本节总结

| 实验 | 已验证的结论 |
|---|---|
| 宿主机网页挂载到 Nginx | 容器能读取指定的宿主机目录 |
| 宿主机修改网页后刷新 | 本例静态内容更新无需重启容器 |
| 容器尝试写入只读挂载 | 返回 `Read-only file system` |
| 删除并重建 Nginx 容器 | 宿主机网页保留，新容器挂载后恢复服务 |
| 两个临时容器共用命名卷 | 第一个容器删除后，数据仍可被第二个容器读取 |

记住五点：

1. 绑定挂载使用宿主机路径，命名数据卷使用卷名。
2. 删除容器会删除其可写层，但不等于删除外部挂载的数据。
3. `:ro` 限制容器通过该挂载写入，不限制宿主机按原权限编辑文件。
4. `--rm` 自动删除退出的容器，本例的命名数据卷继续保留。
5. 挂载不是备份，数据保留也不等于服务持续可用。

下一部分：Dockerfile。将安装 `g++`、`cmake` 等步骤写成文件，构建自己的 C++ 开发环境镜像。
