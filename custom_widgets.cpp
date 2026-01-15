/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
#include "custom_widgets.h"

PageWidget::PageWidget(QWidget *parent) : QWidget(parent) {
    m_spinnerTimer = new QTimer(this);
    connect(m_spinnerTimer, &QTimer::timeout, [this]() {
        if (m_loading) {
            m_rotation += 30;
            if (m_rotation >= 360) m_rotation = 0;
            update();
        }
    });
    m_spinnerTimer->start(80);
}

void PageWidget::setPixmap(const QPixmap &pix) {
    m_loading = false;
    m_currentPixmap = pix;
    update();
}

void PageWidget::setLoading() {
    m_loading = true;
    update();
}

void PageWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.fillRect(rect(), Qt::white);

    if (!m_currentPixmap.isNull()) {
        painter.drawPixmap(rect(), m_currentPixmap);
    }

    if (m_loading && m_currentPixmap.isNull()) { 
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2, height() / 2);
        painter.rotate(m_rotation);
        QPen pen(QColor(41, 128, 185));
        pen.setWidth(4);
        painter.setPen(pen);
        painter.drawArc(-20, -20, 40, 40, 0, 270 * 16);
        painter.restore();
    } 
    else if (m_loading && !m_currentPixmap.isNull()) {
        painter.fillRect(rect(), QColor(255, 255, 255, 40)); 
    }
    
    painter.setPen(QColor(180, 180, 180));
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

void PageWidget::clearPixmap() {
    m_currentPixmap = QPixmap();
    m_loading = true;
    update();
}