参考 [Windows安装 WSL2、Ubuntu 、docker（详细步骤 ， 弃用 docker desktop ）_windows wsl2-CSDN博客](https://blog.csdn.net/hugejiletuhugejiltu/article/details/145320105)

[阿里云-镜像加速器地址](https://cr.console.aliyun.com/cn-hangzhou/instances/mirrors)

# 安装步骤

在 Ubuntu 20.04 上安装 Docker，可以按以下步骤操作：

1. **启动 systemd （可选, 对于 wsl2 必选）**

修改 /etc/wsl.conf, 执行 `sudo vi /etc/wsl.conf`：

<img src="https://cdn.nlark.com/yuque/0/2025/png/22644912/1743325244374-ab32ff2d-088d-4a50-952b-88fdb090a4ad.png" width="720" alt="" title="" crop="0,0,1,1" id="uf7792b3d" class="ne-image">

2. **安装 docker**

```python
sudo apt-get update
sudo apt-get install docker.io
```

3. **验证 Docker 是否正确安装**：

```bash
sudo docker --version
```

4. **启动并启用 Docker 服务**：

```bash
sudo systemctl start docker
sudo systemctl enable docker
```

5. **（可选）配置非 root 用户使用 Docker**：
   如果你希望在不使用 `sudo` 的情况下运行 Docker，可以将当前用户加入 `docker` 组：

```bash
sudo usermod -aG docker ${USER}
```

完成后，**注销并重新登录**，或使用 `newgrp docker` 来更新当前会话。

# 配置 docker 国内镜像源

参考 [【docker】docker配置vpn网络代理、镜像源_docker镜像源代理-CSDN博客](https://blog.csdn.net/m0_74280172/article/details/144352602?utm_medium=distribute.pc_relevant.none-task-blog-2~default~baidujs_baidulandingword~default-1-144352602-blog-137355517.235^v43^pc_blog_bottom_relevance_base4&spm=1001.2101.3001.4242.2&utm_relevant_index=4)

1. 编辑 sudo vim  /etc/docker/daemon.json

```python
{
    "registry-mirrors": [
        "https://registry.docker-cn.com",
        "https://docker.mirrors.ustc.edu.cn",
        "https://hub-mirror.c.163.com",
        "https://mirror.baidubce.com",
        "https://ccr.ccs.tencentyun.com"
    ]
}

```

2. 然后执行以下命令，使配置生效：

```python
sudo systemctl daemon-reload
sudo systemctl restart docker
```

# 配置 dns 域名解析服务器

1. `sudo vim /etc/resolv.conf`

```python
nameserver 8.8.8.8
nameserver 8.8.4.4
```

2. 重启 dns 服务，使配置生效

`sudo systemctl restart systemd-resolved`
