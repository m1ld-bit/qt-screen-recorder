# 屏幕录制器 (Screen Recorder)

一个基于Qt和FFmpeg的Windows屏幕录制桌面应用程序。

## 功能特性

- 全屏录制和选区录制
- 开始/暂停/停止录制控制
- 可选录制麦克风音频
- 支持多种帧率(15/30/60 FPS)
- 可调节画质(低/中/高)
- 录制时显示悬浮时间指示器
- 系统托盘支持
- 快捷键支持(Ctrl+Shift+R)
- 多显示器支持
- 自动日志记录

## 技术栈

- C++17
- Qt 5.15 (Widgets)
- FFmpeg (H.264编码, MP4封装)
- MSVC 2019 (x64)
- qmake

## 项目结构

```
ScreenRecorder/
├── include/              # 头文件
│   ├── Logger.h
│   ├── FFmpegEncoder.h
│   ├── ScreenRecorder.h
│   └── MainWindow.h
├── src/                  # 源文件
│   ├── Logger.cpp
│   ├── FFmpegEncoder.cpp
│   ├── ScreenRecorder.cpp
│   ├── MainWindow.cpp
│   ├── MainWindow.ui
│   └── main.cpp
├── resources/            # 资源文件
│   ├── resources.qrc
│   └── app.rc
├── ffmpeg/               # FFmpeg库(需自行添加)
│   ├── include/
│   └── lib/
└── ScreenRecorder.pro    # 项目配置文件
```

## 编译步骤

### 1. 环境准备

- 安装Visual Studio 2019 (MSVC 2019, x64)
- 安装Qt 5.15 (MSVC 2019, x64)
- 下载FFmpeg开发库

### 2. 配置FFmpeg

将FFmpeg开发库解压到项目根目录下的`ffmpeg`文件夹中:

```
ffmpeg/
├── include/              # FFmpeg头文件
│   ├── libavcodec/
│   ├── libavformat/
│   ├── libavutil/
│   ├── libswscale/
│   └── libswresample/
└── lib/                  # FFmpeg库文件
    ├── avcodec.lib
    ├── avformat.lib
    ├── avutil.lib
    ├── swscale.lib
    └── swresample.lib
```

FFmpeg下载地址: https://github.com/BtbN/FFmpeg-Builds/releases

推荐下载: `ffmpeg-n5.1-latest-win64-gpl-shared-5.1.zip`

### 3. 使用Qt Creator编译

1. 打开Qt Creator
2. 文件 → 打开文件或项目 → 选择`ScreenRecorder.pro`
3. 配置项目: 选择MSVC 2019 64bit编译器
4. 点击构建按钮(Ctrl+B)

### 4. 使用命令行编译

打开"x64 Native Tools Command Prompt for VS 2019":

```cmd
cd E:\project\ScreenRecorder
qmake ScreenRecorder.pro
nmake
```

### 5. 运行程序

将FFmpeg的DLL文件复制到可执行文件所在目录(debug或release),然后运行生成的`ScreenRecorder.exe`

需要的FFmpeg DLL文件:
- avcodec-58.dll
- avformat-58.dll
- avutil-56.dll
- swscale-5.dll
- swresample-3.dll

## 使用说明

### 基本操作

1. **选择录制区域**:
   - 全屏录制: 勾选"全屏录制"
   - 选区录制: 勾选"选区录制",点击"选择区域"按钮,在屏幕上拖动鼠标选择区域

2. **选择显示器**: 如有多个显示器,可从下拉菜单中选择要录制的显示器

3. **设置参数**:
   - 帧率: 15/30/60 FPS
   - 画质: 低/中/高
   - 是否录制麦克风声音

4. **选择保存位置**: 设置视频保存文件夹

5. **开始录制**: 点击"开始录制",3秒倒计时后开始录制

6. **暂停/继续**: 点击"暂停"按钮暂停录制,再次点击继续

7. **停止录制**: 点击"停止"按钮结束录制,视频将自动保存

### 快捷键

- `Ctrl+Shift+R`: 开始/停止录制

### 系统托盘

程序最小化后会驻留在系统托盘,可右键点击托盘图标进行操作。

## 架构说明

### 分层设计

1. **UI层 (MainWindow)**:
   - 用户界面
   - 参数配置
   - 状态显示
   - 系统托盘管理

2. **录制核心层 (ScreenRecorder)**:
   - 屏幕捕获
   - 音频捕获
   - 录制状态管理
   - 多线程处理

3. **编码层 (FFmpegEncoder)**:
   - 视频编码(H.264)
   - 音频编码(AAC)
   - MP4封装
   - 文件写入

4. **工具类 (Logger)**:
   - 日志记录
   - 文件输出

### 多线程设计

- 屏幕捕获和编码在独立线程中运行
- UI线程不被阻塞
- 使用Qt信号槽机制进行线程间通信

## 常见问题

### 1. 编译错误: 找不到FFmpeg头文件

确保FFmpeg的include文件夹已正确放置在项目目录下,并检查`ScreenRecorder.pro`中的路径配置。

### 2. 运行时错误: 缺少DLL文件

将FFmpeg的DLL文件复制到可执行文件所在目录。

### 3. 录制没有声音

确保麦克风已正确连接并在系统中设置为默认录音设备。

## 开发计划

- [ ] 添加摄像头录制功能
- [ ] 添加系统声音录制功能
- [ ] 添加视频编辑功能
- [ ] 添加录制计划任务
- [ ] 支持更多视频格式

## 许可证

本项目仅供学习和研究使用。

## 联系方式

如有问题或建议,欢迎反馈!
