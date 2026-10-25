#ifndef SPECTROGRAM_ITEM_H
#define SPECTROGRAM_ITEM_H

#include <QQuickPaintedItem>
#include <QImage>
#include <QPainter>
#include <QtQml/qqmlregistration.h>

class SpectrogramItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SpectrogramItem(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent) {}

    // Called from QML. Replaces the displayed image and redraws.
    Q_INVOKABLE void setImage(const QImage &img)
    {
        m_image = img;
        update();   // schedules a call to paint()
    }

    void paint(QPainter *painter) override
    {
        if (m_image.isNull())
            return;
        painter->drawImage(boundingRect(), m_image);   // stretches to the item's size
    }

private:
    QImage m_image;
};

#endif // SPECTROGRAM_ITEM_H
