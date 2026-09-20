# 上传 GitHub 操作说明

## 上传前确认

用于上传的目录是：

```text
D:\codex\mywork\elemental-reactions-cpp-github
```

压缩包是：

```text
D:\codex\mywork\elemental-reactions-cpp-github.zip
```

ZIP 用于备份或传给别人。若要让 GitHub 正常展示源码目录，应上传解压后的目录内容，不要只把 ZIP 当作一个文件上传。

源码包包含 C++ 源码、头文件、测试、中文说明、录屏脚本和 GitHub Actions 自动构建配置；不包含编译器、构建缓存、EXE、视频素材或本机路径配置。

## 推荐方法：Git 命令行

### 1. 在 GitHub 创建空仓库

登录 GitHub，点击右上角 `+`，选择 `New repository`。

建议填写：

- Repository name：`elemental-reactions-cpp`
- Description：`A C++17 elemental reaction and four-character battle simulator.`
- Visibility：想公开展示就选 `Public`；暂不公开就选 `Private`

不要勾选自动创建 README、`.gitignore` 或 License，因为本地包已经有 README 和 `.gitignore`。点击 `Create repository`。

### 2. 在本地创建第一次提交

打开 PowerShell：

```powershell
cd D:\codex\mywork\elemental-reactions-cpp-github
git init -b main
git status
git add .
git status
git commit -m "Initial commit: elemental reaction simulator"
```

第二次 `git status` 应只显示源码、文档、测试、脚本和 `.github`，不应出现 `build`、`.tools`、EXE 或视频文件。

如果 Git 提示没有设置身份，只需执行一次：

```powershell
git config --global user.name "你的 GitHub 显示名"
git config --global user.email "你的 GitHub 邮箱"
git commit -m "Initial commit: elemental reaction simulator"
```

若不希望公开真实邮箱，可以在 GitHub 的 `Settings > Emails` 查找并使用 GitHub 提供的 `noreply` 邮箱。

### 3. 连接 GitHub 仓库

在新仓库页面复制 HTTPS 地址，格式类似：

```text
https://github.com/你的用户名/elemental-reactions-cpp.git
```

执行：

```powershell
git remote add origin https://github.com/你的用户名/elemental-reactions-cpp.git
git remote -v
git push -u origin main
```

首次推送时，Git Credential Manager 通常会打开浏览器，让你登录并授权 GitHub。不要在终端中输入 GitHub 账户密码；若你的环境要求令牌，应使用 Personal Access Token。

### 4. 上传后检查

刷新 GitHub 仓库页面，确认：

- 首页自动显示 `README.md`；
- `include`、`src`、`examples`、`tests`、`docs`、`scripts` 和 `.github` 都存在；
- `Actions` 页面出现 `CMake build and tests`；
- Windows 与 Ubuntu 两个任务都通过；
- 仓库中没有 `build-*`、`.tools`、EXE 或视频素材。

## 不使用 Git：网页上传

1. 在 GitHub 创建空仓库；
2. 解压 `elemental-reactions-cpp-github.zip`；
3. 在空仓库页面点击 `uploading an existing file`，或选择 `Add file > Upload files`；
4. 将解压目录里的全部内容拖到上传区域；注意隐藏目录 `.github` 和隐藏文件 `.gitignore`、`.gitattributes` 也要上传；
5. 在页面下方填写提交说明，例如 `Initial commit: elemental reaction simulator`；
6. 点击 `Commit changes`。

网页方法适合首次上传，但后续更新使用 Git 命令更方便。

## 后续更新

以后修改代码后，在仓库目录执行：

```powershell
git status
git add .
git commit -m "描述这次修改"
git push
```

提交前始终先看 `git status`，避免误传构建产物或个人文件。

## 关于开源许可证

当前包没有附加许可证。公开仓库在没有许可证时，其他人通常只能查看和 fork，不能当然地获得复制、修改和再发布授权。

如果你希望别人可以自由使用并保留署名，可考虑 MIT License；如果希望衍生项目也必须以相同许可证开源，可考虑 GPL。确定后可在 GitHub 仓库的 `Add file > Create new file` 中创建 `LICENSE`，并使用 GitHub 的许可证模板。

## 常见错误

### `remote origin already exists`

先检查：

```powershell
git remote -v
```

若地址错误：

```powershell
git remote set-url origin https://github.com/你的用户名/elemental-reactions-cpp.git
```

### `src refspec main does not match any`

说明还没有成功创建提交。重新执行：

```powershell
git add .
git commit -m "Initial commit: elemental reaction simulator"
git branch -M main
git push -u origin main
```

### GitHub 仓库不是空的，推送被拒绝

最简单的处理方式是重新创建一个空仓库，并且不要勾选 README、`.gitignore` 或 License。不要使用强制推送覆盖不确定的远程内容。
