#ifndef CHUNKIMAGERESPONSE_H
#define CHUNKIMAGERESPONSE_H

#include <QQuickImageResponse>
#include <QRunnable>
#include <QImage>
#include <QThreadPool>

class ImageBackend; // forward decl

// One of these is created per image request
class ChunkImageResponse : public QQuickImageResponse, public QRunnable {
public:
    ChunkImageResponse(ImageBackend* backend, int yOffset, int w, int h)
        : m_backend(backend), m_yOffset(yOffset), m_w(w), m_h(h)
    {
        setAutoDelete(false); // QThreadPool won't delete us; Qt manages lifetime
        QThreadPool::globalInstance()->start(this);
    }

    // Called by QThreadPool on a background thread
    void run() override {
        m_image = m_backend->generateChunk(m_yOffset, m_w, m_h);
        emit finished(); // Qt will then call textureFactory() on main thread
    }

    // Qt calls this on the main thread after finished() to grab the result
    QQuickTextureFactory* textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    // Optional: allow QML to cancel in-flight requests during fast scrolling
    void cancel() override {
        // Could set a flag checked inside run() for expensive operations
    }

private:
    ImageBackend* m_backend;
    int m_yOffset, m_w, m_h;
    QImage m_image;
};

#endif // CHUNKIMAGERESPONSE_H
