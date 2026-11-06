#include "TransitMode.h"

ModeRegistry& ModeRegistry::instance()
{
    static ModeRegistry inst;
    return inst;
}

ModeRegistry::ModeRegistry()
{
    // 内置三个模式
    ModeConfig bus;
    bus.id = "bus";
    bus.displayName = QStringLiteral("公交");
    bus.speedKmh = 20.0;
    bus.sameTransferMin = 5.0;
    bus.crossTransferMin = 8.0;
    bus.pathColor = "#1a73e8";  // 谷歌蓝
    bus.pathWeight = 7;
    registerMode(bus);

    ModeConfig metro;
    metro.id = "metro";
    metro.displayName = QStringLiteral("地铁");
    metro.speedKmh = 35.0;
    metro.sameTransferMin = 3.0;
    metro.crossTransferMin = 8.0;
    metro.pathColor = "#8e24aa";  // 紫色
    metro.pathWeight = 9;
    registerMode(metro);

    ModeConfig walk;
    walk.id = "walk";
    walk.displayName = QStringLiteral("步行");
    walk.speedKmh = 4.0;
    walk.sameTransferMin = 0.0;
    walk.crossTransferMin = 0.0;
    walk.pathColor = "#9e9e9e";  // 灰色
    walk.pathWeight = 4;
    registerMode(walk);

    m_default = bus;
}

void ModeRegistry::registerMode(const ModeConfig& cfg)
{
    m_modes.insert(cfg.id, cfg);
}

const ModeConfig& ModeRegistry::get(const QString& modeId) const
{
    auto it = m_modes.constFind(modeId);
    if (it == m_modes.constEnd()) return m_default;
    return it.value();
}

double ModeRegistry::distanceToTime(double distKm, const QString& modeId) const
{
    const ModeConfig& m = get(modeId);
    if (m.speedKmh <= 0) return distKm * 3.0;  // 防御
    return distKm / m.speedKmh * 60.0;  // 转分钟
}

double ModeRegistry::transferCost(const QString& fromMode, const QString& toMode) const
{
    if (fromMode.isEmpty()) return 0.0;  // 起点
    if (fromMode == toMode) {
        return get(toMode).sameTransferMin;
    }
    // 跨模式换乘：用目标模式的 crossTransferMin
    return get(toMode).crossTransferMin;
}
