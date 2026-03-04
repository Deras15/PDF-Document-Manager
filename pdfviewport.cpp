/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
 //pdfviewport.cpp
#include "pdfviewport.h"
#include <QPainter>
#include <QtConcurrent>
#include <QApplication>

const double MAX_DPI = 400.0;
const double DRAFT_DPI = 72.0; 

enum RenderQuality {
    QualityDraft,
    QualityHD
};

PdfViewPort::PdfViewPort(QWidget *parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setAlignment(Qt::AlignHCenter);
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
    renderTimer->setInterval(100);
    
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
    
    m_originalPageSizes.clear();
    clearLayout();
    
    if (m_doc) {
        int total = 0;
        {
            QMutexLocker locker(m_docMutex);
            total = m_doc->numPages();
            for (int i = 0; i < total; ++i) {
                Poppler::Page *p = m_doc->page(i);
                if (p) {
                    m_originalPageSizes.append(p->pageSize());
                    delete p;
                } else {
                    m_originalPageSizes.append(QSize(600, 800));
                }
            }
        }

        for (int i = 0; i < total; ++i) {
            PageWidget *pw = new PageWidget();
            scrollLayout->addWidget(pw);
            pageLabels.append(pw);
        }

        performZoomOrResize();
        verticalScrollBar()->setValue(0);
        
        renderTimer->start(50);
    }
}

void PdfViewPort::setFilePath(const QString &path) {
    m_docPath = path;
}

void PdfViewPort::setZoom(double zoom) {
    if (qAbs(m_currentZoom - zoom) < 0.001) return;
    m_currentZoom = zoom;
    performZoomOrResize();
}

void PdfViewPort::resizeEvent(QResizeEvent *event) {
    QScrollArea::resizeEvent(event);
    performZoomOrResize(); 
}

void PdfViewPort::showEvent(QShowEvent *event) {
    QScrollArea::showEvent(event);
    performZoomOrResize();
}

void PdfViewPort::onRenderTimeout() {
    updateVisiblePages(true);
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
    
    updateVisiblePages(false);
    
    renderTimer->start(100);
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
    
    this->setAlignment(Qt::AlignHCenter);
    scrollLayout->setAlignment(Qt::AlignHCenter);

    scrollContainer->layout()->activate();
    scrollContainer->adjustSize();

    if (currentPageIdx < pageLabels.size()) {
        int newY = pageLabels[currentPageIdx]->y() + qRound(pageLabels[currentPageIdx]->height() * relativeOffset);
        verticalScrollBar()->setValue(newY);
    }

    scrollContainer->setUpdatesEnabled(true);
    
    QTimer::singleShot(10, [this](){ updateVisiblePages(false); });
    renderTimer->start(100); 
}

void PdfViewPort::updateGridHelper() {
    if (pageLabels.isEmpty() || m_originalPageSizes.isEmpty()) return;

    int viewW = viewport()->width();
    int availableWidth = viewW - 25; 
    if (availableWidth < 100) availableWidth = 100;

    int targetWidth = qRound(availableWidth * m_currentZoom);

    for (int i = 0; i < pageLabels.size(); ++i) {
        QSize originalSize = m_originalPageSizes[i];
        double aspectRatio = (double)originalSize.height() / originalSize.width();
        int targetHeight = qRound(targetWidth * aspectRatio);

        if (pageLabels[i]->size() != QSize(targetWidth, targetHeight)) {
            pageLabels[i]->setFixedSize(targetWidth, targetHeight);
            
            if (pageLabels[i]->property("rendered_width").toInt() != targetWidth) {
                pageLabels[i]->setProperty("is_hd", false);
            }
        }
    }
}

