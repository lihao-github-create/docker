# 第十部分：C++、CMake 与 Docker 开发环境

本节对应 [学习指南](../learn_guide.md) 第十一章的“C++ Docker 开发环境”小项目。实验使用 [phase9-cpp-dev](../phase9-cpp-dev/)：宿主机编辑源码，容器提供编译工具，通过绑定挂载保存源码和构建产物，再用 Compose 固定开发配置。

## 1. 核心概念与目录结构

```text
宿主机 phase9-cpp-dev/ ← 绑定挂载 → 容器 /workspace/
        src/main.cpp                 g++、CMake、Make
        build/cpp_demo               配置、编译、运行
```

开发镜像保存工具链；源码和构建产物位于宿主机目录中。删除临时容器后，绑定挂载中的文件仍然存在。

```text
phase9-cpp-dev/
├── Dockerfile
├── .dockerignore
├── .env                 # 本机 UID/GID，由命令生成
├── compose.yaml
├── CMakeLists.txt
├── src/
│   └── main.cpp
└── build/               # CMake 生成的构建目录
```

仓库现有 `.gitignore` 会忽略 `.env` 和 `build/`。重建开发环境时，需要在本机重新生成 `.env`。

## 2. 开发镜像与 CMake 配置

实际 [Dockerfile](../phase9-cpp-dev/Dockerfile)：

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends g++ cmake make && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["bash"]
```

这里没有 `COPY` 源码，也没有在镜像构建时编译程序。与 [第五阶段的程序镜像](06-dockerfile.md) 相比，本次镜像用于提供开发工具，项目在容器运行时挂载进来。

实际 [.dockerignore](../phase9-cpp-dev/.dockerignore)：

```text
.git
build/
```

它控制发送给镜像构建器的上下文，不控制运行时的绑定挂载，也不控制 Git 是否跟踪文件。

实际 [CMakeLists.txt](../phase9-cpp-dev/CMakeLists.txt)：

```cmake
cmake_minimum_required(VERSION 3.16)
project(cpp_demo LANGUAGES CXX)

add_executable(cpp_demo src/main.cpp)
target_compile_features(cpp_demo PRIVATE cxx_std_17)
```

它定义可执行目标 `cpp_demo`，源码为 `src/main.cpp`，要求 C++17。

初始源码打印 `Hello from CMake + Docker!`，修改后的实际 [main.cpp](../phase9-cpp-dev/src/main.cpp) 为：

```cpp
#include <iostream>

int main() {
    std::cout << "Source updated, image unchanged!" << std::endl;
    return 0;
}
```

## 3. 实验一：容器编译，宿主机保存结果

以下命令在宿主机的 `phase9-cpp-dev` 目录执行。

```bash
docker build -t cpp-dev:cmake .

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" \
  cpp-dev:cmake \
  bash -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./build/cpp_demo'
```

| 参数或命令 | 作用 |
|---|---|
| `--rm` | 程序退出后删除临时容器 |
| `--user UID:GID` | 使用宿主机当前用户的数字 UID/GID 运行 |
| `-v "$PWD:/workspace"` | 将当前项目目录绑定挂载到容器工作目录 |
| `cmake -S . -B build` | 指定源码目录与构建目录，配置并生成构建系统 |
| `-DCMAKE_BUILD_TYPE=Debug` | 为本例的 Makefile 构建设置 Debug 配置 |
| `cmake --build build` | 调用生成的构建系统编译项目 |
| `./build/cpp_demo` | 执行构建得到的程序 |
| `bash -c '... && ...'` | 由 shell 处理命令连接，前一步成功才执行下一步 |

实际首次构建识别到 GNU C++ 11.4.0，配置输出指向 `/workspace/build`，随后编译、链接并打印：

```text
[ 50%] Building CXX object CMakeFiles/cpp_demo.dir/src/main.cpp.o
[100%] Linking CXX executable cpp_demo
[100%] Built target cpp_demo
Hello from CMake + Docker!
```

回到宿主机检查：

```bash
ls -l build/cpp_demo
```

实际文件所有者是 `demo demo`。这里关键是数字 UID/GID 匹配，容器内不必存在名为 `demo` 的用户。`--user` 避免本次编译生成由 root 拥有的宿主机文件。

## 4. 实验二：源码修改与增量构建

先修改源码中的输出文字，再只运行已有二进制：

```bash
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" \
  cpp-dev:cmake \
  ./build/cpp_demo
