#include "mainwindow.h"
#include "core/TransitNetwork.h"
#include "core/PathFinder.h"
#include "core/TransitMode.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>
#include <QUrl>
#include <QDir>
#include <QDebug>
#include <QStringList>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QStatusBar>
#include <QFont>
#include <QElapsedTimer>
#include <QWebEnginePage>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_network(nullptr)
{
    setupUi();
    loadCsv();
    populateStopCombos();

    // 构建公交网络（图）
    m_network = new TransitNetwork();
    m_network->build(stops, routes);
    qDebug() << "Graph built: nodes=" << m_network->graph().nodeCount();

    // 加载地图 HTML
    QString htmlPath = QCoreApplication::applicationDirPath() + "/web/map.html";
    if (!QFile::exists(htmlPath))
        htmlPath = QCoreApplication::applicationDirPath() + "/../web/map.html";
    if (!QFile::exists(htmlPath))
        htmlPath = QCoreApplication::applicationDirPath() + "/../../web/map.html";

    qDebug() << "Loading map from:" << htmlPath;
    // ★ 用自定义的 JsBridgePage 替换默认 page，捕获 console.log 实现 JS→C++ 通信
    JsBridgePage* bridge = new JsBridgePage(view);
    view->setPage(bridge);
    connect(bridge, &JsBridgePage::stopClicked, this, &MainWindow::onStopClicked);
    connect(bridge, &JsBridgePage::stopRightClicked, this, &MainWindow::onStopRightClicked);

    view->load(QUrl::fromLocalFile(QFileInfo(htmlPath).absoluteFilePath()));
    connect(view, &QWebEngineView::loadFinished, this, &MainWindow::onMapLoaded);
}

MainWindow::~MainWindow() { delete m_network; }

