# Symbian S60v3 Hello World（C++ → SIS）

一个跑在塞班 **S60 第三版（S60 3rd Edition / Symbian OS 9.x / EKA2）** 上的
C++ Hello World，并且用 GitHub Actions 调用
[hpnkv/symbian-platform](https://github.com/hpnkv/symbian-platform) 自动完成
**ARM 编译 → E32 可执行文件 → `.sis` 安装包 → 自签名** 的全过程。

程序在屏幕上居中显示 `Hello, World!`，按 **右软键（Exit）** 或 **Escape** 退出。

## 仓库结构

```
app/app.cc                         Hello World 源码（W32 / 窗口服务器）
app/symbian.toml                   应用与安装包身份：UID、版本、菜单名、图标
app/icon.svg                       应用菜单图标
.github/workflows/build-sis.yml    构建流水线（编译 + 打包 + 签名）
```

`app/` 目录里的文件会在构建时覆盖 SDK 生成的样板代码，所以这里放的就是这个
应用的"真身"。

## 怎么拿到 SIS 安装包

1. 把这个目录推到 GitHub 仓库（`main` 或 `master` 分支）。
2. Actions 会自动开始构建；也可以到仓库的 **Actions** 页面点
   **Run workflow** 手动触发。
3. 构建完成后，在这一次运行页面的 **Artifacts** 里下载
   `helloworld-sis`，压缩包里的 **`helloworld-signed.sis`** 就是安装包。
4. 如果推一个形如 `v1.0.0` 的 tag，安装包还会自动附到 Release 上，方便长期下载。

## 流水线做了什么

| 步骤 | 命令 | 结果 |
| --- | --- | --- |
| 安装工具 | `pip install symbian-platform==0.2.0` | Python 版 `symbian` 命令行工具 |
| 安装 SDK | `symbian sdk install --archive …` | 下载官方 native SDK 压缩包（含 ARM Clang/LLD、EKA2 运行库、资源编译器、CMake、Ninja） |
| 生成工程 | `symbian init --architecture armv5t` | 由 SDK 生成与该版本完全匹配的 `CMakeLists.txt` / CMakePresets / 工程配置 |
| 换入源码 | `cp -R app/. …` | 用本仓库的 `app.cc`、`symbian.toml`、`icon.svg` 替换样板 |
| 编译 | `symbian app build` | `helloworld.elf`（带调试符号）+ `helloworld.exe`（E32 镜像） |
| 打包 | `symbian package` | `helloworld.sis`（含可执行文件、菜单注册资源和图标） |
| 签名 | `symbian signing create` / `signing sign` | `helloworld-signed.sis`（本地自签名） |

每一步的 JSON 报告和产物都会一起上传，方便核对。构建是**可复现**的：同样的输入
会得到逐字节相同的镜像。

## 在本地构建（macOS / Linux）

Windows 上无法运行这套工具链（SDK 只提供 macOS / Linux 版本）。在 macOS 15+
或 glibc 2.39+ 的 Linux（如 Ubuntu 24.04）上：

```sh
python3 -m venv ~/.venvs/symbian && source ~/.venvs/symbian/bin/activate
pip install symbian-platform==0.2.0
curl -fL -o symbian-sdk.tar.gz \
  https://github.com/hpnkv/symbian-platform/releases/download/v0.2.0/symbian-sdk-0.2.0-$(uname -s | tr 'A-Z' 'a-z')-$(uname -m).tar.gz
symbian sdk install ~/symbian-sdk --archive symbian-sdk.tar.gz

# 生成工程，然后用本仓库的 app/ 覆盖样板源码
symbian init /tmp/helloworld --name helloworld --uid3 0xE0001001 \
  --architecture armv5t --non-interactive --no-build
cp -R app/. /tmp/helloworld/

symbian app build --project /tmp/helloworld
symbian package --project /tmp/helloworld \
  --artifact /tmp/helloworld/.symbian/build/helloworld.exe \
  --output /tmp/helloworld/.symbian/package
```

## 装到手机上

1. 把 `helloworld-signed.sis` 拷到手机的存储卡（或大容量存储）里，放到
   `Installs` 文件夹；也可以先用数据线连电脑再拷。
2. 手机上打开 **设置 → 应用程序管理**，把 **软件安装** 设为 **全部**，
   把 **在线证书检查** 设为 **关**（S60 3rd Edition 安装自签名包需要这一步）。
3. 用手机的文件管理器找到这个 `.sis`，打开并确认安装。
4. 在应用程序菜单里找到 **Hello World**，启动即可。

## 关于 S60 第三版的兼容性

- **目标架构**：默认按 **ARMv5T** 构建。ARMv5T 二进制在 ARM9 和 ARM11 的
  S60 第三版机器上都能运行，是覆盖面最广的选择。如果确定目标机型需要，
  可以把工作流里的 `APP_ARCH` 改成 `armv6`。
- **系统库**：这个现代运行库需要固件提供 `libpthread.dll`（即 P.I.P.S. /
  Open C 组件）。S60 第三版 FP1 及以后的机型通常自带；较早的 S60 第三版
  机器可能需要先安装 Open C。
- **签名**：这里的签名是本地自签名，**不等于 Symbian Signed**，也不会带来
  额外权限。部分机型的安装策略仍可能拒绝，需要按上面的步骤放宽安装设置。
- 上游 SDK 自己也提示：这类镜像通过结构校验并不代表手机一定接受，正式发布前
  请在目标机型（或 EKA2L1 模拟器）上实测。

## 想改点东西

| 想做的事 | 改哪里 |
| --- | --- |
| 改显示的文字 | `app/app.cc` 里的 `kGreeting` / `kHint` |
| 改应用名、版本、发布者 | `app/symbian.toml` 的 `[package]` 和 `[application]` |
| 改图标 | 替换 `app/icon.svg` |
| 改 UID3 | `app/symbian.toml` 的 `uid3` / `uid`，以及工作流里的 `APP_UID3`（两处必须一致，取值需在 `0xe0000000`–`0xefffffff` 之间） |
| 加更多 `.cc` 源文件 | 把文件放进 `app/`，并在 `app/CMakeLists.txt` 里提供自己的构建描述（该文件会覆盖 SDK 生成的版本；可以先从生成结果复制一份再改） |
| 换 SDK 版本 | 工作流里的 `SDK_VERSION`（Python 包版本和 SDK 压缩包版本必须一致） |

## 依赖与出处

- 构建平台：[hpnkv/symbian-platform](https://github.com/hpnkv/symbian-platform)（Apache-2.0）
- 上游工具链：Clang/LLD、LLVM、Abseil、mbedTLS 等，授权见上游仓库

安装包本身不包含任何诺基亚固件或系统 DLL，这些必须由目标设备自带。
