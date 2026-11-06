#ifndef GRAPH_H
#define GRAPH_H

#include <QVector>
#include <QString>
#include <QHash>

// 图的边（多模态）
struct Edge {
    int to;            // 目标节点（站点 ID）
    double weight;     // 权重：时间（分钟）
    double distance;   // 物理距离（km），保留用于显示
    QString routeId;   // 所属线路 ID
    QString routeName; // 线路显示名（"520路" "轨道6号线"）
    QString mode;      // 交通模式 ID："bus" / "metro" / "walk" / ...

    Edge() : to(-1), weight(0.0), distance(0.0) {}
    Edge(int t, double w, double d, const QString& rid, const QString& rname, const QString& m)
        : to(t), weight(w), distance(d), routeId(rid), routeName(rname), mode(m) {}
};

// 邻接表表示的图（手写，体现数据结构课程功底）
class Graph
{
public:
    Graph() = default;

    // 添加节点（站点）
    void addNode(int nodeId);

    // 添加无向边（公交线路通常双向通行）
    void addEdge(int from, int to, double weight, double distance,
                 const QString& routeId, const QString& routeName, const QString& mode);

    // 获取节点数
    int nodeCount() const { return m_adj.size(); }

    // 获取某节点的所有出边
    const QVector<Edge>& neighbors(int nodeId) const;

    // 节点是否存在
    bool hasNode(int nodeId) const { return m_adj.contains(nodeId); }

    // 获取所有节点 ID
    QList<int> allNodes() const { return m_adj.keys(); }

private:
    // 邻接表：节点 ID -> 出边数组
    QHash<int, QVector<Edge>> m_adj;
    QVector<Edge> m_emptyEdges; // 用于返回空引用
};

#endif // GRAPH_H