void MainWindow::setupUi()
{
    // ===== 左侧 UI 面板 =====
    QWidget *leftPanel = new QWidget(this);
    leftPanel->setMinimumWidth(320);
    leftPanel->setMaximumWidth(400);
    // 强制浅色主题：白底深字，避免系统暗主题导致看不清
    leftPanel->setStyleSheet(
        "QWidget { background:#ffffff; color:#212121; font-family:'Microsoft YaHei','微软雅黑',sans-serif; }"
        "QLabel  { color:#212121; }"
        "QGroupBox { color:#1976d2; font-weight:bold; border:1px solid #ddd; border-radius:6px; margin-top:10px; padding-top:8px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; background:#ffffff; }"
        "QRadioButton { color:#212121; padding:2px; }"
        "QComboBox { color:#212121; background:#ffffff; padding:4px; border:1px solid #ccc; border-radius:3px; }"
        "QComboBox QAbstractItemView { color:#212121; background:#ffffff; selection-background-color:#bbdefb; selection-color:#0d47a1; }"
    );
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(12);

    // 标题
    QLabel *title = new QLabel(QStringLiteral("公交换乘查询系统"), leftPanel);
    QFont titleFont = title->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet("color: #1976d2; padding: 8px 0;");
    leftLayout->addWidget(title);

    QLabel *subtitle = new QLabel(QStringLiteral("重庆市北碚区"), leftPanel);
    subtitle->setStyleSheet("color: #666; font-size: 11px;");
    leftLayout->addWidget(subtitle);

    // ===== 起终点选择组 =====
    QGroupBox *gbStops = new QGroupBox(QStringLiteral("起点和终点"), leftPanel);
    QVBoxLayout *gbStopsLayout = new QVBoxLayout(gbStops);

    QLabel *lblFrom = new QLabel(QStringLiteral("起点站："), gbStops);
    cbFrom = new QComboBox(gbStops);
    cbFrom->setEditable(true);
    cbFrom->setInsertPolicy(QComboBox::NoInsert);
    QPushButton *btnClearFrom = new QPushButton(QStringLiteral("✕"), gbStops);
    btnClearFrom->setFixedSize(28, 28);
    btnClearFrom->setToolTip(QStringLiteral("取消起点"));
    btnClearFrom->setStyleSheet(
        "QPushButton { background:#fff; color:#d93025; border:1px solid #d93025;"
        " border-radius:4px; font-weight:bold; font-size:13px; }"
        "QPushButton:hover { background:#fde7e7; }"
        "QPushButton:disabled { color:#ccc; border-color:#ddd; background:#f5f5f5; }");
    btnClearFrom->setEnabled(false);  // 默认禁用，选了起点后才启用

    QLabel *lblTo = new QLabel(QStringLiteral("终点站："), gbStops);
    cbTo = new QComboBox(gbStops);
    cbTo->setEditable(true);
    cbTo->setInsertPolicy(QComboBox::NoInsert);
    QPushButton *btnClearTo = new QPushButton(QStringLiteral("✕"), gbStops);
    btnClearTo->setFixedSize(28, 28);
    btnClearTo->setToolTip(QStringLiteral("取消终点"));
    btnClearTo->setStyleSheet(btnClearFrom->styleSheet());
    btnClearTo->setEnabled(false);

    // 起点行：标签 + 下拉 + ✕
    QHBoxLayout *rowFrom = new QHBoxLayout();
    rowFrom->setSpacing(6);
    rowFrom->addWidget(cbFrom, 1);
    rowFrom->addWidget(btnClearFrom);

    // 终点行
    QHBoxLayout *rowTo = new QHBoxLayout();
    rowTo->setSpacing(6);
    rowTo->addWidget(cbTo, 1);
    rowTo->addWidget(btnClearTo);

    gbStopsLayout->addWidget(lblFrom);
    gbStopsLayout->addLayout(rowFrom);
    gbStopsLayout->addSpacing(6);
    gbStopsLayout->addWidget(lblTo);
    gbStopsLayout->addLayout(rowTo);
    leftLayout->addWidget(gbStops);

    // 保存按钮引用，供 slot 使用
    m_btnClearFrom = btnClearFrom;
    m_btnClearTo = btnClearTo;

    // 取消按钮信号
    connect(btnClearFrom, &QPushButton::clicked, this, [this]() {
        runJS("setStopSelection(0, 'clearStart');");
        m_startStopId = -1;
        m_clickStep = 0;
        m_btnClearFrom->setEnabled(false);
        lblStatus->setText(QStringLiteral("已取消起点"));
    });
    connect(btnClearTo, &QPushButton::clicked, this, [this]() {
        runJS("setStopSelection(0, 'clearEnd');");
        m_endStopId = -1;
        m_clickStep = 1;
        m_btnClearTo->setEnabled(false);
        lblStatus->setText(QStringLiteral("已取消终点"));
    });

    // ===== 优化目标控件（隐藏，仍然创建以兼容代码）=====
    rbShortestTime = new QRadioButton(leftPanel);
    rbLeastTransfer = new QRadioButton(leftPanel);
    rbBalanced = new QRadioButton(leftPanel);
    rbShortestTime->setChecked(true);
    rbShortestTime->hide(); rbLeastTransfer->hide(); rbBalanced->hide();

    // ===== 算法选择（隐藏，默认 Dijkstra）=====
    // 控件仍创建以保持后续代码兼容，但不加入界面
    rbAlgoDijkstra = new QRadioButton(QStringLiteral("Dijkstra"), leftPanel);
    rbAlgoAStar    = new QRadioButton(QStringLiteral("A*"), leftPanel);
    rbAlgoDijkstra->setChecked(true);
    rbAlgoDijkstra->hide();
    rbAlgoAStar->hide();

    // ===== 按钮 =====
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnQuery = new QPushButton(QStringLiteral("查询方案"), leftPanel);
    btnQuery->setStyleSheet("QPushButton { background:#1976d2; color:white; padding:8px; font-weight:bold; border-radius:4px; }"
                            "QPushButton:hover { background:#1565c0; }");
    btnClear = new QPushButton(QStringLiteral("清除"), leftPanel);
    btnClear->setStyleSheet("QPushButton { background:#e0e0e0; padding:8px; border-radius:4px; }"
                            "QPushButton:hover { background:#bdbdbd; }");
    btnLayout->addWidget(btnQuery, 2);
    btnLayout->addWidget(btnClear, 1);
    leftLayout->addLayout(btnLayout);

    // ===== 方案列表（点击切换）=====
    QLabel *lblResult = new QLabel(QStringLiteral("方案列表（点击切换）"), leftPanel);
    lblResult->setStyleSheet("color:#666; font-size:11px; margin-top:6px;");
    leftLayout->addWidget(lblResult);

    m_lstAlternatives = new QListWidget(leftPanel);
    m_lstAlternatives->setStyleSheet(
        "QListWidget { background:#ffffff; border:1px solid #ddd; padding:2px; }"
        "QListWidget::item { padding:6px; margin:2px; border:1px solid #ddd; "
        " border-radius:4px; color:#212121; background:#fafafa; }"
        "QListWidget::item:hover { background:#e8f0fe; border-color:#1976d2; }"
        "QListWidget::item:selected { background:#e8f0fe; border:2px solid #1976d2; "
        " color:#0d47a1; }"
    );
    m_lstAlternatives->setMaximumHeight(140);
    leftLayout->addWidget(m_lstAlternatives);
    connect(m_lstAlternatives, &QListWidget::itemClicked, this,
        [this](QListWidgetItem* item) {
            if (!item) return;
            int idx = item->data(Qt::UserRole).toInt();
            highlightAlternative(idx);
        });

    // ===== 路线详情区 =====
    QLabel *lblDetail = new QLabel(QStringLiteral("详细路线"), leftPanel);
    lblDetail->setStyleSheet("color:#666; font-size:11px; margin-top:6px;");
    leftLayout->addWidget(lblDetail);

    txtResult = new QTextBrowser(leftPanel);
    txtResult->setStyleSheet(
        "QTextBrowser { "
        "  background:#fafafa; "
        "  color:#212121; "                // 深黑色字体
        "  border:1px solid #ddd; "
        "  padding:8px; "
        "  font-size:12px; "
        "  font-family:'Microsoft YaHei','微软雅黑',sans-serif; "
        "}"
    );
    txtResult->setText(QStringLiteral("请在地图上点击站点：第 1 次为起点，第 2 次为终点。\n选完会自动生成 3 个方案供你比较。"));
    leftLayout->addWidget(txtResult, 1);

    // 状态栏
    lblStatus = new QLabel(QStringLiteral("准备就绪"), leftPanel);
    lblStatus->setStyleSheet("color:#666; font-size:10px;");
    leftLayout->addWidget(lblStatus);

    // ===== 右侧地图 =====
    view = new QWebEngineView(this);
    view->setMinimumSize(800, 600);

    // ===== 整体布局：左右分割 =====
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(view);
    splitter->setSizes({340, 1000});
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);

    setCentralWidget(splitter);

    statusBar()->showMessage(QStringLiteral("重庆北碚公交换乘系统  |  数据：60 站点 / 20 线路"));

    // 连接信号
    connect(btnQuery, &QPushButton::clicked, this, &MainWindow::onQueryClicked);
    connect(btnClear, &QPushButton::clicked, this, &MainWindow::onClearClicked);
}

