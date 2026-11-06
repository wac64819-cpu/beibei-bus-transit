#ifndef TRANSITNETWORK_H
#define TRANSITNETWORK_H

#include "Graph.h"
#include "../mainwindow.h"  // 为了使用 Stop / Route 结构
#include <QVector>
#include <QHash>
#include <QPair>

class TransitNetwork
{
public:
    TransitNetwork() = default;

    // 从站点和线路数据构建图
    void build(const QVector<Stop>& stops, const QVector<Route>& routes);

    // 访问内部图
    const Graph& graph() const { return m_graph; }

    // 两个站点的直线距离（km）
    static double haversineDistance(double lng1, double lat1, double lng2, double lat2);

    // 节点坐标查询（A* 启发函数用）
    bool nodePosition(int nodeId, double& outLng, double& outLat) const;

    // 整个网络中最快模式的速度（用于 A* 启发，保证 admissible）
    double fastestSpeedKmh() const { return m_fastestSpeedKmh; }

private:
    Graph m_graph;
    QHash<int, QPair<double, double>> m_nodePos;  // nodeId -> (lng, lat)
    double m_fastestSpeedKmh = 35.0;  // 默认 35 km/h（地铁）
};

#endif // TRANSITNETWORK_H
