#######################################
# Effect

set(mousepos_SOURCES
    main.cpp
    mousepos.cpp
    mousepos.qrc
)

kconfig_add_kcfg_files(mousepos_SOURCES
    mouseposconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_mousepos ${mousepos_SOURCES})

#######################################
# Config
set(kwin_mousepos_config_SRCS mousepos_config.cpp)
ki18n_wrap_ui(kwin_mousepos_config_SRCS mousepos_config.ui)
kconfig_add_kcfg_files(kwin_mousepos_config_SRCS mouseposconfig.kcfgc)

kwin_add_effect_config(kwin_mousepos_config ${kwin_mousepos_config_SRCS})

target_link_libraries(kwin_mousepos_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)