void MainWindow::loadCsv()
{
    QString base = QCoreApplication::applicationDirPath();
    QString stopsPath = base + "/data/stops.csv";
    if (!QFile::exists(stopsPath))
        stopsPath = base + "/../data/stops.csv";
    if (!QFile::exists(stopsPath))
        stopsPath = base + "/../../data/stops.csv";

    QFile fs(stopsPath);
    if (fs.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&fs);
        in.setEncoding(QStringConverter::Utf8);
        in.readLine();
        while (!in.atEnd()) {
            QStringList p = in.readLine().split(',');
            if (p.size() < 4) continue;
            Stop s;
            s.id = p[0].toInt();
            s.name = p[1];
            s.lng = p[2].toDouble();
            s.lat = p[3].toDouble();
            stops.append(s);
        }
        fs.close();
        qDebug() << "Loaded stops:" << stops.size();
    } else {
        qDebug() << "Cannot open stops.csv at" << stopsPath;
    }

    QString routesPath = base + "/data/routes.csv";
    if (!QFile::exists(routesPath))
        routesPath = base + "/../data/routes.csv";
    if (!QFile::exists(routesPath))
        routesPath = base + "/../../data/routes.csv";

    QFile fr(routesPath);
    if (fr.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&fr);
        in.setEncoding(QStringConverter::Utf8);
        in.readLine();
        while (!in.atEnd()) {
            QStringList p = in.readLine().split(',');
            if (p.size() < 4) continue;
            Route r;
            r.id = p[0];
            r.name = p[1];
            for (const QString &s : p[2].split('-'))
                r.stopSeq.append(s.toInt());
            r.color = p[3];
            // 第 5 列可选：type；缺失默认 bus（向后兼容）
            r.type = (p.size() >= 5 && !p[4].trimmed().isEmpty()) ? p[4].trimmed() : QStringLiteral("bus");
            routes.append(r);
        }
        fr.close();
        qDebug() << "Loaded routes:" << routes.size();
    }
}

