#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <memory>
#include <vector>
#include "CurveEngine.h"

namespace MotorStudio {

// 曲线渲染器（基于 QOpenGLWidget）
class CurveRenderer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit CurveRenderer(QWidget* parent = nullptr);
    ~CurveRenderer() override;

    // 设置曲线引擎
    void setEngine(CurveEngine* engine);

    // 交互控制
    void pause();
    void resume();
    bool isPaused() const;

    // 缩放
    void zoomIn();
    void zoomOut();
    void zoomFit();

    // 导出
    void exportCSV(const std::string& filePath);
    void exportScreenshot(const std::string& filePath);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio