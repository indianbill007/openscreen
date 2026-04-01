#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QImage>
#include <memory>
#include <functional>

namespace openscreen {

struct ZoomFocus;
enum class ZoomDepth;
struct CropRegion;

class VideoPreview : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit VideoPreview(QWidget* parent = nullptr);
    ~VideoPreview() override;

    /// Upload a new video frame (BGRA data)
    void setFrame(const uint8_t* bgraData, int width, int height, int stride);

    /// Upload a frame from QImage
    void setFrame(const QImage& image);

    /// Set wallpaper (image path, solid color "#hex", or CSS gradient string)
    void setWallpaper(const QString& wallpaper);

    /// Set zoom parameters for preview
    void setZoom(double scale, double focusX, double focusY, double progress = 1.0);

    /// Reset zoom to 1x (no zoom)
    void resetZoom();

    /// Set padding around the video (0-100)
    void setPadding(double padding);

    /// Set border radius for the video
    void setBorderRadius(double radius);

    /// Set the crop region (normalized 0-1)
    void setCropRegion(double x, double y, double w, double h);

    /// Get the video's native size
    QSize videoSize() const;

signals:
    /// Emitted when user clicks on the preview to set zoom focus
    void focusClicked(double normalizedX, double normalizedY);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void setupShaders();
    void setupGeometry();
    void renderWallpaper();
    void renderVideoFrame();
    void updateVideoTexture();
    QRectF computeVideoRect() const;

    // Shaders
    std::unique_ptr<QOpenGLShaderProgram> texturedShader_;
    std::unique_ptr<QOpenGLShaderProgram> solidShader_;

    // Geometry
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_{QOpenGLBuffer::VertexBuffer};

    // Video texture
    std::unique_ptr<QOpenGLTexture> videoTexture_;
    QImage pendingFrame_;
    bool frameDirty_ = false;
    int videoWidth_ = 0;
    int videoHeight_ = 0;

    // Wallpaper
    std::unique_ptr<QOpenGLTexture> wallpaperTexture_;
    QString currentWallpaper_;
    QColor solidColor_{Qt::black};
    bool wallpaperIsImage_ = false;
    bool wallpaperIsSolid_ = true;

    // Transform state
    double zoomScale_ = 1.0;
    double focusX_ = 0.5;
    double focusY_ = 0.5;
    double zoomProgress_ = 1.0;
    double padding_ = 50.0;
    double borderRadius_ = 0.0;
    double cropX_ = 0.0, cropY_ = 0.0, cropW_ = 1.0, cropH_ = 1.0;
};

} // namespace openscreen
