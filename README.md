# 屏幕录制器 (Screen Recorder)

一个基于 Qt5 和 FFmpeg 的 Windows 屏幕录制桌面应用程序。

## 功能特性

- ✅ 全屏录制
- ⏳ 区域录制（UI已支持，选择功能待完善）
- ✅ 开始/暂停/停止录制
- ✅ 音频录制（麦克风输入）
- ✅ 帧率选择（15/30/60 FPS）
- ✅ 画质设置（低/中/高）
- ⏳ 输出 MP4 格式（H.264 + AAC）- 需要配置 FFmpeg
- ✅ 系统托盘支持
- ✅ 快捷键控制（Ctrl+Shift+R）
- ✅ 实时状态显示

## 技术栈

- **语言**：C++17
- **框架**：Qt 5.15 (Widgets)
- **编码**：FFmpeg 4.x
- **编译器**：MinGW 或 MSVC 2019+
- **构建工具**：qmake

## 项目结构

```
ScreenRecorder/
├── include/
│   ├── MainWindow.h          # 主窗口类
│   ├── ScreenRecorder.h      # 屏幕录制类
│   ├── FFmpegEncoder.h       # FFmpeg编码器类
│   └── Logger.h              # 日志类
├── src/
│   ├── main.cpp              # 程序入口
│   ├── MainWindow.cpp        # 主窗口实现
│   ├── ScreenRecorder.cpp    # 屏幕录制实现
│   ├── FFmpegEncoder.cpp     # FFmpeg编码器实现
│   ├── Logger.cpp            # 日志实现
│   ├── MainWindow.ui         # UI文件
├── resources/
│   └── resources.qrc         # 资源文件
├── ffmpeg/                   # FFmpeg库目录（需要自行添加）
│   ├── include/              # FFmpeg头文件
│   ├── lib/                  # FFmpeg库文件
│   └── bin/                  # FFmpeg DLL文件
└── ScreenRecorder.pro        # 项目配置文件
```

## 前置准备

### 重要：FFmpeg（可选）

本项目支持**不使用 FFmpeg 也可以运行**！
- ✅ 可以测试屏幕录制功能
- ⏳ 只是不会保存视频文件

要保存视频，需要配置 FFmpeg（详见 [FFMPEG_SETUP.md](FFMPEG_SETUP.md)）

### 1. 安装 Qt5

从 [Qt官网](https://www.qt.io/download) 下载并安装 Qt 5.15 或更高版本。

### 2. 下载 FFmpeg

从 [FFmpeg官网](https://ffmpeg.org/download.html) 或 [gyan.dev](https://www.gyan.dev/ffmpeg/builds/) 下载 Windows 开发包（dev 版本）。

推荐使用 `ffmpeg-git-full.7z` 或 `ffmpeg-release-full.7z`。

### 3. 配置 FFmpeg 目录

在项目根目录创建 `ffmpeg` 文件夹，并按以下结构放置文件：

```
ScreenRecorder/
└── ffmpeg/
    ├── include/              # 复制 dev/include 下的所有头文件
    │   ├── libavcodec/
    │   ├── libavformat/
    │   ├── libavutil/
    │   ├── libswscale/
    │   ├── libswresample/
    │   └── ...
    ├── lib/                  # 复制 dev/lib 下的所有 .a 或 .lib 文件
    │   ├── libavcodec.a
    │   ├── libavformat.a
    │   ├── libavutil.a
    │   ├── libswscale.a
    │   ├── libswresample.a
    │   └── ...
    └── bin/                  # 复制 shared/bin 下的所有 .dll 文件
        ├── avcodec-58.dll
        ├── avformat-58.dll
        ├── avutil-56.dll
        ├── swscale-5.dll
        ├── swresample-3.dll
        └── ...
```

## 编译和运行

### 使用 Qt Creator

1. 打开 Qt Creator
2. `文件` → `打开文件或项目` → 选择 `ScreenRecorder.pro`
3. 配置项目（选择编译套件）
4. 点击绿色运行按钮

### 使用命令行

```bash
# 1. 进入项目目录
cd E:\project\ScreenRecorder

# 2. 运行 qmake
qmake ScreenRecorder.pro

# 3. 使用 MinGW 编译
mingw32-make

# 或者使用 MSVC 编译
nmake

# 4. 运行程序
debug\ScreenRecorder.exe  # 或者 release\ScreenRecorder.exe
```

## 使用说明

### 基本录制

1. **选择录制区域**
   - 全屏：选择"全屏录制"
   - 区域：选择"区域录制"，点击"选择区域"

2. **设置帧率**
   - 从下拉菜单选择 15/30/60 FPS

3. **设置画质**
   - 低画质：较小文件，较快编码
   - 中画质：平衡（推荐）
   - 高画质：较大文件，较好质量

4. **音频设置**
   - 勾选"录制音频"以启用麦克风

5. **设置保存路径**
   - 默认保存到桌面
   - 点击"浏览"可自定义路径

6. **开始录制**
   - 点击"开始录制"
   - 或按快捷键 `Ctrl+Shift+R`

7. **控制录制**
   - 暂停/继续：点击"暂停/继续"
   - 停止：点击"停止录制"

## FFmpeg 依赖配置

如果 FFmpeg 不在项目根目录，请修改 `ScreenRecorder.pro` 中的路径：

```qmake
# 修改这一行
FFMPEG_DIR = D:/path/to/your/ffmpeg
```

## 架构说明

### 模块职责

| 模块 | 职责 |
|------|------|
| **MainWindow** | UI 管理，用户交互，状态显示 |
| **ScreenRecorder** | 屏幕采集，帧队列，多线程管理 |
| **FFmpegEncoder** | H.264 视频编码，AAC 音频编码，MP4 封装 |
| **Logger** | 日志输出（简化版使用 qDebug） |

### 数据流

```
屏幕 → ScreenRecorder → 帧队列 → FFmpegEncoder → MP4文件
       （采集线程）            （编码线程）
```

### 线程模型

- **UI 线程**：处理用户交互
- **采集线程**：抓取屏幕帧
- **视频编码线程**：编码视频帧
- **音频编码线程**：编码音频帧（可选）
- **复用线程**：写入 MP4 文件

## 注意事项

1. **DLL 文件**：运行时确保 FFmpeg 的 DLL 文件在可执行文件同一目录或系统 PATH 中

2. **性能**：高帧率 + 高画质会占用较多 CPU

3. **音频**：当前音频采集功能需要进一步完善

## 待完善功能

- [ ] 屏幕区域选择（绘制选择框）
- [ ] 音频采集和编码集成
- [ ] 录制倒计时
- [ ] 视频预览
- [ ] 更多编码器选项
- [ ] 自定义快捷键
- [ ] 录制计划
- [ ] 水印功能

## 许可证

本项目仅供学习和研究使用。

## 致谢

- Qt Project
- FFmpeg Team
