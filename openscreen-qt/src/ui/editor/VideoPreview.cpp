#include "VideoPreview.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QFileInfo>

#include <cmath>

namespace openscreen {

// ---------------------------------------------------------------------------
// Shader sources
// ---------------------------------------------------------------------------

static const char* kTexturedVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 uProjection;
uniform mat4 uModel;

out vec2 vTexCoord;

void main() {
    gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* kTexturedFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform float uBorderRadius;
uniform vec2 uRectSize;
uniform vec2 uRectCenter;

out vec4 fragColor;

float roundedBoxSDF(vec2 center, vec2 halfSize, float radius) {
    vec2 d = abs(center) - halfSize + vec2(radius);
    return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0) - radius;
}

void main() {
    vec4 color = texture(uTexture, vTexCoord);

    if (uBorderRadius > 0.0) {
        vec2 pixelPos = gl_FragCoord.xy;
        vec2 halfSize = uRectSize * 0.5;
        float dist = roundedBoxSDF(pixelPos - uRectCenter, halfSize, uBorderRadius);
        float alpha = 1.0 - smoothstep(-1.0, 1.0, dist);
        color.a *= alpha;
    }

    fragColor = color;
}
)";

static const char* kSolidVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 uProjection;
uniform mat4 uModel;

void main() {
    gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
}
)";

static const char* kSolidFragmentShader = R"(
#version 330 core
uniform vec4 uColor;

out vec4 fragColor;

void main() {
    fragColor = uColor;
}
)";

// ---------------------------------------------------------------------------
// Unit quad: position (x,y) + texcoord (u,v)
// ---------------------------------------------------------------------------

static const float kQuadVertices[] = {
    // pos        // texcoord
    0.0f, 0.0f,   0.0f, 1.0f,  // bottom-left  (flipped V for image coords)
    1.0f, 0.0f,   1.0f, 1.0f,  // bottom-right
    1.0f, 1.0f,   1.0f, 0.0f,  // top-right
    0.0f, 0.0f,   0.0f, 1.0f,  // bottom-left
    1.0f, 1.0f,   1.0f, 0.0f,  // top-right
    0.0f, 1.0f,   0.0f, 0.0f,  // top-left
};

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

