SET(SOURCES screens/ui_Main.c
    screens/ui_Pedal.c
    screens/ui_Tuner.c
    ui_theme_manager.c
    ui_themes.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_lineasset_png.c
    images/ui_img_tunerasset_png.c
    images/ui_img_flatindicator_png.c
    fonts/ui_font_IndexFont.c)

add_library(ui ${SOURCES})
