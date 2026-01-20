/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
 //custom_widgets.cpp
#include "custom_widgets.h"

PageWidget::PageWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void PageWidget::setPixmap(const QPixmap &pix) {
    m_loading = false;
    m_currentPixmap = pix;
    update();
}

void PageWidget::setLoading() {
    m_loading = true;
}

void PageWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    
    if (!m_currentPixmap.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
    }

    painter.fillRect(rect(), Qt::white);

    if (!m_currentPixmap.isNull()) {
        painter.drawPixmap(rect(), m_currentPixmap);
    }

    painter.setPen(QColor(200, 200, 200));
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

void PageWidget::clearPixmap() {
    m_currentPixmap = QPixmap();
    m_loading = false;
    update();
}