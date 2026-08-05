QT       += core gui widgets serialport serialbus network opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Qt 5.14 MinGW 7.3 兼容
win32-g++ {
    QMAKE_CXXFLAGS += -std=c++17
}

DEFINES += QT_DEPRECATED_WARNINGS

# ============================================================
# 源文件（递归包含 src/ 下所有 .cpp）
# ============================================================
SOURCES += \
    main.cpp \
    mainwindow.cpp

SOURCES += $$files(src/*.cpp, true)

# ============================================================
# 头文件（递归包含 src/ 下所有 .h）
# ============================================================
HEADERS += \
    mainwindow.h

HEADERS += $$files(src/*.h, true)

# ============================================================
# UI 文件
# ============================================================
FORMS += \
    mainwindow.ui

# ============================================================
# 包含路径
# ============================================================
INCLUDEPATH += \
    src

# ============================================================
# 安装规则
# ============================================================
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target