#-------------------------------------------------
#
# Project created by QtCreator 2015-02-11T00:01:41
#
#-------------------------------------------------

QT       += core gui concurrent
CONFIG += c++11 sse4.1

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_CXXFLAGS += -msse4.1 -mssse3 -msse3 -msse2 -msse -Wno-missing-braces

TARGET = Terravox
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    brusheditor.cpp \
    terrainview.cpp \
    terrain.cpp \
    session.cpp \
    terrainedit.cpp \
    terraingenerator.cpp \
    labelslider.cpp \
    toolcontroller.cpp \
    brushtool.cpp \
    brushtoolcontroller.cpp \
    brushtoolview.cpp \
    terrainviewoptionswindow.cpp \
    vxl.cpp \
    dithersampler.cpp \
    temporarybuffer.cpp \
    emptytooleditor.cpp \
    coherentnoisegenerator.cpp \
    colorpicker.cpp \
    colorview.cpp \
    colorpickerwindow.cpp \
    colormodelview.cpp \
    effectcontroller.cpp \
    effecteditor.cpp \
    erosioneffect.cpp \
    erosioneffectcontroller.cpp \
    erosioneditor.cpp \
    terrainview_p.cpp \
    terrainview_p_context.cpp \
    terrainview_p_ao.cpp \
    colorsamplerview.cpp \
    manipulatetoolcontroller.cpp \
    manipulatetoolview.cpp

HEADERS  += mainwindow.h \
    brusheditor.h \
    terrainview.h \
    terrain.h \
    session.h \
    terrainedit.h \
    terraingenerator.h \
    labelslider.h \
    toolcontroller.h \
    brushtool.h \
    brushtoolcontroller.h \
    brushtoolview.h \
    terrainviewoptionswindow.h \
    vxl.h \
    dithersampler.h \
    temporarybuffer.h \
    emptytooleditor.h \
    coherentnoisegenerator.h \
    colorpicker.h \
    colorview.h \
    colorpickerwindow.h \
    colormodelview.h \
    effectcontroller.h \
    effecteditor.h \
    erosioneffect.h \
    erosioneffectcontroller.h \
    erosioneditor.h \
    terrainview_p.h \
    colorsamplerview.h \
    manipulatetoolcontroller.h \
    manipulatetoolview.h

FORMS    += mainwindow.ui \
    brusheditor.ui \
    terrainviewoptionswindow.ui \
    emptytooleditor.ui \
    colorpicker.ui \
    colorpickerwindow.ui \
    effecteditor.ui \
    erosioneditor.ui

RESOURCES += \
    res/resources.qrc
