# 第六部分：Dockerfile 与 C++ 镜像构建

本节对应 [学习指南](../learn_guide.md) 中的“第五阶段：Dockerfile”，同时总结后续的 Layer 与构建缓存。通过 `cpp-hello:v1` 到 `v4` 的练习，理解如何把 C++ 程序构建成镜像，并在构建时和运行时配置它。

练习目录为 [phase5-dockerfile](../phase5-dockerfile/)，包含 `hello.cpp`、`Dockerfile` 和 `.dockerignore`。下文按学习过程记录不同版本；目录中的文件目前是最终的环境变量版本。复现早期实验时，需要先使用对应版本的文件内容，镜像标签不会自动切换源码。

## 1. Dockerfile、镜像与容器的关系

```text
hello.cpp + Dockerfile + .dockerignore
                 │
                 │ docker build：安装依赖、复制源码、编译
                 ↓
                镜像
                 │
                 │ docker run：启动指定的程序
                 ↓
                容器
```

本节学习的八个指令：

| 指令 | 作用 |
|---|---|
| `FROM` | 选择基础镜像，开始一个构建阶段 |
| `RUN` | 构建镜像时执行命令，例如安装依赖、编译程序 |
| `WORKDIR` | 设置后续指令及容器启动时的工作目录，目录不存在则创建 |
| `COPY` | 从构建上下文复制文件到镜像 |
| `CMD` | 设置默认启动命令；与本节的 exec 形式 `ENTRYPOINT` 配合时提供默认参数 |
| `ENTRYPOINT` | 指定入口程序，本节使用 exec 形式 |
| `ARG` | 声明构建参数，不自动成为容器的环境变量 |
| `ENV` | 设置后续构建步骤和容器可使用的环境变量 |

**`RUN` 在构建时执行；`CMD` 和 `ENTRYPOINT` 记录启动配置，指定容器启动时要执行什么。**

## 2. 实验一：构建并运行 C++ 程序

### 初始 hello.cpp

```cpp
#include <iostream>

int main() {
    std::cout << "Hello from Docker!" << std::endl;
    return 0;
}
```

### 初始 Dockerfile

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends g++ && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY hello.cpp .

RUN g++ hello.cpp -o hello

CMD ["./hello"]
```

- 第一个 `RUN` 更新软件包索引、安装编译器，再清理索引文件。`&&` 使下一条命令只在前一条成功时执行；行尾 `\` 将长指令分行书写。
- `--no-install-recommends` 不安装推荐包，但仍会安装必要依赖。
- `COPY hello.cpp .` 将源码复制到当前工作目录 `/workspace`。
- 第二个 `RUN` 编译生成 `/workspace/hello`。
- `CMD ["./hello"]` 是 JSON 数组形式，也叫 exec 形式。本例启动时直接运行该程序。

在宿主机执行：

```bash
cd ~/cpp_study/docker/phase5-dockerfile
docker build -t cpp-hello:v1 .
docker images cpp-hello
docker run --rm cpp-hello:v1
```

构建命令中：

| 部分 | 含义 |
|---|---|
| `-t cpp-hello:v1` | 指定镜像名称 `cpp-hello` 和标签 `v1` |
| 最后的 `.` | 当前目录是构建上下文，默认从这里寻找 `Dockerfile` |

本次实际输出包括：

```text
Successfully built 6f777c517714
Successfully tagged cpp-hello:v1
Hello from Docker!
```

程序执行完毕后容器退出。`--rm` 自动删除退出的练习容器，镜像继续保留。

## 3. 构建上下文、.dockerignore 与 COPY

本次 `.dockerignore` 内容：

```text
.git
build/
*.o
hello
```

它排除构建上下文中的这些文件，避免把宿主机编译产物等内容带入构建。本例只显式复制 `hello.cpp`；以后使用 `COPY . .` 时，排除无关文件尤其有用。

`COPY` 的源路径相对于构建上下文。本例的目标 `.` 相对于 `WORKDIR /workspace`，所以复制结果是 `/workspace/hello.cpp`。

不要把 `COPY` 与上一阶段的绑定挂载混淆：

| 方式 | 文件如何进入容器 | 宿主机修改后 |
|---|---|---|
| `COPY hello.cpp .` | 构建时把当时的文件复制进镜像 | 已有镜像不会自动更新，需要重新构建 |
| 绑定挂载 | 运行时让容器访问宿主机路径 | 容器可读到文件变化；本例 C++ 源码变化后仍需重新编译 |

## 4. 实验二：修改源码，观察构建缓存

先不修改文件，再执行相同构建命令，可以观察缓存复用。随后将问候语改为：

```cpp
std::cout << "Hello from Docker v2!" << std::endl;
```

仅修改宿主机源码而不构建，运行旧的 `cpp-hello:v1` 仍会使用镜像内已有的程序。

构建并运行新版本：

```bash
docker build -t cpp-hello:v2 .
docker run --rm cpp-hello:v2
```

本次提供的构建日志确认：

| 步骤 | 实际观察 | 含义 |
|---|---|---|
| `FROM ubuntu:22.04` | 使用已有基础镜像 | 基础环境保持不变 |
| `RUN apt-get ...` | `Using cache` | 没有重新安装编译器 |
| `WORKDIR /workspace` | `Using cache` | 复用已有结果 |
| `COPY hello.cpp .` | 生成新结果 | 源码变化，重新复制 |
| `RUN g++ hello.cpp -o hello` | `Running in ...` | 重新执行编译 |
| `CMD ["./hello"]` | 生成新结果 | 在新镜像中记录默认命令 |

实际运行输出：

```text
Hello from Docker v2!
```

在这个顺序构建的例子中，`COPY` 的缓存失效后，后续步骤需要重新处理。因此，应将变化少、耗时长的依赖安装放前面，将经常变化的源码放后面。

**缓存并不等于自动检查软件包是否更新。** 安装指令及前面的依赖不变时，即使软件源有了新版本，该 `RUN` 也可能复用缓存。

### 构建步骤不等于文件系统层

指南中的 Layer 示意是帮助理解缓存的简化模型。基础镜像本身可以包含多个层；Dockerfile 的每条指令也不一定产生一个文件系统层。

- `RUN`、`COPY` 通常产生文件系统变化。
- `CMD`、`ENTRYPOINT`、`ENV` 主要设置镜像配置。

本次旧版构建器在 `CMD` 步骤下也显示 `Running in ...`，这是处理中间容器的日志，不表示执行了 `./hello`。问候语是在之后的 `docker run` 中打印的。

## 5. CMD 覆盖与容器主进程

以下是用于理解 `CMD` 的补充命令，适用于仍只有 `CMD ["./hello"]` 的 `v2` 镜像：

```bash
docker run --rm cpp-hello:v2 pwd
docker run --rm cpp-hello:v2 ls -l /workspace
docker run --rm -it cpp-hello:v2 bash
```

未配置入口程序时，镜像后提供的命令会替换本例的 `CMD`：

- `pwd` 打印工作目录 `/workspace`。
- `ls` 可以查看镜像中的源码和编译产物。
- `bash` 启动交互终端，进入后可手动执行 `./hello`，用 `exit` 退出。

本次学习中已理解并确认主进程的区别：

```text
默认启动 ./hello → hello 是主进程 → 程序结束，容器退出
改为启动 bash   → bash 是主进程  → 手动运行 hello 后，bash 仍在运行
```

容器是否继续运行取决于主进程是否还在运行。执行 `exit` 结束 `bash` 后，容器才退出。

## 6. 实验三：ENTRYPOINT 与 CMD 配合

将程序改为接受命令行参数：

```cpp
#include <iostream>

int main(int argc, char* argv[]) {
    const char* name = argc > 1 ? argv[1] : "World";
    std::cout << "Hello, " << name << "!" << std::endl;
    return 0;
}
```

将 Dockerfile 原来的 `CMD ["./hello"]` 替换为：

```dockerfile
ENTRYPOINT ["/workspace/hello"]
CMD ["Docker"]
```

在本节使用的 exec 形式下，`ENTRYPOINT` 提供程序，`CMD` 提供默认参数；运行时在镜像后提供的参数替换 `CMD`，再传给入口程序。

```bash
docker build -t cpp-hello:v3 .
```

本次实际验证结果：

| 命令 | 实际执行 | 实际输出 |
|---|---|---|
| `docker run --rm cpp-hello:v3` | `/workspace/hello Docker` | `Hello, Docker!` |
| `docker run --rm cpp-hello:v3 LiHao` | `/workspace/hello LiHao` | `Hello, LiHao!` |
| `docker run --rm cpp-hello:v3 bash` | `/workspace/hello bash` | `Hello, bash!` |

第三条不会进入终端：`bash` 成了传给 `hello` 的参数。

若要替换入口程序，使用：

```bash
docker run --rm -it --entrypoint /bin/bash cpp-hello:v3
```

这次实际进入容器后，手动执行：

```bash
./hello
```

实际输出：

```text
Hello, World!
```

原因是手动执行时没有传参数，程序使用源码中的备用值 `World`。`CMD ["Docker"]` 用于组合容器启动命令，不会给后续手动运行的程序自动补参数。`--entrypoint` 覆盖入口时会清除镜像的默认 `CMD`；如需参数，应在运行命令中显式提供。

## 7. 实验四：ARG 与 ENV

### 最终 hello.cpp

```cpp
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    const char* name = argc > 1 ? argv[1] : "World";

    const char* greeting = std::getenv("GREETING");
    if (greeting == nullptr) {
        greeting = "Hello";
    }

    std::cout << greeting << ", " << name << "!" << std::endl;
    return 0;
}
```

- `argv[1]` 决定向谁问候。
- `std::getenv("GREETING")` 读取环境变量，决定问候语。
- 环境变量不存在时，程序使用备用值 `Hello`。

### 最终 Dockerfile

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends g++ && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY hello.cpp .
RUN g++ hello.cpp -o hello

ARG DEFAULT_GREETING=Hello
ENV GREETING=${DEFAULT_GREETING}

ENTRYPOINT ["/workspace/hello"]
CMD ["Docker"]
```

