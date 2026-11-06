#ifndef TRANSITMODE_H
#define TRANSITMODE_H

#include <QString>
#include <QHash>

/**
 * 交通模式定义（可插拔）
 *
 * 添加新模式只需在 ModeRegistry::instance() 里 register 一个 ModeConfig，
 * 算法层无需改动——这就是"可扩展架构"的关键。
 *
 * 当前支持：bus / metro / walk
 * 可未来扩展：tram / brt / cable_car / bike_share ...
 */
struct ModeConfig {
    QString id;              // "bus" / "metro" / "walk"
    QString displayName;     // "公交" / "地铁" / "步行"
    double speedKmh;         // 平均速度 (km/h)
    double sameTransferMin;  // 同模式换乘代价（分钟）
    double crossTransferMin; // 跨模式换乘代价（分钟，进出站时间）
    QString pathColor;       // 在地图上画路径的颜色
    int pathWeight;          // 画线粗细

    ModeConfig() : speedKmh(20), sameTransferMin(5), crossTransferMin(8), pathWeight(6) {}
};

class ModeRegistry
{
public:
    static ModeRegistry& instance();

    // 注册一个模式
    void registerMode(const ModeConfig& cfg);

    // 获取模式配置（不存在则返回默认 bus 配置）
    const ModeConfig& get(const QString& modeId) const;

    // 把"距离 km"转成"时间分钟"
    double distanceToTime(double distKm, const QString& modeId) const;

    // 两个模式之间的换乘代价（分钟）
    double transferCost(const QString& fromMode, const QString& toMode) const;

private:
    ModeRegistry();
    QHash<QString, ModeConfig> m_modes;
    ModeConfig m_default;
};

#endif // TRANSITMODE_H
