/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dummyvsyncmonitor.h"

namespace KWin
{

DummyVsyncMonitor *DummyVsyncMonitor::create(QObject *parent)
{
    return new DummyVsyncMonitor(parent);
}

DummyVsyncMonitor::DummyVsyncMonitor(QObject *parent)
    : VsyncMonitor(parent)
{
}

int DummyVsyncMonitor::refreshRate() const
{
    return m_refreshRate;
}

void DummyVsyncMonitor::setRefreshRate(int refreshRate)
{
    m_refreshRate = refreshRate;
}

template <typename T>
T alignTimestamp(const T &timestamp, const T &alignment)
{
    return timestamp + ((alignment - (timestamp % alignment)) % alignment);
}

void DummyVsyncMonitor::arm()
{
    const std::chrono::nanoseconds currentTime(std::chrono::steady_clock::now().time_since_epoch());
    const std::chrono::nanoseconds vblankInterval(1'000'000'000'000ull / m_refreshRate);

    m_vblankTimestamp = alignTimestamp(currentTime, vblankInterval);

    Q_EMIT vblankOccurred(m_vblankTimestamp);
}

} // namespace KWin
