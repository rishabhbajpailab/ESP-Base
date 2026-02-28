idf_component_register(SRCS "app_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES board_waveshare_169 taskmgr ui_app sensors net)
