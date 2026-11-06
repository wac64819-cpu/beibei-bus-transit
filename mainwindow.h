#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QVector>
#include <QString>
#include <QComboBox>
#include <QRadioButton>
#include <QPushButton>
#include <QTextBrowser>
#include <QListWidget>
#include <QLabel>
#include "core/PathFinder.h"

// 自定义 WebEnginePage：用来捕获 JS 端的 console.log
// JS 端发 "STOP_CLICK:42" → 这里发 signal stopClicked(42)
class JsBridgePage : public QWebEnginePage
{
    Q_OBJECT
public:
    using QWebEnginePage::QWebEnginePage;
signals:
    void stopClicked(int stopId);
    void stopRightClicked(int stopId);
protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel /*level*/,
                                  const QString& msg,
                                  int /*lineNumber*/,
                                  const QString& /*sourceID*/) override {
        if (msg.startsWith(QStringLiteral("STOP_CLICK:"))) {
            bool ok = false; int sid = msg.mid(11).toInt(&ok);
            if (ok) emit stopClicked(sid);
        } else if (msg.startsWith(QStringLiteral("STOP_RCLICK:"))) {
            bool ok = false; int sid = msg.mid(12).toInt(&ok);
            if (ok) emit stopRightClicked(sid);
        }
    }
};

struct Stop {
    int id;
    QString name;
    double lng;
    double lat;
};

struct Route {
    QString id;
    QString name;
    QVector<int> stopSeq;
    QString color;
    QString type;  // "bus" / "metro" / 等
};

class TransitNetwork;  // 前向声明（在 cpp 中包含）

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMapLoaded(bool ok);
    void onQueryClicked();
    void onClearClicked();
    void onStopClicked(int stopId);        // 左键：循环选起点/终点
    void onStopRightClicked(int stopId);   // 右键：取消该站点的选择

private:
    // 地图
    QWebEngineView *view;

    // 左侧 UI
    QComboBox *cbFrom;
    QComboBox *cbTo;
    QRadioButton *rbShortestTime;
    QRadioButton *rbLeastTransfer;
    QRadioButton *rbBalanced;
    QRadioButton *rbAlgoDijkstra;
    QRadioButton *rbAlgoAStar;
    QPushButton *btnQuery;
    QPushButton *btnClear;
    QPushButton *m_btnClearFrom = nullptr;  // ✕ 取消起点
    QPushButton *m_btnClearTo   = nullptr;  // ✕ 取消终点
    QListWidget *m_lstAlternatives = nullptr;   // 方案列表
    QTextBrowser *txtResult;
    QLabel *lblStatus;

    // 数据
    QVector<Stop> stops;
    QVector<Route> routes;
    TransitNetwork* m_network;  // 公交网络（图）

    // 地图点击切换：0 = 下次点设为起点；1 = 下次点设为终点
    int m_clickStep = 0;
    int m_startStopId = -1;   // 当前选中的起点 ID（-1 表示未选）
    int m_endStopId   = -1;   // 当前选中的终点 ID

    // 三种方案的查询结果缓存（用户点哪个方案就高亮哪个）
    QVector<PathResult> m_alternatives;
    int m_currentAlt = -1;    // 当前显示的方案索引

    // 方法
    void loadCsv();
    void setupUi();
    void populateStopCombos();
    void drawAllStops();
    void highlightPath(const QVector<int>& stopIds);
    void runJS(const QString &js);
    const Stop* findStop(int id) const;
    QString escapeJsString(const QString &s) const;

    // 自动查询 + 显示多方案
    void queryAllAlternatives();
    void renderAlternativeList();      // 渲染方案列表
    void highlightAlternative(int idx); // 在地图上高亮指定方案 + 显示详情
    QString summarizeAlt(int idx) const; // 一行简要描述
};

#endif // MAINWINDOW_H