```

实际仍输出旧文字：

```text
Hello from CMake + Docker!
```

修改源码不会自动修改已经编译好的二进制。随后执行：

```bash
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" \
  cpp-dev:cmake \
  bash -c 'cmake --build build && ./build/cpp_demo'
```

实际重新编译 `main.cpp`、链接，输出：

```text
[ 50%] Building CXX object CMakeFiles/cpp_demo.dir/src/main.cpp.o
[100%] Linking CXX executable cpp_demo
[100%] Built target cpp_demo
Source updated, image unchanged!
```

不修改源码，再执行同一条命令，关键输出变为：

```text
[100%] Built target cpp_demo
Source updated, image unchanged!
```

没有再次出现编译和链接步骤，说明构建系统复用了已有产物。每次都是新容器，增量构建仍有效，因为 `build/` 保存在绑定挂载中。

| 变化 | 本例需要的操作 |
|---|---|
| 只运行已经编译好的程序 | 执行 `./build/cpp_demo` |
| 修改 C++ 源码 | 重新构建程序，无需重建工具镜像 |
| 修改 CMake 配置 | 重新配置并构建程序 |
| 修改 Dockerfile 中的工具或系统依赖 | 重建开发镜像，并按工具链变化处理构建目录 |

镜像构建缓存与 CMake 增量构建是两个层面的复用：前者复用镜像构建步骤，后者复用项目构建产物。

## 5. 实验三：用 Compose 固定开发配置

在宿主机项目目录生成本机 `.env`：

```bash
printf 'DEV_UID=%s\nDEV_GID=%s\n' "$(id -u)" "$(id -g)" > .env
```

本次实际内容为：

```dotenv
DEV_UID=1000
DEV_GID=1000
```

其他机器应使用自身的 UID/GID。Compose 用这些值替换 YAML 中的变量；`.env` 中的变量不会自动注入容器，宿主机同名环境变量的插值优先级高于 `.env`。

实际 [compose.yaml](../phase9-cpp-dev/compose.yaml)：

```yaml
services:
  dev:
    image: cpp-dev:cmake
    build: .
    user: "${DEV_UID}:${DEV_GID}"
    volumes:
      - ".:/workspace"
    command:
      - bash
      - -c
      - cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./build/cpp_demo
```

`WORKDIR /workspace` 继承自镜像；`command` 覆盖镜像的 `CMD ["bash"]`。配置中的三个列表元素分别是程序 `bash`、参数 `-c`、完整的 shell 命令字符串。

检查变量替换与挂载路径，再运行：

```bash
docker compose config
docker compose run --rm dev
```

本次已有 `cpp-dev:cmake` 镜像。重新准备镜像或修改 Dockerfile 时，可以执行 `docker compose build dev`。

实际运行完成配置、生成和增量构建，并输出：

```text
-- Build files have been written to: /workspace/build
[100%] Built target cpp_demo
Source updated, image unchanged!
```

只运行程序时，覆盖服务默认命令：

```bash
docker compose run --rm dev ./build/cpp_demo
```

实际输出也是 `Source updated, image unchanged!`，但没有执行 CMake。

最后检查 `ls -l build/cpp_demo`，文件仍归 `demo demo` 所有。本例的 `dev` 是一次性任务服务，使用 `run --rm` 创建临时容器；任务结束后退出是正常行为。

## 6. 学习结果与参考

本次验证了“宿主机编辑 → 容器编译 → 产物留在宿主机 → 新容器复用产物”的开发流程。它把工具镜像、项目文件和运行配置分开管理，适合后续扩展到更大的 C++ 工程。

- [CMake 命令行说明](https://cmake.org/cmake/help/latest/manual/cmake.1.html)：源码目录、构建目录、配置与构建命令。
- [Docker 绑定挂载](https://docs.docker.com/engine/storage/bind-mounts/)：宿主机与容器共享文件。
- [Compose 变量插值](https://docs.docker.com/compose/how-tos/environment-variables/variable-interpolation/)：`.env` 与变量优先级。
- [Compose run](https://docs.docker.com/reference/cli/docker/compose/run/)：临时容器、命令覆盖与 `--rm`。
