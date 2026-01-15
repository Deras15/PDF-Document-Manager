/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
#include "pdfviewport.h"
#include <QPainter>
#include <QtConcurrent>
#include <QApplication>

PdfViewPort::PdfViewPort(QWidget *parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setStyleSheet("QScrollArea { background-color: #525659; border: none; }");
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    scrollContainer = new QWidget();
    scrollLayout = new QVBoxLayout(scrollContainer);
    scrollLayout->setContentsMargins(0, 20, 0, 20);
    scrollLayout->setSpacing(20);
    scrollLayout->setAlignment(Qt::AlignHCenter);
    setWidget(scrollContainer);

    renderTimer = new QTimer(this);
    renderTimer->setSingleShot(true);
    renderTimer->setInterval(700);
    connect(renderTimer, &QTimer::timeout, this, &PdfViewPort::onRenderTimeout);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &PdfViewPort::onScrollValueChanged);
}

PdfViewPort::~PdfViewPort() {
    stopAllRenders();
}

void PdfViewPort::stopAllRenders() {
    for (auto *w : activeRenders.values()) {
        w->disconnect();
        w->waitForFinished();
        w->deleteLater();
    }
    activeRenders.clear();
}

void PdfViewPort::cancelAllRenders() {
    for (auto *w : activeRenders.values()) {
        w->disconnect();
        w->deleteLater();
    }
    activeRenders.clear();
}

void PdfViewPort::setDocument(Poppler::Document *doc, QMutex *mutex) {
    stopAllRenders();
    
    m_doc = doc;
    m_docMutex = mutex;
    clearLayout();
    
    if (m_doc) {
        int total = 0;
        {

            QMutexLocker locker(m_docMutex);
            total = m_doc->numPages();
        }

        for (int i = 0; i < total; ++i) {
            PageWidget *pw = new PageWidget();
            scrollLayout->addWidget(pw);
            pageLabels.append(pw);
        }

        performZoomOrResize();
        verticalScrollBar()->setValue(0);
        verticalScrollBar()->setSingleStep(25);
    }
}

void PdfViewPort::setZoom(double zoom) {
    m_currentZoom = zoom;
    if (!m_fitWidth) performZoomOrResize();
}

void PdfViewPort::setFitWidth(bool fit) {
    m_fitWidth = fit;
    performZoomOrResize();
}

void PdfViewPort::resizeEvent(QResizeEvent *event) {
    QScrollArea::resizeEvent(event);
    renderTimer->start();
}

void PdfViewPort::onRenderTimeout() {

    updateVisiblePages();
}


void PdfViewPort::onScrollValueChanged(int value) {
    Q_UNUSED(value);
    if (pageLabels.isEmpty()) return;

    QScrollBar *bar = verticalScrollBar();
    int foundPage = -1;

    if (value <= 15) {
        foundPage = 1;
    } else if (value >= bar->maximum() - 15) {
        foundPage = pageLabels.size();
    } else {
        int readingLineY = value + (viewport()->height() * 0.2);
        for (int i = 0; i < pageLabels.size(); ++i) {
            QRect g = pageLabels[i]->geometry();
            if (readingLineY >= g.top() && readingLineY <= (g.bottom() + scrollLayout->spacing())) {
                foundPage = i + 1;
                break;
            }
        }
    }

    if (foundPage != -1) {
        emit pageInViewChanged(foundPage);
    }
    
     renderTimer->start(200);
}

void PdfViewPort::performZoomOrResize() {
    if (pageLabels.isEmpty() || !m_doc) return;

    cancelAllRenders();
    scrollContainer->setUpdatesEnabled(false);

    int scrollY = verticalScrollBar()->value();
    int currentPageIdx = 0;
    double relativeOffset = 0.0;
    for (int i = 0; i < pageLabels.size(); ++i) {
        QRect g = pageLabels[i]->geometry();
        if (scrollY < g.bottom()) {
            currentPageIdx = i;
            if (g.height() > 0) relativeOffset = double(scrollY - g.top()) / g.height();
            break;
        }
    }

    updateGridHelper();
    
    scrollContainer->layout()->activate();
    scrollContainer->adjustSize();

    int newY = pageLabels[currentPageIdx]->y() + qRound(pageLabels[currentPageIdx]->height() * relativeOffset);
    verticalScrollBar()->setValue(newY);

    scrollContainer->setUpdatesEnabled(true);
    
    renderTimer->start(700); 
}

void PdfViewPort::updateGridHelper() {
    if (!m_doc || pageLabels.isEmpty()) return;

    QSize referenceSize;
    {
        QMutexLocker locker(m_docMutex);
        Poppler::Page *firstPage = m_doc->page(0);
        if (firstPage) {
            referenceSize = firstPage->pageSize();
            delete firstPage;
        } else {
            referenceSize = QSize(600, 800);
        }
    }

    int viewW = viewport()->width() - 40;
    if (viewW < 100) viewW = 100;

    for (int i = 0; i < pageLabels.size(); ++i) {
        int tw, th;
        if (m_fitWidth) {
            tw = viewW;
            th = tw * referenceSize.height() / referenceSize.width();
        } else {
            tw = referenceSize.width() * m_currentZoom;
            th = referenceSize.height() * m_currentZoom;
        }

        if (pageLabels[i]->size() != QSize(tw, th)) {
            pageLabels[i]->setFixedSize(tw, th);
            pageLabels[i]->setProperty("rendered_zoom", -100.0);
        }
    }
}

