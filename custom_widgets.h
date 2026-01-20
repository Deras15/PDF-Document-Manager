    
/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
 //custom_widgets.h
#ifndef CUSTOM_WIDGETS_H
#define CUSTOM_WIDGETS_H

#include <QSpinBox>
#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QWheelEvent>

class InvertedSpinBox : public QSpinBox {
protected:
    void wheelEvent(QWheelEvent *event) override {
        if (event->angleDelta().y() < 0) stepUp();
        else stepDown();
        event->accept();
    }
};

class PageWidget : public QWidget {
    Q_OBJECT
public:
    explicit PageWidget(QWidget *parent = nullptr);
    void setPixmap(const QPixmap &pix);
    void setLoading();
    void clearPixmap();
    bool isLoading() const { return m_loading; }
    bool hasImage() const { return !m_currentPixmap.isNull(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_loading = false;
    QPixmap m_currentPixmap;
};

#endif // CUSTOM_WIDGETS_H