#ifndef IMAGEBACKEND_H
#define IMAGEBACKEND_H

#include <QObject>
#include <QImage>

/*
 *
 * ⚠️ Thread safety: generateChunk will be called from worker threads. If it accesses shared state, you need mutexes.
 *
 */

class ImageBackend : public QObject {
    Q_OBJECT
public:
    explicit ImageBackend(QObject* parent = nullptr) : QObject(parent) {}

    // Called from background thread — must be thread-safe!
    // Returns a chunk of height `chunkH` starting at pixel row `yOffset`
    QImage generateChunk(int yOffset, int chunkW, int chunkH) {
        QImage img(chunkW, chunkH, QImage::Format_RGB32);
        const int border = 6;

        for (int row = 0; row < chunkH; ++row) {
            auto* line = reinterpret_cast<QRgb*>(img.scanLine(row));
            bool onVerticalBorder = (row < border || row >= chunkH - border);

            for (int col = 0; col < chunkW; ++col) {
                bool onHorizontalBorder = (col < border || col >= chunkW - border);

                if (onVerticalBorder || onHorizontalBorder)
                    line[col] = qRgb(255, 0, 0);   // red border
                else
                    line[col] = qRgb(0, 0, 255);   // blue fill
            }
        }
        return img;
    }
};

#endif // IMAGEBACKEND_H
