/*
 * PDF Reader
 * Copyright (c) 2024 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
//mainwindow.h    
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QToolBar>
#include <QSplitter>
#include <QScrollBar>
#include <QMutex>
#include <QMap>
#include <QFutureWatcher>
#include <poppler-qt5.h>
#include "pdfsearchpanel.h"

class InvertedSpinBox : public QSpinBox {
protected:
    void wheelEvent(QWheelEvent *event) override {
        if (event->angleDelta().y() < 0) stepUp();
        else stepDown();
        event->accept();
    }
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onPageSpinChanged(int page, double yOffsetFraction = 0.0);
    void updateGridHelper();
    void updateVisiblePages();
    void onRenderTimeout();
    void onZoomChanged(double value);
    void onFitWidthToggled(bool checked);
    
    // Слот для получения готовой картинки
    void onPageRendered(int pageIndex, QImage image);

private:
    Poppler::Document *doc;
    QMutex docMutex; // Мьютекс для защиты doc

    QTreeWidget *treeWidget;
    QScrollArea *scrollArea;
    QWidget *scrollContainer;
    QVBoxLayout *scrollLayout;
    InvertedSpinBox *pageSelector;
    QLabel *totalPagesLabel;
    PdfSearchPanel *searchPanel;
    
    QDoubleSpinBox *zoomSpinBox;
    QCheckBox *fitWidthCheck;
    double currentZoom; 

    QList<QLabel*> pageLabels;
    QString currentSearchText;
    QRectF currentSearchRect;
    QTimer *renderTimer;
    
    // Храним активные вотчеры для рендеринга: <ИндексСтраницы, Вотчер>
    QMap<int, QFutureWatcher<QImage>*> activeRenders;

    void scanDirectory(const QString &path);
    void clearLayout();
    void requestPageRender(int index); // Асинхронный запрос рендера
    void performZoomOrResize(); 
};

#endif // MAINWINDOW_H

  