VideoPreview::VideoPreview(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

VideoPreview::~VideoPreview()
{
    makeCurrent();
    videoTexture_.reset();
    wallpaperTexture_.reset();
    texturedShader_.reset();
    solidShader_.reset();
    vbo_.destroy();
    vao_.destroy();
    doneCurrent();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void VideoPreview::setFrame(const uint8_t* bgraData, int width, int height, int stride)
{
    if (!bgraData || width <= 0 || height <= 0) {
        return;
    }

    // QImage::Format_ARGB32 is BGRA in memory on little-endian systems
    pendingFrame_ = QImage(bgraData, width, height, stride, QImage::Format_ARGB32).copy();
    videoWidth_ = width;
    videoHeight_ = height;
    frameDirty_ = true;
    update();
}

void VideoPreview::setFrame(const QImage& image)
{
    if (image.isNull()) {
        return;
    }

    pendingFrame_ = image.convertToFormat(QImage::Format_RGBA8888);
    videoWidth_ = image.width();
    videoHeight_ = image.height();
    frameDirty_ = true;
    update();
}

void VideoPreview::setWallpaper(const QString& wallpaper)
{
    if (wallpaper == currentWallpaper_) {
        return;
    }
    currentWallpaper_ = wallpaper;

    if (wallpaper.startsWith('#')) {
        // Solid color
        solidColor_ = QColor(wallpaper);
        wallpaperIsSolid_ = true;
        wallpaperIsImage_ = false;
        wallpaperTexture_.reset();
    } else if (wallpaper.startsWith('/') || wallpaper.startsWith(':') ||
               QFileInfo::exists(wallpaper)) {
        // Image file or Qt resource
        QImage img(wallpaper);
        if (!img.isNull()) {
            makeCurrent();
            wallpaperTexture_ = std::make_unique<QOpenGLTexture>(
                img.mirrored().convertToFormat(QImage::Format_RGBA8888));
            wallpaperTexture_->setMinificationFilter(QOpenGLTexture::Linear);
            wallpaperTexture_->setMagnificationFilter(QOpenGLTexture::Linear);
            wallpaperTexture_->setWrapMode(QOpenGLTexture::ClampToEdge);
            doneCurrent();
            wallpaperIsImage_ = true;
            wallpaperIsSolid_ = false;
        } else {
            // Fallback to black if image failed to load
            solidColor_ = Qt::black;
            wallpaperIsSolid_ = true;
            wallpaperIsImage_ = false;
        }
    } else {
        // Gradient string — solid color fallback for now (Phase 5)
        solidColor_ = Qt::black;
        wallpaperIsSolid_ = true;
        wallpaperIsImage_ = false;
    }

    update();
}

void VideoPreview::setZoom(double scale, double focusX, double focusY, double progress)
{
    zoomScale_ = scale;
    focusX_ = focusX;
    focusY_ = focusY;
    zoomProgress_ = progress;
    update();
}

void VideoPreview::resetZoom()
{
    zoomScale_ = 1.0;
    focusX_ = 0.5;
    focusY_ = 0.5;
    zoomProgress_ = 1.0;
    update();
}

void VideoPreview::setPadding(double padding)
{
    padding_ = padding;
    update();
}

void VideoPreview::setBorderRadius(double radius)
{
    borderRadius_ = radius;
    update();
}

void VideoPreview::setCropRegion(double x, double y, double w, double h)
{
    cropX_ = x;
    cropY_ = y;
    cropW_ = w;
    cropH_ = h;
    update();
}

QSize VideoPreview::videoSize() const
{
    return {videoWidth_, videoHeight_};
}

// ---------------------------------------------------------------------------
// OpenGL lifecycle
// ---------------------------------------------------------------------------

void VideoPreview::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    setupShaders();
    setupGeometry();
}

void VideoPreview::resizeGL(int /*w*/, int /*h*/)
{
    // Projection matrix is rebuilt each frame in paintGL
}

void VideoPreview::paintGL()
{
    const auto dpr = devicePixelRatioF();
    const int fbW = static_cast<int>(width() * dpr);
    const int fbH = static_cast<int>(height() * dpr);
    glViewport(0, 0, fbW, fbH);

    glClear(GL_COLOR_BUFFER_BIT);

    // Build orthographic projection in widget (logical) coordinates
    QMatrix4x4 projection;
    projection.ortho(0.0f, static_cast<float>(width()),
                     0.0f, static_cast<float>(height()),
                     -1.0f, 1.0f);

    // Store projection for sub-render calls
    texturedShader_->bind();
    texturedShader_->setUniformValue("uProjection", projection);
    texturedShader_->release();

    solidShader_->bind();
    solidShader_->setUniformValue("uProjection", projection);
    solidShader_->release();

    renderWallpaper();
    renderVideoFrame();
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void VideoPreview::mousePressEvent(QMouseEvent* event)
{
    if (videoWidth_ <= 0 || videoHeight_ <= 0) {
        QOpenGLWidget::mousePressEvent(event);
        return;
    }

    const QRectF videoRect = computeVideoRect();
    if (videoRect.isEmpty()) {
        QOpenGLWidget::mousePressEvent(event);
        return;
    }

    const auto pos = event->position();
    const double nx = (pos.x() - videoRect.x()) / videoRect.width();
    const double ny = (pos.y() - videoRect.y()) / videoRect.height();

    if (nx >= 0.0 && nx <= 1.0 && ny >= 0.0 && ny <= 1.0) {
        emit focusClicked(nx, ny);
    }

    QOpenGLWidget::mousePressEvent(event);
}

// ---------------------------------------------------------------------------
// Setup helpers
// ---------------------------------------------------------------------------

void VideoPreview::setupShaders()
{
    // --- Textured shader ---
    texturedShader_ = std::make_unique<QOpenGLShaderProgram>();
    texturedShader_->addShaderFromSourceCode(QOpenGLShader::Vertex, kTexturedVertexShader);
    texturedShader_->addShaderFromSourceCode(QOpenGLShader::Fragment, kTexturedFragmentShader);
    texturedShader_->link();

    // --- Solid color shader ---
    solidShader_ = std::make_unique<QOpenGLShaderProgram>();
    solidShader_->addShaderFromSourceCode(QOpenGLShader::Vertex, kSolidVertexShader);
    solidShader_->addShaderFromSourceCode(QOpenGLShader::Fragment, kSolidFragmentShader);
    solidShader_->link();
}

void VideoPreview::setupGeometry()
{
    vao_.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&vao_);

    vbo_.create();
    vbo_.bind();
    vbo_.allocate(kQuadVertices, sizeof(kQuadVertices));

    // position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));

    // texcoord attribute (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));

    vbo_.release();
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void VideoPreview::renderWallpaper()
{
    QMatrix4x4 model;
    model.translate(0.0f, 0.0f);
    model.scale(static_cast<float>(width()), static_cast<float>(height()));

    if (wallpaperIsImage_ && wallpaperTexture_) {
        texturedShader_->bind();
        texturedShader_->setUniformValue("uModel", model);
        texturedShader_->setUniformValue("uBorderRadius", 0.0f);
        texturedShader_->setUniformValue("uTexture", 0);

        wallpaperTexture_->bind(0);

        QOpenGLVertexArrayObject::Binder vaoBinder(&vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        wallpaperTexture_->release();
        texturedShader_->release();
    } else {
        // Solid color (also used as gradient fallback)
        solidShader_->bind();
        solidShader_->setUniformValue("uModel", model);
        solidShader_->setUniformValue("uColor",
            QVector4D(static_cast<float>(solidColor_.redF()),
                      static_cast<float>(solidColor_.greenF()),
                      static_cast<float>(solidColor_.blueF()),
                      static_cast<float>(solidColor_.alphaF())));

        QOpenGLVertexArrayObject::Binder vaoBinder(&vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        solidShader_->release();
    }
}

void VideoPreview::renderVideoFrame()
{
    if (videoWidth_ <= 0 || videoHeight_ <= 0) {
        return;
    }

    // Upload pending frame data to GPU
    if (frameDirty_) {
        updateVideoTexture();
        frameDirty_ = false;
    }

    if (!videoTexture_) {
        return;
    }

    // Compute where the video sits (with padding & aspect ratio)
    const QRectF videoRect = computeVideoRect();
    if (videoRect.isEmpty()) {
        return;
    }

    // Compute zoom transform
    // Interpolate effective scale: 1.0 -> zoomScale_ based on zoomProgress_
    const double effectiveScale = 1.0 + (zoomScale_ - 1.0) * zoomProgress_;

    // Compute the center of the zoom in widget coordinates
    const double centerX = videoRect.x() + videoRect.width() * focusX_;
    const double centerY = videoRect.y() + videoRect.height() * focusY_;

    // Build model matrix: translate to focus, scale, translate back, then position
    QMatrix4x4 model;
    model.translate(static_cast<float>(centerX), static_cast<float>(centerY));
    model.scale(static_cast<float>(effectiveScale), static_cast<float>(effectiveScale));
    model.translate(static_cast<float>(-centerX), static_cast<float>(-centerY));
    model.translate(static_cast<float>(videoRect.x()), static_cast<float>(videoRect.y()));
    model.scale(static_cast<float>(videoRect.width()), static_cast<float>(videoRect.height()));

    // Compute rect center and size in framebuffer coordinates for the SDF
    const auto dpr = devicePixelRatioF();
    const auto rectCenterX = static_cast<float>((videoRect.x() + videoRect.width() * 0.5) * dpr);
    const auto rectCenterY = static_cast<float>((videoRect.y() + videoRect.height() * 0.5) * dpr);
    const auto rectW = static_cast<float>(videoRect.width() * dpr);
    const auto rectH = static_cast<float>(videoRect.height() * dpr);

    texturedShader_->bind();
    texturedShader_->setUniformValue("uModel", model);
    texturedShader_->setUniformValue("uBorderRadius", static_cast<float>(borderRadius_ * dpr));
    texturedShader_->setUniformValue("uRectSize", QVector2D(rectW, rectH));
    texturedShader_->setUniformValue("uRectCenter", QVector2D(rectCenterX, rectCenterY));
    texturedShader_->setUniformValue("uTexture", 0);

    videoTexture_->bind(0);

    QOpenGLVertexArrayObject::Binder vaoBinder(&vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    videoTexture_->release();
    texturedShader_->release();
}

void VideoPreview::updateVideoTexture()
{
    if (pendingFrame_.isNull()) {
        return;
    }

    const QImage frame = pendingFrame_.convertToFormat(QImage::Format_RGBA8888);

    if (!videoTexture_ ||
        videoTexture_->width() != frame.width() ||
        videoTexture_->height() != frame.height()) {
        // Recreate texture at new size
        videoTexture_ = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        videoTexture_->setSize(frame.width(), frame.height());
        videoTexture_->setFormat(QOpenGLTexture::RGBA8_UNorm);
        videoTexture_->setMinificationFilter(QOpenGLTexture::Linear);
        videoTexture_->setMagnificationFilter(QOpenGLTexture::Linear);
        videoTexture_->setWrapMode(QOpenGLTexture::ClampToEdge);
        videoTexture_->allocateStorage();
    }

    videoTexture_->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, frame.constBits());

    // Clear pending frame to free memory
    pendingFrame_ = QImage();
}

QRectF VideoPreview::computeVideoRect() const
{
    const double widgetW = width();
    const double widgetH = height();

    if (videoWidth_ <= 0 || videoHeight_ <= 0 || widgetW <= 0 || widgetH <= 0) {
        return {};
    }

    // Padding is a percentage (0-100) of the smaller widget dimension
    const double padFraction = padding_ / 100.0;
    const double padPixels = std::min(widgetW, widgetH) * padFraction * 0.5;

    const double contentW = widgetW - padPixels * 2.0;
    const double contentH = widgetH - padPixels * 2.0;

    if (contentW <= 0.0 || contentH <= 0.0) {
        return {};
    }

    // Apply crop to determine effective video aspect ratio
    const double croppedW = videoWidth_ * cropW_;
    const double croppedH = videoHeight_ * cropH_;
    const double videoAspect = croppedW / croppedH;
    const double contentAspect = contentW / contentH;

    double drawW = 0.0;
    double drawH = 0.0;

    if (videoAspect > contentAspect) {
        // Video is wider — fit to width, letterbox top/bottom
        drawW = contentW;
        drawH = contentW / videoAspect;
    } else {
        // Video is taller — fit to height, pillarbox left/right
        drawH = contentH;
        drawW = contentH * videoAspect;
    }

    // Center in the content area
    const double drawX = padPixels + (contentW - drawW) * 0.5;
    const double drawY = padPixels + (contentH - drawH) * 0.5;

    return {drawX, drawY, drawW, drawH};
}

} // namespace openscreen