void PdfViewPort::updateVisiblePages(bool allowHD) {
    if (pageLabels.isEmpty() || !m_doc) return;

    int scrollY = verticalScrollBar()->value();
    int viewportH = viewport()->height();
    double dpr = this->devicePixelRatioF();
    
    int buffer = viewportH * 1.0; 
    QRect renderZone(0, scrollY - buffer, viewport()->width(), viewportH + (buffer * 2));

    for (int i = 0; i < pageLabels.size(); ++i) {
        PageWidget *pw = pageLabels[i];
        QRect pageRect = pw->geometry();

         if (pageRect.intersects(renderZone)) {
            QSize originalSize = m_originalPageSizes[i];
            double zoomFactor = double(pw->width()) / originalSize.width();
            double requiredDpi = 72.0 * zoomFactor * dpr;
            if (requiredDpi > MAX_DPI) requiredDpi = MAX_DPI;
            
            bool isHD = pw->property("is_hd").toBool();
            int renderedW = pw->property("rendered_width").toInt();
            int currentW = pw->width();

            bool needRender = false;
            int quality = QualityDraft;

            if (!pw->hasImage() || renderedW != currentW) {
                needRender = true;
                quality = allowHD ? QualityHD : QualityDraft;
            } 
            else {
                if (allowHD && !isHD) {
                    if (requiredDpi > (DRAFT_DPI + 15.0)) {
                        needRender = true;
                        quality = QualityHD;
                    }
                }
            }

            if (needRender) {
                requestPageRender(i, quality);
            }

        } else {
            if (qAbs(pageRect.top() - scrollY) > viewportH * 3) {
                if (!pw->isLoading() && pw->hasImage()) {
                    pw->clearPixmap();
                    pw->setProperty("rendered_width", -1);
                    pw->setProperty("is_hd", false);
                }
            }
        }
    }
}

void PdfViewPort::requestPageRender(int i, int quality) {
if (activeRenders.contains(i)) return;

    PageWidget *lbl = pageLabels[i];
    if (!lbl->hasImage()) {
        lbl->setLoading();
    }
    
    QSize tSize = lbl->size(); 
    if (tSize.width() <= 0) return;

    double dpr = this->devicePixelRatioF();
    QString docPath = m_docPath; 
    QString sText = m_currentSearchText;
    QRectF sRect = m_currentSearchRect;

    QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>();
    activeRenders.insert(i, watcher);

    connect(watcher, &QFutureWatcher<QImage>::finished, [this, i, watcher, lbl, dpr, tSize, quality](){
        if (activeRenders.value(i) == watcher) {
            QImage result = watcher->result();
            
            if (!result.isNull()) {
                result.setDevicePixelRatio(dpr); 
                lbl->setPixmap(QPixmap::fromImage(result));
                lbl->setProperty("is_hd", (quality == QualityHD));
                lbl->setProperty("rendered_width", tSize.width());
            }
            activeRenders.remove(i);
            QTimer::singleShot(0, [this](){ 
                if (!renderTimer->isActive()) {
                    updateVisiblePages(true);
                }
            });
        }
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([i, tSize, sText, sRect, dpr, docPath, quality]() {
        QImage img;
        if (docPath.isEmpty()) return img;

        if (quality == QualityDraft) {
            QThread::currentThread()->setPriority(QThread::HighestPriority);
        } else {
            QThread::currentThread()->setPriority(QThread::NormalPriority);
        }

        Poppler::Document *threadDoc = Poppler::Document::load(docPath);
        if (!threadDoc) return img;

        threadDoc->setRenderHint(Poppler::Document::TextAntialiasing, true);
        threadDoc->setRenderHint(Poppler::Document::Antialiasing, true);

        if (i < threadDoc->numPages()) {
            Poppler::Page *p = threadDoc->page(i);
            if (p) {
                double dpi;
                
                if (quality == QualityDraft) {
                    dpi = DRAFT_DPI * dpr; 
                } else {
                    QSizeF pdfSize = p->pageSizeF();
                    double zoomFactor = double(tSize.width()) / pdfSize.width();
                    dpi = 72.0 * zoomFactor * dpr;
                    if (dpi > MAX_DPI) dpi = MAX_DPI; 
                }
                
                img = p->renderToImage(dpi, dpi);

                if (!img.isNull() && !sText.isEmpty()) {
                    QList<QRectF> res = p->search(sText, Poppler::Page::IgnoreCase);
                    if (!res.isEmpty()) {
                        QPainter painter(&img);
                        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
                        double sx = double(img.width()) / p->pageSizeF().width();
                        double sy = double(img.height()) / p->pageSizeF().height();
                        for (const QRectF &r : res) {
                            bool active = qAbs(r.x() - sRect.x()) < 0.001;
                            painter.setBrush(active ? QColor(255, 140, 0) : QColor(255, 235, 60));
                            painter.setPen(Qt::NoPen);
                            painter.drawRect(QRectF(r.x()*sx, r.y()*sy, r.width()*sx, r.height()*sy).adjusted(-2,-2,2,2));
                        }
                    }
                }
                delete p;
            }
        }
        delete threadDoc;
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
    for (auto *p : pageLabels) {
        p->setProperty("rendered_width", -1);
    }
    updateVisiblePages(true);
}

void PdfViewPort::clearSearch() {
    m_currentSearchText = "";
    for (auto *p : pageLabels) {
        p->setProperty("rendered_width", -1);
    }
    updateVisiblePages(true);
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