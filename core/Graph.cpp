#include "Graph.h"

void Graph::addNode(int nodeId)
{
    if (!m_adj.contains(nodeId)) {
        m_adj.insert(nodeId, QVector<Edge>());
    }
}

void Graph::addEdge(int from, int to, double weight, double distance,
                    const QString& routeId, const QString& routeName, const QString& mode)
{
    addNode(from);
    addNode(to);
    // 无向图：双向都加边
    m_adj[from].append(Edge(to, weight, distance, routeId, routeName, mode));
    m_adj[to].append(Edge(from, weight, distance, routeId, routeName, mode));
}

const QVector<Edge>& Graph::neighbors(int nodeId) const
{
    auto it = m_adj.constFind(nodeId);
    if (it == m_adj.constEnd()) return m_emptyEdges;
    return it.value();
}
