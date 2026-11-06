#ifndef MINHEAP_H
#define MINHEAP_H

#include <vector>
#include <functional>
#include <stdexcept>
#include <utility>

/**
 * 手写二叉小顶堆（Binary Min-Heap）
 *
 * 数据结构课程核心内容：
 *   - 用一维数组存储完全二叉树
 *   - 满足"堆性质"：父节点的值 <= 任一子节点的值
 *   - 顶部（下标 0）始终是当前最小元素
 *
 * 节点位置关系（0-based 下标）：
 *   - 节点 i 的父节点：(i - 1) / 2
 *   - 节点 i 的左子：2*i + 1
 *   - 节点 i 的右子：2*i + 2
 *
 * 复杂度：
 *   - push():       O(log n)   —— 从尾部上浮
 *   - pop()/top():  O(log n) / O(1)
 *   - 构造 n 个元素：O(n log n)（逐个插入）
 *
 * 模板参数：
 *   T        - 元素类型
 *   Compare  - 比较函数（默认 std::less<T>，让较小的元素优先）
 *
 * 用法示例：
 *   MinHeap<int> heap;
 *   heap.push(5); heap.push(2); heap.push(8);
 *   while (!heap.empty()) { cout << heap.pop() << " "; }
 *   // 输出: 2 5 8
 *
 * 用于 Dijkstra：传入自定义 Compare，比较"路径累计代价"
 */
template <typename T, typename Compare = std::less<T>>
class MinHeap
{
public:
    MinHeap() = default;
    explicit MinHeap(const Compare& comp) : m_comp(comp) {}

    // 入堆：把元素加到尾部，然后向上调整（sift up）
    void push(const T& value) {
        m_data.push_back(value);
        siftUp(static_cast<int>(m_data.size()) - 1);
        ++m_pushOps;
    }

    void push(T&& value) {
        m_data.push_back(std::move(value));
        siftUp(static_cast<int>(m_data.size()) - 1);
        ++m_pushOps;
    }

    // 出堆：返回最小元素（堆顶），用尾部元素替换后向下调整（sift down）
    T pop() {
        if (m_data.empty()) {
            throw std::out_of_range("MinHeap::pop on empty heap");
        }
        T topVal = std::move(m_data.front());
        m_data.front() = std::move(m_data.back());
        m_data.pop_back();
        if (!m_data.empty()) {
            siftDown(0);
        }
        ++m_popOps;
        return topVal;
    }

    const T& top() const {
        if (m_data.empty()) {
            throw std::out_of_range("MinHeap::top on empty heap");
        }
        return m_data.front();
    }

    bool empty() const { return m_data.empty(); }
    size_t size() const { return m_data.size(); }
    void clear() { m_data.clear(); }

    // 统计接口（用于性能分析）
    long long pushOps() const { return m_pushOps; }
    long long popOps()  const { return m_popOps; }
    long long compareOps() const { return m_compareOps; }
    void resetStats() { m_pushOps = m_popOps = m_compareOps = 0; }

private:
    // 上浮：把下标 i 的元素与父节点比较，如果更小就交换，循环至堆顶或满足堆性质
    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            ++m_compareOps;
            if (m_comp(m_data[i], m_data[parent])) {
                std::swap(m_data[i], m_data[parent]);
                i = parent;
            } else {
                break;
            }
        }
    }

    // 下沉：把下标 i 的元素与较小的子节点比较，如果更大就交换，循环至叶子或满足堆性质
    void siftDown(int i) {
        int n = static_cast<int>(m_data.size());
        while (true) {
            int left  = 2 * i + 1;
            int right = 2 * i + 2;
            int smallest = i;
            if (left < n) {
                ++m_compareOps;
                if (m_comp(m_data[left], m_data[smallest])) smallest = left;
            }
            if (right < n) {
                ++m_compareOps;
                if (m_comp(m_data[right], m_data[smallest])) smallest = right;
            }
            if (smallest == i) break;
            std::swap(m_data[i], m_data[smallest]);
            i = smallest;
        }
    }

    std::vector<T> m_data;
    Compare m_comp;

    // 性能统计（可用于答辩演示）
    long long m_pushOps = 0;
    long long m_popOps = 0;
    long long m_compareOps = 0;
};

#endif // MINHEAP_H
