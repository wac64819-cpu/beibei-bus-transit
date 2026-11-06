#include "PathFinder.h"
#include "TransitMode.h"
#include "TransitNetwork.h"
#include "MinHeap.h"
#include <QHash>
#include <limits>
#include <cmath>

namespace {
    constexpr double INF = std::numeric_limits<double>::infinity();
}

// Dijkstra（带模式感知的换乘代价）
// transferMultiplier: 1.0=正常；> 1.0 表示放大换乘惩罚以达到"最少换乘"目标
PathResult PathFinder::dijkstraWithTransferCost(int from, int to, double transferMultiplier)
{
    PathResult result;
    if (!m_graph || !m_graph->hasNode(from) || !m_graph->hasNode(to)) {
        return result;
    }

    auto& modes = ModeRegistry::instance();

    // Dijkstra 状态：累计代价、当前节点、上一段线路 ID、上一段模式
    struct State {
        double cost;
        int node;
        QString lastRoute;
        QString lastMode;
        // 小顶堆比较器：cost 小的优先出堆
        bool operator<(const State& o) const { return cost < o.cost; }
    };

    // 用手写二叉堆替代 std::priority_queue（数据结构课核心）
    MinHeap<State> pq;

    QHash<int, double> dist;
    QHash<int, int> prev;
    QHash<int, QString> prevRouteName;
    QHash<int, QString> prevRouteId;
    QHash<int, QString> prevMode;

    for (int n : m_graph->allNodes()) dist[n] = INF;
    dist[from] = 0.0;

    pq.push({0.0, from, QString(), QString()});

    int visitedNodes = 0;
    while (!pq.empty()) {
        State cur = pq.pop();  // 手写堆的 pop 同时返回堆顶并删除
        if (cur.cost > dist[cur.node] + 1e-9) continue;
        ++visitedNodes;
        if (cur.node == to) break;

        for (const Edge& e : m_graph->neighbors(cur.node)) {
            // 换乘代价：如果当前边的 routeId 和上一段不同就有惩罚
            double transferCost = 0.0;
            if (!cur.lastRoute.isEmpty() && cur.lastRoute != e.routeId) {
                // 用 ModeRegistry 统一计算（同模式 / 跨模式 给不同代价）
                transferCost = modes.transferCost(cur.lastMode, e.mode) * transferMultiplier;
            }
            double newCost = cur.cost + e.weight + transferCost;
            if (newCost < dist[e.to] - 1e-9) {
                dist[e.to] = newCost;
                prev[e.to] = cur.node;
                prevRouteName[e.to] = e.routeName;
                prevRouteId[e.to] = e.routeId;
                prevMode[e.to] = e.mode;
                pq.push({newCost, e.to, e.routeId, e.mode});
            }
        }
    }

    if (dist[to] == INF) return result;

    // 回溯路径
    QVector<int> stopsRev;
    QVector<QString> routesRev, routeIdsRev, modesRev;
    int cur = to;
    while (cur != from) {
        stopsRev.append(cur);
        routesRev.append(prevRouteName.value(cur));
        routeIdsRev.append(prevRouteId.value(cur));
        modesRev.append(prevMode.value(cur));
        if (!prev.contains(cur)) return result;  // 异常
        cur = prev[cur];
    }
    stopsRev.append(from);

    // 反转
    QVector<int> stops;
    QVector<QString> routes, routeIds, modesArr;
    for (int i = stopsRev.size() - 1; i >= 0; --i) stops.append(stopsRev[i]);
    for (int i = routesRev.size() - 1; i >= 0; --i) routes.append(routesRev[i]);
    for (int i = routeIdsRev.size() - 1; i >= 0; --i) routeIds.append(routeIdsRev[i]);
    for (int i = modesRev.size() - 1; i >= 0; --i) modesArr.append(modesRev[i]);

    // 统计换乘
    int transfers = 0, crossTrans = 0;
    double totalDist = 0.0, totalTime = 0.0;
    // 重新查图计算精确距离/时间（避免回溯时数据丢失）
    for (int i = 1; i < stops.size(); ++i) {
        for (const Edge& e : m_graph->neighbors(stops[i - 1])) {
            if (e.to == stops[i] && e.routeId == routeIds[i - 1]) {
                totalDist += e.distance;
                totalTime += e.weight;
                break;
            }
        }
        if (i >= 2 && routeIds[i - 1] != routeIds[i - 2]) {
            transfers++;
            if (modesArr[i - 1] != modesArr[i - 2]) crossTrans++;
        }
    }

    result.found = true;
    result.stopIds = stops;
    result.routeNames = routes;
    result.routeIds = routeIds;
    result.modes = modesArr;
    result.totalDistance = totalDist;
    result.totalTimeMin = totalTime;
    result.transferCount = transfers;
    result.crossModeTransfers = crossTrans;
    // 手写堆统计
    result.heapPushOps = pq.pushOps();
    result.heapPopOps = pq.popOps();
    result.heapCompareOps = pq.compareOps();
    result.visitedNodes = visitedNodes;
    result.algorithm = QStringLiteral("Dijkstra");
    return result;
}

