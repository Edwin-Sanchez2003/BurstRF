#ifndef CHUNKIMAGEPROVIDER_H
#define CHUNKIMAGEPROVIDER_H

#include <QQuickAsyncImageProvider>
#include <QUrlQuery>
#include "ChunkImageResponse.h"
#include "ImageBackend.h"

class ChunkImageProvider : public QQuickAsyncImageProvider {
public:
    explicit ChunkImageProvider(ImageBackend* backend)
        : m_backend(backend) {}

    // Called on main thread; must return immediately
    QQuickImageResponse* requestImageResponse(
        const QString& id,          // everything after "image://myprovider/"
        const QSize& requestedSize  // size hint from QML Image item
        ) override {
        // Parse "chunk?y=400&w=800&h=300" style IDs
        // Simple parsing for MVP:
        QUrlQuery q("?" + id.section('?', 1));
        int yOffset = q.queryItemValue("y").toInt();
        int w = requestedSize.width()  > 0 ? requestedSize.width()  : 800;
        int h = requestedSize.height() > 0 ? requestedSize.height() : 300;

        return new ChunkImageResponse(m_backend, yOffset, w, h);
        // Qt takes ownership of this pointer
    }

private:
    ImageBackend* m_backend; // shared, must be thread-safe
};

#endif // CHUNKIMAGEPROVIDER_H
