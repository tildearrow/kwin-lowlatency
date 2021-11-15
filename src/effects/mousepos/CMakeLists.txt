#######################################
# Config
set(kwin_mousepos_config_SRCS mousepos_config.cpp)
ki18n_wrap_ui(kwin_mousepos_config_SRCS mousepos_config.ui)
qt5_add_dbus_interface(kwin_mousepos_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_mousepos_config_SRCS mouseposconfig.kcfgc)

add_library(kwin_mousepos_config MODULE ${kwin_mousepos_config_SRCS})

target_link_libraries(kwin_mousepos_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::Service
    KF5::XmlGui
)

kcoreaddons_desktop_to_json(kwin_mousepos_config mousepos_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_mousepos_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