void PdfViewPort::updateVisiblePages() {
    if (pageLabels.isEmpty() || !m_doc) return;

    int scrollY = verticalScrollBar()->value();
    int viewportH = viewport()->height();
    int maxScroll = verticalScrollBar()->maximum();

    int buffer = viewportH * 1.5;
    QRect renderZone(0, scrollY - buffer, viewport()->width(), viewportH + (buffer * 2));

    if (scrollY >= maxScroll - 20) {
        renderZone.setBottom(scrollContainer->height() + 100);
    }

    if (scrollY <= 20) {
        renderZone.setTop(-100);
    }

    for (int i = 0; i < pageLabels.size(); ++i) {
        PageWidget *pw = pageLabels[i];
        QRect pageRect = pw->geometry();

        if (pageRect.intersects(renderZone)) {
            double currentZ = m_fitWidth ? -1.0 : m_currentZoom;
            if (pw->property("rendered_zoom").toDouble() != currentZ) {
                requestPageRender(i);
            }
        } else {
            if (qAbs(pageRect.top() - scrollY) > viewportH * 4) {
                if (!pw->isLoading() && pw->property("rendered_zoom").toDouble() != -300.0) {
                    pw->clearPixmap();
                    pw->setProperty("rendered_zoom", -300.0);
                }
            }
        }
    }
}

void PdfViewPort::requestPageRender(int i) {
    if (activeRenders.contains(i) || !m_doc) return;

    if (activeRenders.size() >= QThread::idealThreadCount()) return;
    
    PageWidget *lbl = pageLabels[i];
    lbl->setLoading();
    QSize tSize = lbl->size();
    if (tSize.width() <= 0) return;

    lbl->setLoading();
    double dpr = this->devicePixelRatioF();

    QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>();
    activeRenders.insert(i, watcher);

    connect(watcher, &QFutureWatcher<QImage>::finished, [this, i, watcher, lbl, dpr](){
        if (activeRenders.value(i) == watcher) {
            QImage result = watcher->result();
            if (!result.isNull()) {
                result.setDevicePixelRatio(dpr); 
                lbl->setPixmap(QPixmap::fromImage(result));
                lbl->setProperty("rendered_zoom", m_fitWidth ? -1 : m_currentZoom);
            }
            activeRenders.remove(i);
        }
        watcher->deleteLater();
    });

    QString sText = m_currentSearchText;
    QRectF sRect = m_currentSearchRect;

    watcher->setFuture(QtConcurrent::run([this, i, tSize, sText, sRect, dpr]() {
        QThread::currentThread()->setPriority(QThread::LowPriority);
        QImage img;
        QSizeF pdfSize;
        {
            QMutexLocker locker(m_docMutex);
            if (!m_doc || i >= m_doc->numPages()) return img;
            Poppler::Page *p = m_doc->page(i);
            if (p) {
                pdfSize = p->pageSizeF();
                
                double zoomLevel = double(tSize.width()) / pdfSize.width();
                double qualityMultiplier = (zoomLevel > 2.0) ? 1.0 : (zoomLevel > 1.2 ? 1.2 : 1.5); 
                double targetDpi = 72.0 * zoomLevel * dpr * qualityMultiplier;

                double maxWidthPx = 3500.0;
                if ((pdfSize.width() / 72.0) * targetDpi > maxWidthPx) {
                    targetDpi = (maxWidthPx * 72.0) / pdfSize.width();
                }
                
                targetDpi = qBound(72.0, targetDpi, 400.0); 
                img = p->renderToImage(targetDpi, targetDpi);
                delete p;
            }
        }
        
        if (img.isNull()) return img;

        if (!sText.isEmpty()) {
            QList<QRectF> res;
            { QMutexLocker locker(m_docMutex); if(m_doc) { Poppler::Page *p = m_doc->page(i); res = p->search(sText, Poppler::Page::IgnoreCase); delete p; } }
            if (!res.isEmpty()) {
                QPainter painter(&img);
                painter.setCompositionMode(QPainter::CompositionMode_Multiply);
                double sx = double(img.width()) / pdfSize.width();
                double sy = double(img.height()) / pdfSize.height();
                for (const QRectF &r : res) {
                    bool active = qAbs(r.x() - sRect.x()) < 0.001;
                    painter.setBrush(active ? QColor(255, 140, 0) : QColor(255, 235, 60));
                    painter.setPen(Qt::NoPen);
                    painter.drawRect(QRectF(r.x()*sx, r.y()*sy, r.width()*sx, r.height()*sy).adjusted(-2,-2,2,2));
                }
            }
        }
        return img;
    }));
}

void PdfViewPort::goToPage(int page, double yOffsetFraction) {
    if (page < 1 || page > pageLabels.size()) return;
    int y = pageLabels[page-1]->y() + (pageLabels[page-1]->height() * yOffsetFraction);
    verticalScrollBar()->setValue(y);
}

void PdfViewPort::updateHighlight(const QString &text, QRectF rect) {
    m_currentSearchText = text;
    m_currentSearchRect = rect;
    for (auto *p : pageLabels) p->setProperty("rendered_zoom", -200.0);
    updateVisiblePages();
}

void PdfViewPort::clearSearch() {
    m_currentSearchText = "";
    for (auto *p : pageLabels) p->setProperty("rendered_zoom", -200.0);
    updateVisiblePages();
}

void PdfViewPort::clearLayout() {
    pageLabels.clear();
    QLayoutItem *child;
    while ((child = scrollLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
}

void PdfViewPort::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        emit zoomRequested(event->angleDelta().y() > 0);
        event->accept(); 
    } else {
        QScrollArea::wheelEvent(event);
    }
}