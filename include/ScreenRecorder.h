#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#include <QObject>
#include <QThread>
#include <QScreen>
#include <QRect>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QSharedPointer>
#include <QAtomicInt>
#include <QTimer>

/**
 * @brief ScreenRecorder 类 - 专注于屏幕采集
 * 
 * 功能特性：
 * - 支持全屏/指定矩形区域采集
 * - 多显示器支持，自动识别主显示器
 * - 子线程采集，不阻塞UI
 * - 线程安全的帧队列
 * - 实时帧率调整
 * - 屏幕分辨率变化检测
 */
class ScreenRecorder : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 录制状态枚举
     */
    enum RecordState {
        Idle,       // 空闲
        Recording,  // 录制中
        Paused      // 暂停
    };
    Q_ENUM(RecordState)

    /**
     * @brief 采集配置结构
     */
    struct CaptureConfig {
        bool fullScreen;           // 是否全屏
        QRect recordRect;          // 录制区域（全屏时自动计算）
        int frameRate;             // 帧率（15/30/60）
        int screenIndex;           // 显示器索引（-1为主显示器）
        int maxQueueSize;          // 最大队列大小，防止内存溢出

        CaptureConfig()
            : fullScreen(true)
            , frameRate(30)
            , screenIndex(-1)
            , maxQueueSize(30) {}
    };

    /**
     * @brief 帧数据结构
     */
    struct Frame {
        QImage image;              // 帧图像
        qint64 timestamp;          // 时间戳（毫秒）

        Frame() : timestamp(0) {}
        Frame(const QImage& img, qint64 ts)
            : image(img), timestamp(ts) {}
    };

    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit ScreenRecorder(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ScreenRecorder() override;

    /**
     * @brief 设置采集配置
     * @param config 配置信息
     */
    void setConfig(const CaptureConfig& config);

    /**
     * @brief 获取当前配置
     * @return 配置信息
     */
    CaptureConfig config() const;

    /**
     * @brief 开始采集
     * @return 是否成功
     */
    bool start();

    /**
     * @brief 暂停采集
     */
    void pause();

    /**
     * @brief 恢复采集
     */
    void resume();

    /**
     * @brief 停止采集
     */
    void stop();

    /**
     * @brief 获取当前状态
     * @return 录制状态
     */
    RecordState state() const;

    /**
     * @brief 实时设置帧率（不中断当前录制）
     * @param fps 新的帧率
     */
    void setFrameRate(int fps);

    /**
     * @brief 获取当前帧率
     * @return 帧率
     */
    int frameRate() const;

    /**
     * @brief 从队列获取一帧（线程安全）
     * @return 帧的智能指针，如果队列为空返回nullptr
     */
    QSharedPointer<Frame> getFrame();

    /**
     * @brief 获取队列中当前帧数量
     * @return 帧数量
     */
    int queueSize() const;

    /**
     * @brief 清空帧队列
     */
    void clearQueue();

    /**
     * @brief 获取所有可用显示器
     * @return 显示器列表
     */
    static QList<QScreen*> getScreens();

    /**
     * @brief 获取主显示器
     * @return 主显示器指针
     */
    static QScreen* getPrimaryScreen();

signals:
    /**
     * @brief 状态变化信号
     * @param newState 新状态
     */
    void stateChanged(ScreenRecorder::RecordState newState);

    /**
     * @brief 新帧可用信号
     * @param frame 新帧
     */
    void newFrameAvailable(QSharedPointer<ScreenRecorder::Frame> frame);

    /**
     * @brief 采集错误信号
     * @param error 错误信息
     */
    void errorOccurred(const QString& error);

    /**
     * @brief 屏幕配置变化信号（分辨率/多显示器变化）
     */
    void screenConfigChanged();

private slots:
    /**
     * @brief 定时器触发的采集槽
     */
    void onCaptureTick();

    /**
     * @brief 检测屏幕配置变化
     */
    void onScreenConfigChanged();

private:
    /**
     * @brief 初始化采集参数
     * @return 是否成功
     */
    bool initializeCapture();

    /**
     * @brief 清理资源
     */
    void cleanup();

    /**
     * @brief 采集一帧屏幕
     * @return 帧图像
     */
    QImage captureFrame();

    /**
     * @brief 更新当前选择的显示器
     */
    void updateSelectedScreen();

    /**
     * @brief 更新状态并发送信号
     * @param newState 新状态
     */
    void setState(RecordState newState);

    // 成员变量
    QAtomicInt m_state;                    // 录制状态（原子操作，线程安全）
    CaptureConfig m_config;                // 采集配置
    QScreen* m_selectedScreen;             // 选中的显示器

    QThread* m_captureThread;              // 采集线程
    QTimer* m_captureTimer;                // 采集定时器（运行在子线程）
    qint64 m_startTime;                    // 开始时间戳

    QQueue<QSharedPointer<Frame>> m_frameQueue;  // 帧队列
    mutable QMutex m_queueMutex;           // 队列保护锁

    QTimer* m_screenMonitorTimer;          // 屏幕配置监控定时器
    QRect m_lastScreenGeometry;            // 上次记录的屏幕几何
    int m_lastScreenCount;                 // 上次记录的显示器数量
};

#endif // SCREENRECORDER_H