void MainWindow::populateStopCombos()
{
    cbFrom->clear();
    cbTo->clear();
    for (const Stop &s : stops) {
        QString display = QString("%1  (#%2)").arg(s.name).arg(s.id);
        cbFrom->addItem(display, s.id);
        cbTo->addItem(display, s.id);
    }
    if (stops.size() > 1) {
        cbFrom->setCurrentIndex(0);
        cbTo->setCurrentIndex(stops.size() / 2);
    }
}

void MainWindow::onMapLoaded(bool ok)
{
    if (!ok) {
        qDebug() << "Map load failed";
        return;
    }
    qDebug() << "Map loaded, drawing stops only...";
    drawAllStops();
    // 注意：不调用 drawAllRoutes()——按用户要求只显示站点
}

void MainWindow::drawAllStops()
{
    for (const Stop &s : stops) {
        QString name = escapeJsString(s.name);
        QString js = QString("addStop(%1, %2, '%3', %4);")
                        .arg(s.lat).arg(s.lng).arg(name).arg(s.id);
        runJS(js);
    }
}

void MainWindow::onQueryClicked()
{
    // 从下拉框取起终点，同步到地图选择状态，然后用统一的多方案查询
    int fromId = cbFrom->currentData().toInt();
    int toId = cbTo->currentData().toInt();
    const Stop *sFrom = findStop(fromId);
    const Stop *sTo = findStop(toId);
    if (!sFrom || !sTo) {
        txtResult->setText(QStringLiteral("起点或终点无效"));
        return;
    }
    if (fromId == toId) {
        txtResult->setText(QStringLiteral("起点和终点相同"));
        return;
    }
    // 同步到内部状态 + 地图高亮
    m_startStopId = fromId;
    m_endStopId = toId;
    runJS(QString("setStopSelection(%1, 'start');").arg(fromId));
    runJS(QString("setStopSelection(%1, 'end');").arg(toId));
    if (m_btnClearFrom) m_btnClearFrom->setEnabled(true);
    if (m_btnClearTo)   m_btnClearTo->setEnabled(true);
    queryAllAlternatives();
    return;  // 不再走下面的旧单方案逻辑（保留代码兼容防废弃警告）

    OptimizeMode mode = OptimizeMode::ShortestTime;
    QString modeName = QStringLiteral("最短时间");
    if (rbLeastTransfer->isChecked()) { mode = OptimizeMode::LeastTransfer; modeName = QStringLiteral("最少换乘"); }
    else if (rbBalanced->isChecked()) { mode = OptimizeMode::Balanced; modeName = QStringLiteral("综合最优"); }

    // 调用算法（PathFinder 现在能用 network 做 A* 启发函数）
    PathFinder finder(&m_network->graph(), m_network);
    QElapsedTimer timer; timer.start();
    PathResult res;
    bool useAStar = rbAlgoAStar->isChecked();
    if (useAStar && mode == OptimizeMode::ShortestTime) {
        // A* 仅支持"最短时间"目标
        res = finder.findShortestPathAStar(fromId, toId);
    } else {
        if (mode == OptimizeMode::ShortestTime)        res = finder.findShortestPath(fromId, toId);
        else if (mode == OptimizeMode::LeastTransfer)  res = finder.findLeastTransfer(fromId, toId);
        else                                            res = finder.findBalanced(fromId, toId);
    }
    qint64 elapsed = timer.elapsed();

    if (!res.found) {
        txtResult->setText(QStringLiteral("未找到 %1 到 %2 的可达路径\n\n（站点间可能没有公交/地铁线路连接）")
                           .arg(sFrom->name, sTo->name));
        runJS("if (typeof pathLayer !== 'undefined') pathLayer.clearLayers();");
        return;
    }

    // 高亮路径到地图（按模式分段彩色 + 每条腿不同颜色）
    {
        // 先计算每段的"腿"编号——连续同 routeId 算同一腿，换乘+1
        QVector<int> legIndex(res.stopIds.size() > 0 ? res.stopIds.size() - 1 : 0, 0);
        int curLeg = 0;
        QString lastRouteId;
        for (int i = 0; i < legIndex.size(); ++i) {
            QString rid = res.routeIds.value(i);
            if (i > 0 && rid != lastRouteId) curLeg++;
            legIndex[i] = curLeg;
            lastRouteId = rid;
        }

        // 构造 segments 数组
        QString segmentsJs = "[";
        bool firstSeg = true;
        for (int i = 0; i + 1 < res.stopIds.size(); ++i) {
            const Stop *sa = findStop(res.stopIds[i]);
            const Stop *sb = findStop(res.stopIds[i + 1]);
            if (!sa || !sb) continue;
            if (!firstSeg) segmentsJs += ",";
            firstSeg = false;
            QString segMode = res.modes.value(i, "bus");
            int leg = legIndex.value(i, 0);
            segmentsJs += "{from:[" + QString::number(sa->lat, 'f', 6) + "," + QString::number(sa->lng, 'f', 6)
                        + "],to:[" + QString::number(sb->lat, 'f', 6) + "," + QString::number(sb->lng, 'f', 6)
                        + "],mode:'" + segMode + "',leg:" + QString::number(leg)
                        + ",fromId:" + QString::number(sa->id)
                        + ",toId:" + QString::number(sb->id) + "}";
        }
        segmentsJs += "]";

        // 起终点坐标
        QString startCoord = "[0,0]", endCoord = "[0,0]";
        if (!res.stopIds.isEmpty()) {
            const Stop* sStart = findStop(res.stopIds.first());
            const Stop* sEnd = findStop(res.stopIds.last());
            if (sStart) startCoord = "[" + QString::number(sStart->lat, 'f', 6) + "," + QString::number(sStart->lng, 'f', 6) + "]";
            if (sEnd)   endCoord   = "[" + QString::number(sEnd->lat,   'f', 6) + "," + QString::number(sEnd->lng,   'f', 6) + "]";
        }

        QString js = "highlightPathByMode(" + segmentsJs + ", " + startCoord + ", " + endCoord + ");";
        qDebug().noquote() << "[runJS]" << js.left(300);
        runJS(js);
    }

    // 生成 HTML 富文本（每条腿对应不同颜色）
    static const QStringList LEG_COLORS = {
        "#1a73e8","#f57c00","#43a047","#e53935",
        "#8e24aa","#00897b","#5e35b1","#fb8c00"
    };
    auto legColor = [&](int idx) { return LEG_COLORS.value(idx % LEG_COLORS.size()); };

    QString html;
    html += "<div style='font-family:Microsoft YaHei,sans-serif; color:#212121;'>";
    html += QString("<div style='font-size:13px;'><b>🚏 %1 → %2</b></div>").arg(sFrom->name.toHtmlEscaped(), sTo->name.toHtmlEscaped());
    html += QString("<div style='color:#666; margin:4px 0;'>📊 %1</div>").arg(modeName);
    html += "<hr style='border:0; border-top:1px dashed #ccc; margin:6px 0;'/>";
    html += QString("<div>⏱  预计时长：<b>%1</b> 分钟</div>").arg(QString::number(res.totalTimeMin, 'f', 1));
    html += QString("<div>📏 总距离：<b>%1</b> 公里</div>").arg(QString::number(res.totalDistance, 'f', 2));
    html += QString("<div>🔄 换乘次数：<b>%1</b> 次").arg(res.transferCount);
    if (res.crossModeTransfers > 0) html += QString("（含跨模式 %1 次）").arg(res.crossModeTransfers);
    html += "</div>";
    html += QString("<div>📍 经过站点：<b>%1</b> 个</div>").arg(res.stopIds.size());
    html += "<hr style='border:0; border-top:1px dashed #ccc; margin:6px 0;'/>";
    html += "<div style='margin-bottom:4px;'><b>🗺️ 详细路线：</b></div>";

    // 计算每段的 leg index（与地图一致）
    QVector<int> legIdx(res.stopIds.size() > 0 ? res.stopIds.size() - 1 : 0, 0);
    {
        int cl = 0;
        QString lastRid;
        for (int i = 0; i < legIdx.size(); ++i) {
            QString rid = res.routeIds.value(i);
            if (i > 0 && rid != lastRid) cl++;
            legIdx[i] = cl;
            lastRid = rid;
        }
    }

    QString curRouteId;
    for (int i = 0; i < res.stopIds.size(); ++i) {
        const Stop* s = findStop(res.stopIds[i]);
        if (!s) continue;
        if (i == 0) {
            html += QString("<div style='color:#34a853;'><b>🟢 %1</b></div>").arg(s->name.toHtmlEscaped());
            continue;
        }
        QString segRouteId = res.routeIds.value(i - 1);
        QString segRouteName = res.routeNames.value(i - 1);
        QString segMode = res.modes.value(i - 1, "bus");
        QString icon = (segMode == "metro") ? "🚇" : (segMode == "walk" ? "🚶" : "🚌");
        QString color = legColor(legIdx.value(i - 1, 0));
        if (segRouteId != curRouteId) {
            html += QString("<div style='margin:4px 0 2px 12px; color:%1;'><b>%2 %3</b></div>")
                       .arg(color, icon, segRouteName.toHtmlEscaped());
            curRouteId = segRouteId;
        }
        if (i == res.stopIds.size() - 1) {
            html += QString("<div style='color:#ea4335;'><b>🔴 %1</b></div>").arg(s->name.toHtmlEscaped());
        } else {
            html += QString("<div style='margin-left:24px; color:#555;'>· %1</div>").arg(s->name.toHtmlEscaped());
        }
    }
    html += "</div>";

    txtResult->setHtml(html);
    lblStatus->setText(QStringLiteral("用时 %1 分钟 | %2 次换乘 | %3 km")
                       .arg(QString::number(res.totalTimeMin, 'f', 0))
                       .arg(res.transferCount)
                       .arg(QString::number(res.totalDistance, 'f', 2)));
}

