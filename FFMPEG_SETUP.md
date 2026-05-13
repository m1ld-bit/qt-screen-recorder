# FFmpeg 配置指南

## 快速开始（当前状态）

✅ **项目现在可以直接编译运行了！** 
- FFmpeg 已暂时禁用
- 可以测试屏幕录制功能
- 只是录制的视频不会保存为文件

## 如何启用 FFmpeg

### 第一步：下载 FFmpeg

#### 方法 1：从 gyan.dev 下载（推荐）

访问：https://www.gyan.dev/ffmpeg/builds/

下载：
- `ffmpeg-git-full.7z`（最新开发版）
- 或 `ffmpeg-release-full.7z`（稳定版）

#### 方法 2：从 GitHub Actions 下载

访问：https://github.com/BtbN/FFmpeg-Builds/releases

下载：
- `ffmpeg-n6.1-latest-win64-gpl-shared-6.1.zip`（或类似文件名）

### 第二步：解压并放置文件

在项目根目录创建 `ffmpeg` 文件夹：

```
E:\project\ScreenRecorder\
└── ffmpeg\
    ├── include\              # 头文件
    ├── lib\                  # 库文件
    └── bin\                  # DLL 文件
```

#### 具体操作：

1. 解压下载的 FFmpeg 压缩包
2. 找到 `include` 文件夹 → 复制所有内容到 `ffmpeg\include\`
3. 找到 `lib` 文件夹 → 复制所有 `.a` 或 `.lib` 文件到 `ffmpeg\lib\`
4. 找到 `bin` 文件夹 → 复制所有 `.dll` 文件到 `ffmpeg\bin\`

最终结构应该是：

```
ffmpeg/
├── include/
│   ├── libavcodec/
│   │   └── avcodec.h
│   ├── libavformat/
│   ├── libavutil/
│   ├── libswscale/
│   └── libswresample/
├── lib/
│   ├── libavcodec.dll.a
│   ├── libavformat.dll.a
│   ├── libavutil.dll.a
│   ├── libswscale.dll.a
│   └── libswresample.dll.a
└── bin/
    ├── avcodec-60.dll
    ├── avformat-60.dll
    ├── avutil-58.dll
    ├── swscale-7.dll
    └── swresample-4.dll
```

### 第三步：修改项目配置

打开 `ScreenRecorder.pro`，找到第 20 行：

```qmake
# DEFINES += ENABLE_FFMPEG
```

取消注释，改为：

```qmake
DEFINES += ENABLE_FFMPEG
```

### 第四步：重新编译

1. 在 Qt Creator 中：
   - `构建` → `清理项目 "ScreenRecorder"`
   - `构建` → `运行 qmake`
   - `构建` → `重新构建项目 "ScreenRecorder"`

2. 或者删除 `build-...` 文件夹后重新构建

### 第五步：复制 DLL 文件

编译成功后，将 `ffmpeg\bin\` 中的所有 DLL 文件复制到编译输出目录：

```
build-ScreenRecorder-Desktop_Qt_5_12_11_MinGW_32_bit-Debug\debug\
├── ScreenRecorder.exe
├── avcodec-60.dll
├── avformat-60.dll
├── avutil-58.dll
├── swscale-7.dll
└── swresample-4.dll
```

## 验证配置

启用 FFmpeg 后，你应该可以：

✅ 看到编译器输出：`使用FFmpeg编码器`  
✅ 录制的视频保存为 MP4 文件  
✅ 在桌面找到录制的视频

## 故障排除

### 问题 1：找不到头文件

```
fatal error: libavcodec/avcodec.h: No such file or directory
```

**解决**：检查 `ffmpeg\include` 文件夹中是否有 `libavcodec\avcodec.h`

### 问题 2：链接错误

```
undefined reference to `avcodec_open2'
```

**解决**：检查 `ffmpeg\lib` 文件夹中是否有 `.a` 或 `.lib` 文件

### 问题 3：运行时找不到 DLL

```
The code execution cannot proceed because avcodec-60.dll was not found
```

**解决**：将 `ffmpeg\bin` 中的 DLL 复制到可执行文件同一目录

## 快速测试

配置好 FFmpeg 后：

1. 运行程序
2. 点击"开始录制"
3. 操作屏幕几秒钟
4. 点击"停止录制"
5. 检查桌面，应该有一个 `.mp4` 文件

## 需要帮助？

如果有问题，请检查：
1. FFmpeg 文件路径是否正确
2. `ScreenRecorder.pro` 中的路径是否匹配
3. DLL 文件是否在正确位置
4. Qt Creator 中的项目是否重新 qmake
