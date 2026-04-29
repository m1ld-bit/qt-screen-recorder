#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QShortcut>
#include "ScreenRecorder.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onSelectAreaClicked();
    void onBrowseOutputPath();
    void onOpenFolder();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onToggleRecording();
    void onStateChanged(ScreenRecorder::RecordState state);
    void onCountdownUpdated(int remaining);
    void onElapsedTimeUpdated(qint64 ms);
    void onRecordingFinished(const QString& filePath);
    void onRecorderError(const QString& error);

private:
    void initUI();
    void initTrayIcon();
    void initShortcuts();
    void updateUI();
    void showFloatingIndicator();
    void hideFloatingIndicator();
    void loadSettings();
    void saveSettings();
    QString generateFileName();

    Ui::MainWindow* ui;
    ScreenRecorder* m_recorder;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    QShortcut* m_recordShortcut;
    QWidget* m_floatingWidget;
    QLabel* m_floatingLabel;
    bool m_isSelectingArea;
    QPoint m_selectionStart;
    QRect m_selectedRect;
};

#endif // MAINWINDOW_H