void MainWindow::highlightPath(const QVector<int>& stopIds)
{
    // 构造坐标数组传给 JS
    QString coords = "[";
    for (int i = 0; i < stopIds.size(); ++i) {
        const Stop* s = findStop(stopIds[i]);
        if (!s) continue;
        if (i > 0) coords += ",";
        coords += QString("[%1,%2]").arg(s->lat).arg(s->lng);
    }
    coords += "]";
    // 调用 JS 函数高亮
    QString js = QString("highlightPath(%1);").arg(coords);
    runJS(js);
}

void MainWindow::onClearClicked()
{
    txtResult->setText(QStringLiteral("已清除\n\n请选择起点和终点，然后点击\"查询路径\"按钮。"));
    lblStatus->setText(QStringLiteral("已清除"));
    runJS("if (typeof pathLayer !== 'undefined') pathLayer.clearLayers();");
    runJS("if (typeof setStopSelection !== 'undefined') setStopSelection(0, 'clear');");
    m_clickStep = 0;
    m_startStopId = -1;
    m_endStopId = -1;
    if (m_btnClearFrom) m_btnClearFrom->setEnabled(false);
    if (m_btnClearTo)   m_btnClearTo->setEnabled(false);
    if (m_lstAlternatives) m_lstAlternatives->clear();
    m_alternatives.clear();
    m_currentAlt = -1;
}

