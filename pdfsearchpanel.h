/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
//pdfsearchpanel.h
    
#ifndef PDFSEARCHPANEL_H
#define PDFSEARCHPANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QPair>
#include <QMutex>
#include <QFutureWatcher>
#include <QAtomicInt>
#include <poppler-qt5.h>

class PdfSearchPanel : public QWidget {
    Q_OBJECT
public:
    explicit PdfSearchPanel(QWidget *parent = nullptr);
    void setDocument(Poppler::Document *newDoc, QMutex *mutex); 
    void cancelSearch(); 

signals:
    void pageFound(int pageIndex, QString text, QRectF rect);
    void searchReset(); 

private slots:
    void onFindStart();
    void onSearchFinished(); 
    void onNext();
    void onPrev();
    void onReset();

private:
    Poppler::Document *doc = nullptr;
    QMutex *docMutex = nullptr; 

    QLineEdit *searchField;
    QPushButton *btnStart;
    QLabel *lblStatus;
    QWidget *navWidget;
    QPushButton *btnNext, *btnPrev, *btnReset;
    
    QList<QPair<int, QRectF>> searchResults; 
    int currentIndex = -1;

    QFutureWatcher<QList<QPair<int, QRectF>>> *searchWatcher;
    QAtomicInt currentSearchCanceled; 
};

#endif // PDFSEARCHPANEL_H

  