### 构建时设置镜像默认值

```bash
docker build \
  --build-arg DEFAULT_GREETING=Hi \
  -t cpp-hello:v4 .
```

```text
构建参数 DEFAULT_GREETING=Hi
             ↓ ENV 在构建时替换变量
镜像默认环境变量 GREETING=Hi
             ↓ 程序调用 getenv
运行时打印 Hi, Docker!
```

`ARG` 不会自动成为容器的环境变量。本例通过 `ENV` 将它的值保存成了另一个环境变量 `GREETING`。如果构建时不指定 `--build-arg`，则使用 Dockerfile 中的默认值 `Hello`。

### 运行时覆盖环境变量

本次三次运行的实际结果：

```bash
docker run --rm cpp-hello:v4
# Hi, Docker!

docker run --rm -e GREETING=Welcome cpp-hello:v4 LiHao
# Welcome, LiHao!

docker run --rm cpp-hello:v4
# Hi, Docker!
```

`-e` 只覆盖这次创建的容器的环境变量，无需重新构建，也不会修改镜像中的默认值。

| 比较项 | ARG | ENV |
|---|---|---|
| 用途 | 参数化构建步骤或镜像配置 | 配置构建及容器运行环境 |
| 在哪里设置 | Dockerfile 声明，`docker build --build-arg` 覆盖 | Dockerfile 声明；容器运行时用 `docker run -e` 覆盖 |
| 自动成为容器环境变量 | 不会 | 会 |

### 为什么运行时改 DEFAULT_GREETING 没有效果？

补充判断命令：

```bash
docker run --rm -e DEFAULT_GREETING=Welcome cpp-hello:v4
```

预期仍是 `Hi, Docker!`，本次已正确解释其原因：程序读取的是 `GREETING`。

进一步说，`ENV GREETING=${DEFAULT_GREETING}` 的替换在构建时已经完成，镜像保存的是 `GREETING=Hi`。两个变量不会持续联动，运行时增加 `DEFAULT_GREETING=Welcome` 不会重新执行 Dockerfile，也不会改变 `GREETING`。

## 8. 本次构建日志中的提示

本次构建开头出现：

```text
DEPRECATED: The legacy builder is deprecated and will be removed in a future release.
```

这表示正在使用旧版构建器，Docker 建议使用 Buildx 组件配合 BuildKit。本次构建和运行均成功，该提示没有阻止实验。学习过程中尚未安装或切换构建器；这里保留日志说明。

安装阶段还出现了 `debconf` 前端回退和部分手册文件缺失的提示。根据后续成功安装、编译及运行的结果，这些提示没有阻止本次练习完成。

## 9. 本节结论

1. `docker build` 根据 Dockerfile 构建镜像，`docker run` 根据镜像创建并启动容器。
2. 构建命令最后的 `.` 指定构建上下文，`.dockerignore` 排除其中的无关文件。
3. `COPY` 保存构建时的文件内容，修改宿主机源码后需要重新构建镜像。
4. 将依赖安装放在复制源码之前，可让源码修改复用安装缓存。
5. `RUN` 在构建时执行；`CMD` 和 `ENTRYPOINT` 设置启动行为。
6. 本节 exec 形式的 `ENTRYPOINT` 与 `CMD` 组合为“程序 + 默认参数”，运行时参数可以替换默认参数。
7. 主进程结束，容器退出；`--rm` 删除退出的容器，镜像仍保留。
8. `ARG` 配置构建，`ENV` 配置环境；构建时的变量替换不会在运行时持续联动。

下一阶段：Docker Compose，用一个配置文件管理多项服务及其网络、端口和挂载。

官方参考：[Dockerfile 指令](https://docs.docker.com/reference/dockerfile/)、[构建上下文](https://docs.docker.com/build/concepts/context/)、[构建缓存规则](https://docs.docker.com/build/cache/invalidation/)、[ARG 与 ENV](https://docs.docker.com/build/building/variables/)、[旧版构建器弃用说明](https://docs.docker.com/engine/deprecated/#legacy-builder-for-linux-images)。