// 地图左键点击：第 1 次设为起点，第 2 次设为终点，循环
void MainWindow::onStopClicked(int stopId)
{
    const Stop* s = findStop(stopId);
    if (!s) return;
    if (m_clickStep == 0) {
        int idx = cbFrom->findData(stopId);
        if (idx >= 0) cbFrom->setCurrentIndex(idx);
        runJS(QString("setStopSelection(%1, 'start');").arg(stopId));
        lblStatus->setText(QStringLiteral("起点：%1（再点一下设终点）").arg(s->name));
        m_startStopId = stopId;
        if (m_btnClearFrom) m_btnClearFrom->setEnabled(true);
        m_clickStep = 1;
    } else {
        int idx = cbTo->findData(stopId);
        if (idx >= 0) cbTo->setCurrentIndex(idx);
        runJS(QString("setStopSelection(%1, 'end');").arg(stopId));
        lblStatus->setText(QStringLiteral("终点：%1，正在计算方案...").arg(s->name));
        m_endStopId = stopId;
        if (m_btnClearTo) m_btnClearTo->setEnabled(true);
        m_clickStep = 0;
        // ★ 选完终点立即自动查询 3 个方案
        queryAllAlternatives();
    }
}

// 选完起终点自动生成 3 个方案
void MainWindow::queryAllAlternatives()
{
    m_alternatives.clear();
    m_currentAlt = -1;
    if (m_startStopId < 0 || m_endStopId < 0 || m_startStopId == m_endStopId) {
        txtResult->setText(QStringLiteral("起点和终点相同或无效"));
        return;
    }
    PathFinder finder(&m_network->graph(), m_network);
    // 3 个方案对应 3 种优化目标
    PathResult r1 = finder.findShortestPath(m_startStopId, m_endStopId);
    r1.summary = QStringLiteral("最短时间");
    PathResult r2 = finder.findLeastTransfer(m_startStopId, m_endStopId);
    r2.summary = QStringLiteral("最少换乘");
    PathResult r3 = finder.findBalanced(m_startStopId, m_endStopId);
    r3.summary = QStringLiteral("综合最优");
    // 全部加入，不去重——但相同站序的方案标注"≡ 与方案 X 相同"
    auto sameStops = [](const PathResult& a, const PathResult& b) {
        return a.stopIds == b.stopIds;
    };
    auto addWithTag = [&](PathResult& r) {
        if (!r.found) return;
        for (int j = 0; j < m_alternatives.size(); ++j) {
            if (sameStops(m_alternatives[j], r)) {
                r.summary += QString("（≡ 方案 %1）").arg(j + 1);
                break;
            }
        }
        m_alternatives.append(r);
    };
    addWithTag(r1);
    addWithTag(r2);
    addWithTag(r3);
    if (m_alternatives.isEmpty()) {
        txtResult->setText(QStringLiteral("未找到从起点到终点的可达路径"));
        return;
    }
    renderAlternativeList();
    // 默认高亮第 0 个方案
    highlightAlternative(0);
}

