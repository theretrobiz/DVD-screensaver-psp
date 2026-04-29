cmake_minimum_required(VERSION 3.19)


project(dvd_screensaver C)

set(CMAKE_C_STANDARD 11)

add_executable(${PROJECT_NAME} main.c)


target_link_libraries(${PROJECT_NAME}
pspgu
pspgum
pspctrl
pspdisplay
pspge
pspdebug
m
)


target_compile_options(${PROJECT_NAME} PRIVATE
-O2
-G0
-Wall
)



create_pbp_file(
TARGET          ${PROJECT_NAME}
TITLE           "DVD Screensaver"
ICON_PATH       NULL
BACKGROUND_PATH NULL
PREVIEW_PATH    NULL
)
