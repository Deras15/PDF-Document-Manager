    
/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
#ifndef PDFVIEWPORT_H
#define PDFVIEWPORT_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QMutex>
#include <QTimer>
#include <QMap>
#include <QFutureWatcher>
#include <poppler-qt5.h>
#include "custom_widgets.h"

class PdfViewPort : public QScrollArea {
    Q_OBJECT
public:
    explicit PdfViewPort(QWidget *parent = nullptr);
    ~PdfViewPort();

    void setDocument(Poppler::Document *doc, QMutex *mutex);
    void setZoom(double zoom);
    void setFitWidth(bool fit);
    void goToPage(int page, double yOffsetFraction = 0.0);
    void updateHighlight(const QString &text, QRectF rect);
    void clearSearch();
    void stopAllRenders();
    int totalPages() const { return pageLabels.size(); }

signals:
    void pageInViewChanged(int page);
    void zoomRequested(bool zoomIn);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void onRenderTimeout();
    void updateVisiblePages();
    void onScrollValueChanged(int value);

private:
    void requestPageRender(int index);
    void performZoomOrResize();
    void updateGridHelper();
    void clearLayout();

    Poppler::Document *m_doc = nullptr;
    QMutex *m_docMutex = nullptr;
    
    QWidget *scrollContainer;
    QVBoxLayout *scrollLayout;
    QList<PageWidget*> pageLabels;
    
    double m_currentZoom = 1.0;
    bool m_fitWidth = true;
    QString m_currentSearchText;
    QRectF m_currentSearchRect;
    
    QTimer *renderTimer;
    QMap<int, QFutureWatcher<QImage>*> activeRenders;
};

#endif // PDFVIEWPORT_H