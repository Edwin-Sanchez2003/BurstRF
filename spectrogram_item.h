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
        setImplicitWidth(img.width());
        setImplicitHeight(img.height());
        //setWidth(img.width());
        //setHeight(img.height());
        update();   // schedules a call to paint()
    }

    void paint(QPainter *painter) override
    {
        if (m_image.isNull())
            return;
        painter->drawImage(QPointF(0, 0), m_image);   // draws at natural size (starting from Item's top-left corner), no scaling
    }

private:
    QImage m_image;
};

#endif // SPECTROGRAM_ITEM_H
