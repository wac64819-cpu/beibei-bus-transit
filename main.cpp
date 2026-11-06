#include "mainwindow.h"
#include <QApplication>
#include <QByteArray>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>

int main(int argc, char *argv[])
{
    // ===== VPN 兼容化设置（必须在 QApplication 之前）=====
    // 1. WebEngine：强制无代理 + 关闭内置异步 DNS（让 Clash 看到域名做规则匹配）
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--no-proxy-server "
            "--proxy-bypass-list=* "
            "--ignore-certificate-errors "
            "--disable-features=BlockInsecurePrivateNetworkRequests,AsyncDns "
            "--disable-async-dns "
            "--disable-http-cache");
    // 2. Qt 网络库也强制无代理
    qputenv("no_proxy", "*");
    qputenv("NO_PROXY", "*");
    // 3. 开启 WebEngine 远程调试，方便排查
    qputenv("QTWEBENGINE_REMOTE_DEBUGGING", "9222");

    QApplication app(argc, argv);
    // 4. Qt 应用全局禁用代理
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    QNetworkProxyFactory::setUseSystemConfiguration(false);

    MainWindow w;
    w.setWindowTitle(QStringLiteral("重庆北碚公交换乘系统"));
    w.resize(1200, 800);
    w.show();
    return app.exec();
}