void MainWindow::renderAlternativeList()
{
    if (!m_lstAlternatives) return;
    m_lstAlternatives->clear();
    for (int i = 0; i < m_alternatives.size(); ++i) {
        const PathResult& r = m_alternatives[i];
        QString icon = (r.crossModeTransfers > 0) ? "🚇" : "🚌";
        QString label = QString("%1 方案 %2：%3\n   ⏱ %4 分钟  📏 %5 km  🔄 %6 次换乘")
            .arg(icon).arg(i + 1).arg(r.summary)
            .arg(QString::number(r.totalTimeMin, 'f', 1))
            .arg(QString::number(r.totalDistance, 'f', 2))
            .arg(r.transferCount);
        QListWidgetItem *item = new QListWidgetItem(label, m_lstAlternatives);
        item->setData(Qt::UserRole, i);  // 保存方案索引
    }
}

void MainWindow::highlightAlternative(int idx)
{
    if (idx < 0 || idx >= m_alternatives.size()) return;
    m_currentAlt = idx;
    const PathResult& res = m_alternatives[idx];
    if (!res.found) return;
    const Stop *sFrom = findStop(m_startStopId);
    const Stop *sTo = findStop(m_endStopId);

    // 计算 leg index
    QVector<int> legIndex(res.stopIds.size() > 0 ? res.stopIds.size() - 1 : 0, 0);
    {
        int cl = 0; QString lastRid;
        for (int i = 0; i < legIndex.size(); ++i) {
            QString rid = res.routeIds.value(i);
            if (i > 0 && rid != lastRid) cl++;
            legIndex[i] = cl;
            lastRid = rid;
        }
    }

    // 构造 segments
    QString segmentsJs = "[";
    bool firstSeg = true;
    for (int i = 0; i + 1 < res.stopIds.size(); ++i) {
        const Stop *sa = findStop(res.stopIds[i]);
        const Stop *sb = findStop(res.stopIds[i + 1]);
        if (!sa || !sb) continue;
        if (!firstSeg) segmentsJs += ",";
        firstSeg = false;
        QString segMode = res.modes.value(i, "bus");
        int leg = legIndex.value(i, 0);
        segmentsJs += "{from:[" + QString::number(sa->lat, 'f', 6) + "," + QString::number(sa->lng, 'f', 6)
                    + "],to:[" + QString::number(sb->lat, 'f', 6) + "," + QString::number(sb->lng, 'f', 6)
                    + "],mode:'" + segMode + "',leg:" + QString::number(leg)
                    + ",fromId:" + QString::number(sa->id)
                    + ",toId:" + QString::number(sb->id) + "}";
    }
    segmentsJs += "]";

    QString startCoord = "[0,0]", endCoord = "[0,0]";
    if (sFrom) startCoord = "[" + QString::number(sFrom->lat, 'f', 6) + "," + QString::number(sFrom->lng, 'f', 6) + "]";
    if (sTo)   endCoord   = "[" + QString::number(sTo->lat,   'f', 6) + "," + QString::number(sTo->lng,   'f', 6) + "]";

    runJS("highlightPathByMode(" + segmentsJs + ", " + startCoord + ", " + endCoord + ");");

    // 高亮列表中对应项
    if (m_lstAlternatives && idx < m_lstAlternatives->count()) {
        m_lstAlternatives->setCurrentRow(idx);
    }

    // 在 txtResult 显示当前方案的详细路线
    QString html = "<div style='font-family:Microsoft YaHei,sans-serif; color:#212121;'>";
    if (sFrom && sTo) {
        html += QString("<div style='font-size:13px; margin-bottom:6px;'><b>🚏 %1 → %2</b></div>")
                .arg(sFrom->name.toHtmlEscaped(), sTo->name.toHtmlEscaped());
    }
    html += QString("<div style='color:#666; margin-bottom:8px;'>📊 方案 %1：%2</div>")
            .arg(m_currentAlt + 1).arg(res.summary);
    html += QString("<div>⏱ <b>%1</b> 分钟  ·  📏 <b>%2</b> km  ·  🔄 <b>%3</b> 次换乘</div>")
            .arg(QString::number(res.totalTimeMin, 'f', 1))
            .arg(QString::number(res.totalDistance, 'f', 2))
            .arg(res.transferCount);
    html += "<hr style='border:0; border-top:1px dashed #ccc; margin:8px 0;'/>";
    html += QString("<div style='margin-bottom:4px;'><b>🗺️ 详细路线：</b></div>");

    static const QStringList LEG_COLORS = {
        "#1a73e8","#f57c00","#43a047","#e53935",
        "#8e24aa","#00897b","#5e35b1","#fb8c00"
    };
    auto legColor = [&](int idx2) { return LEG_COLORS.value(idx2 % LEG_COLORS.size()); };
    QString curRouteId;
    for (int i = 0; i < res.stopIds.size(); ++i) {
        const Stop* s = findStop(res.stopIds[i]);
        if (!s) continue;
        if (i == 0) {
            html += QString("<div style='color:#34a853;'><b>🟢 %1</b></div>").arg(s->name.toHtmlEscaped());
            continue;
        }
        QString segRouteId = res.routeIds.value(i - 1);
        QString segRouteName = res.routeNames.value(i - 1);
        QString segMode = res.modes.value(i - 1, "bus");
        QString icon = (segMode == "metro") ? "🚇" : (segMode == "walk" ? "🚶" : "🚌");
        QString color = legColor(legIndex.value(i - 1, 0));
        if (segRouteId != curRouteId) {
            html += QString("<div style='margin:4px 0 2px 12px; color:%1;'><b>%2 %3</b></div>")
                       .arg(color, icon, segRouteName.toHtmlEscaped());
            curRouteId = segRouteId;
        }
        if (i == res.stopIds.size() - 1) {
            html += QString("<div style='color:#ea4335;'><b>🔴 %1</b></div>").arg(s->name.toHtmlEscaped());
        } else {
            html += QString("<div style='margin-left:24px; color:#555;'>· %1</div>").arg(s->name.toHtmlEscaped());
        }
    }
    html += "</div>";

    txtResult->setHtml(html);
    lblStatus->setText(QStringLiteral("方案 %1：%2 | %3 分钟 | %4 次换乘")
        .arg(m_currentAlt + 1).arg(res.summary)
        .arg(QString::number(res.totalTimeMin, 'f', 0)).arg(res.transferCount));
}

// 地图右键点击：取消该站点的选择（如果它是当前起点/终点）
void MainWindow::onStopRightClicked(int stopId)
{
    const Stop* s = findStop(stopId);
    if (!s) return;
    if (stopId == m_startStopId) {
        runJS("setStopSelection(0, 'clearStart');");
        m_startStopId = -1;
        m_clickStep = 0;  // 下次左键重新设为起点
        lblStatus->setText(QStringLiteral("已取消起点：%1").arg(s->name));
    } else if (stopId == m_endStopId) {
        runJS("setStopSelection(0, 'clearEnd');");
        m_endStopId = -1;
        m_clickStep = 1;  // 下次左键重新设为终点
        lblStatus->setText(QStringLiteral("已取消终点：%1").arg(s->name));
    } else {
        // 该站点本来就没被选中，提示一下
        lblStatus->setText(QStringLiteral("%1 不是当前起点/终点").arg(s->name));
    }
}

const Stop* MainWindow::findStop(int id) const
{
    for (const Stop &s : stops)
        if (s.id == id) return &s;
    return nullptr;
}

QString MainWindow::escapeJsString(const QString &s) const
{
    QString result = s;
    result.replace('\\', "\\\\");
    result.replace('\'', "\\'");
    return result;
}

void MainWindow::runJS(const QString &js)
{
    view->page()->runJavaScript(js);
}
