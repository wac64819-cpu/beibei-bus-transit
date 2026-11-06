#include "TransitNetwork.h"
#include "TransitMode.h"
#include <cmath>
#include <QHash>
#include <QSet>
#include <QDebug>

bool TransitNetwork::nodePosition(int nodeId, double& outLng, double& outLat) const
{
    auto it = m_nodePos.constFind(nodeId);
    if (it == m_nodePos.constEnd()) return false;
    outLng = it.value().first;
    outLat = it.value().second;
    return true;
}

double TransitNetwork::haversineDistance(double lng1, double lat1, double lng2, double lat2)
{
    constexpr double R = 6371.0;  // 地球半径 km
    constexpr double pi = 3.14159265358979323846;
    double rLat1 = lat1 * pi / 180.0;
    double rLat2 = lat2 * pi / 180.0;
    double dLat = (lat2 - lat1) * pi / 180.0;
    double dLng = (lng2 - lng1) * pi / 180.0;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2)
             + std::cos(rLat1) * std::cos(rLat2)
             * std::sin(dLng / 2) * std::sin(dLng / 2);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return R * c;
}

void TransitNetwork::build(const QVector<Stop>& stops, const QVector<Route>& routes)
{
    m_graph = Graph();
    m_nodePos.clear();

    // 用 hash 表加速根据 stopId 查找 Stop
    QHash<int, const Stop*> stopMap;
    for (const Stop& s : stops) {
        stopMap.insert(s.id, &s);
        m_graph.addNode(s.id);
        m_nodePos.insert(s.id, qMakePair(s.lng, s.lat));
    }

    // 计算最快模式速度（A* 启发函数用，必须 ≥ 任何模式的速度）
    auto& mreg = ModeRegistry::instance();
    m_fastestSpeedKmh = 1.0;
    QSet<QString> usedModes;
    for (const Route& r : routes) {
        usedModes.insert(r.type.isEmpty() ? QStringLiteral("bus") : r.type);
    }
    for (const QString& mid : usedModes) {
        double sp = mreg.get(mid).speedKmh;
        if (sp > m_fastestSpeedKmh) m_fastestSpeedKmh = sp;
    }

    // 遍历每条线路，把相邻站点连成边
    auto& modes = ModeRegistry::instance();
    for (const Route& r : routes) {
        QString mode = r.type.isEmpty() ? QStringLiteral("bus") : r.type;
        for (int i = 0; i + 1 < r.stopSeq.size(); ++i) {
            int a = r.stopSeq[i];
            int b = r.stopSeq[i + 1];
            const Stop* sa = stopMap.value(a, nullptr);
            const Stop* sb = stopMap.value(b, nullptr);
            if (!sa || !sb) continue;
            double distKm = haversineDistance(sa->lng, sa->lat, sb->lng, sb->lat);
            double timeMin = modes.distanceToTime(distKm, mode);
            m_graph.addEdge(a, b, timeMin, distKm, r.id, r.name, mode);
        }
    }

    // ===== 关键：自动添加"步行换乘"边 =====
    // 任何两个站点（不同 stop_id）只要距离 <= TRANSFER_THRESHOLD_KM
    // 就在它们之间加一条 mode=walk 的边
    // 这样公交站和地铁站才会被联通成一个连通图
    constexpr double TRANSFER_THRESHOLD_KM = 0.8;  // 800 米内可步行换乘（含跨线换乘）
    int walkEdges = 0;
    for (int i = 0; i < stops.size(); ++i) {
        for (int j = i + 1; j < stops.size(); ++j) {
            const Stop& a = stops[i];
            const Stop& b = stops[j];
            if (a.id == b.id) continue;
            double distKm = haversineDistance(a.lng, a.lat, b.lng, b.lat);
            if (distKm > TRANSFER_THRESHOLD_KM) continue;
            // 步行时间
            double walkTime = modes.distanceToTime(distKm, "walk");
            m_graph.addEdge(a.id, b.id, walkTime, distKm,
                            "WALK", QStringLiteral("步行"), "walk");
            walkEdges++;
        }
    }
    // 调试输出
    qDebug() << "TransitNetwork built:"
             << "stops=" << stops.size()
             << "routes=" << routes.size()
             << "walk-transfer edges=" << walkEdges;
}
