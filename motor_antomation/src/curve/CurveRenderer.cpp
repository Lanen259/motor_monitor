#include "CurveRenderer.h"

namespace MotorStudio {

struct CurveRenderer::Impl {
    CurveEngine* engine = nullptr;
    bool paused = false;
};

CurveRenderer::CurveRenderer(QWidget* parent)
    : QOpenGLWidget(parent), d(std::make_unique<Impl>()) {}

CurveRenderer::~CurveRenderer() = default;

void CurveRenderer::setEngine(CurveEngine* engine) {
    d->engine = engine;
}

void CurveRenderer::pause() { d->paused = true; }
void CurveRenderer::resume() { d->paused = false; }
bool CurveRenderer::isPaused() const { return d->paused; }

void CurveRenderer::zoomIn() {}
void CurveRenderer::zoomOut() {}
void CurveRenderer::zoomFit() {}

void CurveRenderer::exportCSV(const std::string& filePath) {}
void CurveRenderer::exportScreenshot(const std::string& filePath) {}

void CurveRenderer::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void CurveRenderer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void CurveRenderer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    // TODO: 绘制曲线
}

} // namespace MotorStudio