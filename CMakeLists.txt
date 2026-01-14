cmake_minimum_required(VERSION 3.16)
project(fractal_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPFR REQUIRED mpfr)

add_executable(fractal_viewer
    main.cpp
    types.h
    utils.h
    landingpage.h
    fractal2d.h
    # fractal3d.h
)

target_include_directories(fractal_viewer PRIVATE ${MPFR_INCLUDE_DIRS})

target_link_libraries(fractal_viewer PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    ${MPFR_LIBRARIES}
)
