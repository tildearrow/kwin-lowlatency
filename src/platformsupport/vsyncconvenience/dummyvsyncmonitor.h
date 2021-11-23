/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "vsyncmonitor.h"

#include <QTimer>

namespace KWin
{

/**
 * The DummyVsyncMonitor class provides synthetic vblank events as soon as it is armed.
 */
class KWIN_EXPORT DummyVsyncMonitor : public VsyncMonitor
{
    Q_OBJECT

public:
    static DummyVsyncMonitor *create(QObject *parent);

    int refreshRate() const;
    void setRefreshRate(int refreshRate);

public Q_SLOTS:
    void arm() override;

private:
    explicit DummyVsyncMonitor(QObject *parent = nullptr);

    int m_refreshRate = 60000;
    std::chrono::nanoseconds m_vblankTimestamp = std::chrono::nanoseconds::zero();
};

} // namespace KWin