// ============ A* 启发式搜索 ============
// 思路：在 Dijkstra 基础上把"累计代价 g(n)"换成 "f(n) = g(n) + h(n)"
//      其中 h(n) = 从 n 到终点的直线距离 / 最快速度（admissible heuristic）
//      由于 h(n) 永远 ≤ 真实剩余代价，A* 保证找到最优解，且比 Dijkstra 快
PathResult PathFinder::aStarSearch(int from, int to)
{
    PathResult result;
    if (!m_graph || !m_graph->hasNode(from) || !m_graph->hasNode(to)) return result;

    // 没有网络数据时，A* 退化为 Dijkstra
    if (!m_network) {
        result = dijkstraWithTransferCost(from, to, 1.0);
        result.algorithm = QStringLiteral("A*(degraded to Dijkstra: no coords)");
        return result;
    }

    // 终点坐标 + 最快速度（用于启发函数）
    double goalLng = 0, goalLat = 0;
    if (!m_network->nodePosition(to, goalLng, goalLat)) {
        result = dijkstraWithTransferCost(from, to, 1.0);
        result.algorithm = QStringLiteral("A*(degraded)");
        return result;
    }
    const double bestSpeed = m_network->fastestSpeedKmh();
    constexpr double R = 6371.0;
    constexpr double pi = 3.14159265358979323846;

    // 启发函数：到终点的直线距离按"最快速度"计算的时间下限
    auto heuristic = [&](int node) -> double {
        double lng = 0, lat = 0;
        if (!m_network->nodePosition(node, lng, lat)) return 0.0;
        double r1 = lat  * pi / 180.0;
        double r2 = goalLat * pi / 180.0;
        double dLat = (goalLat - lat) * pi / 180.0;
        double dLng = (goalLng - lng) * pi / 180.0;
        double a = std::sin(dLat/2)*std::sin(dLat/2)
                 + std::cos(r1)*std::cos(r2)*std::sin(dLng/2)*std::sin(dLng/2);
        double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1-a));
        double distKm = R * c;
        // 时间下限 = 直线距离 / 最快速度 * 60（分钟）
        return distKm / bestSpeed * 60.0;
    };

    auto& modes = ModeRegistry::instance();

    struct State {
        double f;          // f = g + h
        double g;          // 累计真实代价
        int node;
        QString lastRoute;
        QString lastMode;
        bool operator<(const State& o) const { return f < o.f; }
    };

    MinHeap<State> pq;

    QHash<int, double> gScore;
    QHash<int, int> prev;
    QHash<int, QString> prevRouteName, prevRouteId, prevMode;
    for (int n : m_graph->allNodes()) gScore[n] = std::numeric_limits<double>::infinity();
    gScore[from] = 0.0;

    pq.push({heuristic(from), 0.0, from, QString(), QString()});

    int visitedNodes = 0;
    bool reached = false;
    while (!pq.empty()) {
        State cur = pq.pop();
        // 因为可能用 lazy delete，比较当前 g 和保存的 g
        if (cur.g > gScore[cur.node] + 1e-9) continue;
        ++visitedNodes;
        if (cur.node == to) { reached = true; break; }

        for (const Edge& e : m_graph->neighbors(cur.node)) {
            double transferCost = 0.0;
            if (!cur.lastRoute.isEmpty() && cur.lastRoute != e.routeId) {
                transferCost = modes.transferCost(cur.lastMode, e.mode);
            }
            double newG = cur.g + e.weight + transferCost;
            if (newG < gScore[e.to] - 1e-9) {
                gScore[e.to] = newG;
                prev[e.to] = cur.node;
                prevRouteName[e.to] = e.routeName;
                prevRouteId[e.to] = e.routeId;
                prevMode[e.to] = e.mode;
                double f = newG + heuristic(e.to);
                pq.push({f, newG, e.to, e.routeId, e.mode});
            }
        }
    }

    if (!reached) {
        return result;  // 不可达
    }

    // 回溯路径
    QVector<int> stopsRev;
    QVector<QString> routesRev, routeIdsRev, modesRev;
    int cur = to;
    while (cur != from) {
        stopsRev.append(cur);
        routesRev.append(prevRouteName.value(cur));
        routeIdsRev.append(prevRouteId.value(cur));
        modesRev.append(prevMode.value(cur));
        if (!prev.contains(cur)) return result;
        cur = prev[cur];
    }
    stopsRev.append(from);

    QVector<int> stops;
    QVector<QString> routes, routeIds, modesArr;
    for (int i = stopsRev.size() - 1; i >= 0; --i) stops.append(stopsRev[i]);
    for (int i = routesRev.size() - 1; i >= 0; --i) routes.append(routesRev[i]);
    for (int i = routeIdsRev.size() - 1; i >= 0; --i) routeIds.append(routeIdsRev[i]);
    for (int i = modesRev.size() - 1; i >= 0; --i) modesArr.append(modesRev[i]);

    int transfers = 0, crossTrans = 0;
    double totalDist = 0.0, totalTime = 0.0;
    for (int i = 1; i < stops.size(); ++i) {
        for (const Edge& e : m_graph->neighbors(stops[i - 1])) {
            if (e.to == stops[i] && e.routeId == routeIds[i - 1]) {
                totalDist += e.distance;
                totalTime += e.weight;
                break;
            }
        }
        if (i >= 2 && routeIds[i - 1] != routeIds[i - 2]) {
            transfers++;
            if (modesArr[i - 1] != modesArr[i - 2]) crossTrans++;
        }
    }

    result.found = true;
    result.stopIds = stops;
    result.routeNames = routes;
    result.routeIds = routeIds;
    result.modes = modesArr;
    result.totalDistance = totalDist;
    result.totalTimeMin = totalTime;
    result.transferCount = transfers;
    result.crossModeTransfers = crossTrans;
    result.heapPushOps = pq.pushOps();
    result.heapPopOps = pq.popOps();
    result.heapCompareOps = pq.compareOps();
    result.visitedNodes = visitedNodes;
    result.algorithm = QStringLiteral("A*");
    return result;
}

PathResult PathFinder::findShortestPathAStar(int from, int to)
{
    return aStarSearch(from, to);
}

PathResult PathFinder::findShortestPath(int from, int to)
{
    return dijkstraWithTransferCost(from, to, 1.0);
}

PathResult PathFinder::findLeastTransfer(int from, int to)
{
    // 放大换乘代价到极端值，迫使算法走"换乘最少"的路径
    return dijkstraWithTransferCost(from, to, 100.0);
}

PathResult PathFinder::findBalanced(int from, int to)
{
    // 综合最优：换乘代价正常 + 时间合理平衡
    return dijkstraWithTransferCost(from, to, 1.0);
}
