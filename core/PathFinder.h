#ifndef PATHFINDER_H
#define PATHFINDER_H

#include "Graph.h"
#include <QVector>
#include <QString>

class TransitNetwork;  // 前向声明

// 路径查询结果
struct PathResult {
    bool found;                    // 是否找到路径
    QVector<int> stopIds;          // 经过的站点 ID 序列
    QVector<QString> routeNames;   // 每段使用的线路名（长度 = stopIds.size() - 1）
    QVector<QString> routeIds;     // 每段的 route ID
    QVector<QString> modes;        // 每段的模式（"bus"/"metro"/...）
    double totalDistance;          // 总距离 (km)
    double totalTimeMin;           // 总时间（分钟）
    int transferCount;             // 换乘次数
    int crossModeTransfers;        // 跨模式换乘次数（公交↔地铁等）
    QString summary;               // 文字描述

    // 性能统计（手写堆操作次数）—— 答辩演示用
    long long heapPushOps = 0;
    long long heapPopOps = 0;
    long long heapCompareOps = 0;
    int       visitedNodes = 0;        // 实际访问的节点数
    QString   algorithm;               // "Dijkstra" / "A*"

    PathResult() : found(false), totalDistance(0.0), totalTimeMin(0.0),
                   transferCount(0), crossModeTransfers(0) {}
};

// 优化目标
enum class OptimizeMode {
    ShortestTime,    // 最短时间（用距离代理）
    LeastTransfer,   // 最少换乘
    Balanced         // 综合最优
};

class PathFinder
{
public:
    explicit PathFinder(const Graph* graph, const TransitNetwork* network = nullptr)
        : m_graph(graph), m_network(network) {}

    // ===== Dijkstra 系列 =====
    PathResult findShortestPath(int from, int to);
    PathResult findLeastTransfer(int from, int to);
    PathResult findBalanced(int from, int to);

    // ===== A* 启发式搜索 =====
    // 与 Dijkstra 等价（找最短时间），但用地理直线距离做启发函数加速
    PathResult findShortestPathAStar(int from, int to);

    // 设置算法（外部 UI 用）
    enum class Algo { Dijkstra, AStar };

private:
    const Graph* m_graph;
    const TransitNetwork* m_network;  // 用于 A* 查询节点坐标和最快速度

    // 内部通用 Dijkstra 实现
    PathResult dijkstraWithTransferCost(int from, int to, double transferMultiplier);

    // A* 实现
    PathResult aStarSearch(int from, int to);
};

#endif // PATHFINDER_H
