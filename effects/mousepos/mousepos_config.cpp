/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010 Jorge Mata <matamax123@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include <QAction>
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <KLocalizedString>
#include <KActionCollection>
#include <KAboutData>
#include <KGlobalAccel>
#include <KPluginFactory>

#include <QVBoxLayout>
#include <QLabel>

#include "mousepos_config.h"

// KConfigSkeleton
#include "mouseposconfig.h"

K_PLUGIN_FACTORY_WITH_JSON(MousePosEffectConfigFactory,
                           "mousepos_config.json",
                           registerPlugin<KWin::MousePosEffectConfig>();)

namespace KWin
{

MousePosEffectConfigForm::MousePosEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

MousePosEffectConfig::MousePosEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("mousepos")), parent, args)
{
    MousePosConfig::instance(KWIN_CONFIG);
    m_ui = new MousePosEffectConfigForm(this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    addConfig(MousePosConfig::self(), m_ui);

    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));
    m_actionCollection->setConfigGroup(QStringLiteral("MousePos"));
    m_actionCollection->setConfigGlobal(true);

    QAction *a = m_actionCollection->addAction(QStringLiteral("MousePos"));
    a->setText(i18n("Paint cursor"));
    a->setProperty("isConfigurationAction", true);

    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());

    connect(m_ui->kcfg_mpMotionBlur, SIGNAL(keySequenceChanged(QKeySequence)),
                            SLOT(shortcutChanged(QKeySequence)));

    load();
}

MousePosEffectConfig::~MousePosEffectConfig()
{
}

void MousePosEffectConfig::checkModifiers()
{
}

void MousePosEffectConfig::load()
{
    KCModule::load();

    checkModifiers();
    emit changed(false);
}

void MousePosEffectConfig::save()
{
    KCModule::save();
    m_actionCollection->writeSettings();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("mousepos"));
}

void MousePosEffectConfig::defaults()
{
    KCModule::defaults();
    checkModifiers();
}

void MousePosEffectConfig::shortcutChanged(const QKeySequence &seq)
{
    /*if (QAction *a = m_actionCollection->action(QStringLiteral("MousePos"))) {
        KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << seq, KGlobalAccel::NoAutoloading);
    }*/
//     m_actionCollection->writeSettings();
    emit changed(true);
}

} // namespace

#include "mousepos_config.moc"
