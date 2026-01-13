/*
 * PDF Reader
 * Copyright (c) 2024 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
//mainwindow.cpp
#include "mainwindow.h"
#include <QDir>
#include <QPainter>
#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QFrame>
#include <QtConcurrent>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent), doc(nullptr), currentZoom(1.0)
{
    setWindowTitle("Orion PDF Reader");
    this->resize(1400, 900);
    renderTimer = new QTimer(this);
    renderTimer->setSingleShot(true);
    connect(renderTimer, &QTimer::timeout, this, &MainWindow::onRenderTimeout);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    
    treeWidget = new QTreeWidget();
    treeWidget->setHeaderLabel("Библиотека");
    treeWidget->setFixedWidth(300);

    QWidget *rightContainer = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QWidget *topBar = new QWidget();
    topBar->setStyleSheet("background: #f5f5f5; border-bottom: 1px solid #ddd;");
    topBar->setFixedHeight(50); 

    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(15, 5, 15, 5);
    topLayout->setSpacing(15);

    searchPanel = new PdfSearchPanel();
    topLayout->addWidget(searchPanel);

    topLayout->addStretch();

    fitWidthCheck = new QCheckBox("По ширине");
    fitWidthCheck->setChecked(true);
    connect(fitWidthCheck, &QCheckBox::toggled, this, &MainWindow::onFitWidthToggled);
    topLayout->addWidget(fitWidthCheck);

    zoomSpinBox = new QDoubleSpinBox();
    zoomSpinBox->setRange(25.0, 400.0);
    zoomSpinBox->setValue(100.0);
    zoomSpinBox->setSuffix("%");
    zoomSpinBox->setSingleStep(10.0);
    zoomSpinBox->setFixedWidth(80);
    zoomSpinBox->setAlignment(Qt::AlignCenter);
    zoomSpinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    zoomSpinBox->setEnabled(false); 
    connect(zoomSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            [this](double v){ onZoomChanged(v / 100.0); });
    topLayout->addWidget(zoomSpinBox);

    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setFixedHeight(20);
    topLayout->addWidget(line);

    QWidget *navGroup = new QWidget();
    QHBoxLayout *navLayout = new QHBoxLayout(navGroup);
    navLayout->setContentsMargins(0,0,0,0);
    navLayout->setSpacing(8);

    pageSelector = new InvertedSpinBox();
    pageSelector->setMinimum(1);
    pageSelector->setMaximum(1);
    pageSelector->setFixedWidth(60);
    pageSelector->setAlignment(Qt::AlignCenter);
    pageSelector->setButtonSymbols(QAbstractSpinBox::NoButtons);
    pageSelector->setStyleSheet("QSpinBox { border: 1px solid #ccc; border-radius: 4px; padding: 4px; font-weight: bold; background: white; }");

    totalPagesLabel = new QLabel("/ 0");
    totalPagesLabel->setStyleSheet("color: #666; font-size: 14px; font-weight: bold;");
    
    navLayout->addWidget(pageSelector);
    navLayout->addWidget(totalPagesLabel);
    navLayout->setAlignment(Qt::AlignVCenter);
    
    topLayout->addWidget(navGroup);

    scrollArea = new QScrollArea();
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scrollContainer = new QWidget();
    scrollLayout = new QVBoxLayout(scrollContainer);
    scrollLayout->setContentsMargins(0, 0, 0, 0); 
    scrollLayout->setSpacing(20); 
    scrollLayout->setAlignment(Qt::AlignHCenter); 
    
    scrollArea->setWidget(scrollContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { background-color: #525659; border: none; }");

    rightLayout->addWidget(topBar);
    rightLayout->addWidget(scrollArea);
    
    splitter->addWidget(treeWidget);
    splitter->addWidget(rightContainer);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    connect(treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onItemClicked);
    
    connect(pageSelector, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val){
        this->onPageSpinChanged(val, 0.0);
    });

    connect(searchPanel, &PdfSearchPanel::pageFound, [this](int index, QString text, QRectF rect){
        if (currentSearchText != text) {
            currentSearchText = text;
            for (QLabel *lbl : pageLabels) {
                lbl->setProperty("rendered_zoom", -100.0);
            }
        }

        currentSearchRect = rect; 
        
        if (index >= 0) {
            pageSelector->blockSignals(true);
            pageSelector->setValue(index + 1);
            pageSelector->blockSignals(false);
            
            double yFraction = 0.0;
            {
                QMutexLocker locker(&docMutex);
                if (doc) {
                    Poppler::Page *p = doc->page(index);
                    if (p) {
                        yFraction = rect.top() / p->pageSize().height();
                        delete p;
                    }
                }
            }
            
            if (index < pageLabels.size()) {
                pageLabels[index]->setProperty("rendered_zoom", -100.0);
            }
            
            onPageSpinChanged(index + 1, yFraction);
            updateVisiblePages(); 
        }
    });

    connect(searchPanel, &PdfSearchPanel::searchReset, [this](){
        if (currentSearchText.isEmpty()) return;
        
        currentSearchText = "";

        for (QLabel *lbl : pageLabels) {
            lbl->setProperty("rendered_zoom", -100.0);
        }
        updateVisiblePages();
    });
 connect(scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, [this](int value){
        if (pageLabels.isEmpty() || pageSelector->signalsBlocked()) {
            renderTimer->start(100);
            return;
        }

        QScrollBar *bar = scrollArea->verticalScrollBar();
        int foundPage = -1;

        if (value <= 5) {
            foundPage = 1;
        }
        else if (value >= bar->maximum() - 5) {
            foundPage = pageLabels.size();
        }
        else {
            int readingLineY = value + (scrollArea->viewport()->height() * 0.2);
            
            for (int i = 0; i < pageLabels.size(); ++i) {
                QRect g = pageLabels[i]->geometry();
                if (readingLineY >= g.top() && readingLineY <= (g.bottom() + scrollLayout->spacing())) {
                    foundPage = i + 1;
                    break;
                }
            }
        }

        if (foundPage == -1) {
            int midY = value + (scrollArea->viewport()->height() / 2);
            for (int i = 0; i < pageLabels.size(); ++i) {
                if (midY < pageLabels[i]->geometry().bottom()) {
                    foundPage = i + 1;
                    break;
                }
            }
        }

        if (foundPage != -1 && pageSelector->value() != foundPage) {
            pageSelector->blockSignals(true);
            pageSelector->setValue(foundPage);
            pageSelector->blockSignals(false);
        }
        
        renderTimer->start(100);
    });

    QString libPath = QDir::homePath() + "/Документы/PDF_Library";
    QDir dir(libPath);
    if (!dir.exists()) dir.mkpath(".");
    scanDirectory(libPath);
}

MainWindow::~MainWindow() {

    if (renderTimer) renderTimer->stop();
    if (searchPanel) searchPanel->setDocument(nullptr, nullptr);

    QList<QFutureWatcher<QImage>*> watchers = activeRenders.values();
    
    for(auto *watcher : watchers) {
        if(watcher) {
            watcher->disconnect();
            watcher->waitForFinished();
            delete watcher;
        }
    }
    activeRenders.clear();
    {
        QMutexLocker locker(&docMutex);
        if (doc) {
            delete doc;
            doc = nullptr;
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!pageLabels.isEmpty()) {
        renderTimer->start(150); 
    }
}

void MainWindow::onRenderTimeout() {
    if (pageLabels.isEmpty()) return;

    if (fitWidthCheck->isChecked()) {
        performZoomOrResize();
    } else {
        updateVisiblePages();
    }
}

void MainWindow::onFitWidthToggled(bool checked) {
    zoomSpinBox->setEnabled(!checked);
    performZoomOrResize();
}

void MainWindow::onZoomChanged(double value) {
    currentZoom = value;
    if (!fitWidthCheck->isChecked()) {
        performZoomOrResize();
    }
}

void MainWindow::performZoomOrResize() {
    if (pageLabels.isEmpty()) return;

    int currentPage = pageSelector->value();
    double relativeOffset = 0.0;
    
    QLabel *lbl = pageLabels[currentPage - 1];
    int viewportTop = scrollArea->verticalScrollBar()->value();
    
    if (lbl->y() < (viewportTop + scrollArea->viewport()->height()) && 
        (lbl->y() + lbl->height()) > viewportTop) {
        int delta = viewportTop - lbl->y();
        if (lbl->height() > 0) {
            relativeOffset = (double)delta / lbl->height();
        }
    }

    updateGridHelper();

    scrollContainer->adjustSize(); 
    QApplication::processEvents(); 

    QLabel *newLbl = pageLabels[currentPage - 1];
    int newScrollY = newLbl->y() + (newLbl->height() * relativeOffset);
    
    scrollArea->verticalScrollBar()->setValue(newScrollY);

    updateVisiblePages();
}

void MainWindow::updateGridHelper() {
    QMutexLocker locker(&docMutex);
    if (!doc || pageLabels.isEmpty()) return;

    int viewportWidth = scrollArea->viewport()->width() - 40; 
    if (viewportWidth < 100) viewportWidth = 100;

    for (int i = 0; i < pageLabels.size(); ++i) {
        Poppler::Page *p = doc->page(i);
        if (!p) continue;
        
        QSize pageSize = p->pageSize();
        delete p;

        int targetW, targetH;

        if (fitWidthCheck->isChecked()) {
            double ratio = (double)pageSize.width() / pageSize.height();
            targetW = viewportWidth;
            targetH = targetW / ratio;
        } else {
            targetW = pageSize.width() * currentZoom;
            targetH = pageSize.height() * currentZoom;
        }

        if (pageLabels[i]->width() != targetW) {
            pageLabels[i]->clear(); 
            pageLabels[i]->setFixedSize(targetW, targetH);
            pageLabels[i]->setProperty("rendered_zoom", -100.0); 
        }
    }
}

void MainWindow::updateVisiblePages() {
    if (pageLabels.isEmpty()) return;

    scrollContainer->layout()->activate();

    QRect viewportRect = scrollArea->viewport()->rect();
    viewportRect.translate(0, scrollArea->verticalScrollBar()->value());
    viewportRect.adjust(0, -1000, 0, 1000); 

    for (int i = 0; i < pageLabels.size(); ++i) {
        QLabel *lbl = pageLabels[i];
        
        if (lbl->geometry().intersects(viewportRect)) {
            bool needRender = lbl->pixmap() == nullptr || 
                              lbl->property("rendered_zoom").toDouble() != (fitWidthCheck->isChecked() ? -1 : currentZoom);
            
            if (needRender) {
                requestPageRender(i);
            }
        } else {
            if (lbl->pixmap() != nullptr) {
                lbl->clear();
                lbl->setText("Загрузка...");
                lbl->setStyleSheet("border: 1px solid #999; background: white; color: #ccc; font-size: 20px;");
                lbl->setProperty("rendered_zoom", -100.0); 
            }
        }
    }
}

void MainWindow::requestPageRender(int i) {
    if (i < 0 || i >= pageLabels.size()) return;
    if (activeRenders.contains(i)) return; 

    QLabel *lbl = pageLabels[i];
    QSize targetSize = lbl->size();
    if (targetSize.width() <= 0 || targetSize.height() <= 0) return;

    double currentZ = fitWidthCheck->isChecked() ? -1 : currentZoom;
    QString searchText = currentSearchText;
    
    // !!! НОВОЕ: Захватываем текущий активный прямоугольник в лямбду !!!
    QRectF activeRect = currentSearchRect;
    
    QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>();
    activeRenders.insert(i, watcher);

    connect(watcher, &QFutureWatcher<QImage>::finished, [this, i, watcher, currentZ, lbl](){
        if(activeRenders.contains(i) && activeRenders[i] == watcher) {
            onPageRendered(i, watcher->result());
            activeRenders.remove(i);
            lbl->setProperty("rendered_zoom", currentZ);
        }
        watcher->deleteLater();
    });

    // Добавляем activeRect в захват лямбды (в квадратные скобки)
    QFuture<QImage> future = QtConcurrent::run([this, i, targetSize, searchText, activeRect]() {
        QImage resultImg;
        
        // --- ЭТАП 1: РЕНДЕРИНГ ---
        QSizeF pdfSizeForHighlight;
        bool pageRenderedOk = false;

        {
            QMutexLocker locker(&this->docMutex);
            if(this->doc && i < this->doc->numPages()) {
                Poppler::Page *pdfPage = this->doc->page(i);
                if (pdfPage) {
                    QSizeF pdfSize = pdfPage->pageSizeF();
                    pdfSizeForHighlight = pdfSize;
                    
                    double scaleFactor = (double)targetSize.width() / pdfSize.width();
                    double targetDpi = 72.0 * scaleFactor;
                    double renderDpi = std::max(144.0, targetDpi * 1.5); 
                    if (renderDpi > 300.0) renderDpi = 300.0;

                    resultImg = pdfPage->renderToImage(renderDpi, renderDpi);
                    pageRenderedOk = true;

                    delete pdfPage; 
                }
            }
        } 

        if (!pageRenderedOk || resultImg.isNull()) return resultImg;

        // --- ЭТАП 2: МАСШТАБИРОВАНИЕ ---
        if (resultImg.size() != targetSize) {
            resultImg = resultImg.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
            
        // --- ЭТАП 3: ПОДСВЕТКА С АКТИВНЫМ ВЫДЕЛЕНИЕМ ---
        if (!searchText.isEmpty()) {
            QList<QRectF> results;
            
            {
                QMutexLocker locker(&this->docMutex);
                if(this->doc && i < this->doc->numPages()) {
                    Poppler::Page *p = this->doc->page(i);
                    if (p) {
                        results = p->search(searchText, Poppler::Page::IgnoreCase);
                        delete p;
                    }
                }
            }

            if (!results.isEmpty()) {
                QPainter painter(&resultImg);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setCompositionMode(QPainter::CompositionMode_Multiply);
                painter.setPen(Qt::NoPen);

                double sx = (double)resultImg.width() / pdfSizeForHighlight.width();
                double sy = (double)resultImg.height() / pdfSizeForHighlight.height();

                for (const QRectF &rect : results) {
                    bool isActive = (qAbs(rect.x() - activeRect.x()) < 0.001 && 
                                     qAbs(rect.y() - activeRect.y()) < 0.001 &&
                                     qAbs(rect.width() - activeRect.width()) < 0.001);

                    if (isActive) {
                        painter.setBrush(QColor(255, 140, 0)); 
                    } else {
                        painter.setBrush(QColor(255, 235, 60)); 
                    }

                    QRectF highlightRect(
                        rect.x() * sx, 
                        rect.y() * sy, 
                        rect.width() * sx, 
                        rect.height() * sy
                    );
                    highlightRect.adjust(-2, -2, 2, 2);
                    painter.drawRect(highlightRect);
                }
            }
        }

        return resultImg;
    });

    watcher->setFuture(future);
}

void MainWindow::onPageRendered(int pageIndex, QImage image) {
    if (pageIndex < 0 || pageIndex >= pageLabels.size()) return;
    if (image.isNull()) return;

    QLabel *lbl = pageLabels[pageIndex];
    lbl->setPixmap(QPixmap::fromImage(image));
    lbl->setStyleSheet("border: 1px solid #999; margin-bottom: 20px;");
}

void MainWindow::onPageSpinChanged(int page, double yOffsetFraction) {
    if (pageLabels.isEmpty() || page < 1 || page > pageLabels.size()) return;
    
    scrollContainer->adjustSize();

    QLabel *targetLabel = pageLabels[page - 1];
    int pageY = targetLabel->y(); 
    
    int innerOffset = 0;
    if (yOffsetFraction > 0.0) {
        innerOffset = static_cast<int>(targetLabel->height() * yOffsetFraction);
        innerOffset -= scrollArea->viewport()->height() / 4; 
    }
    
    scrollArea->verticalScrollBar()->setValue(pageY + innerOffset);
    renderTimer->start(50);
}

void MainWindow::onItemClicked(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);
    QString filePath = item->data(0, Qt::UserRole).toString();
    if (filePath.isEmpty() || !QFile::exists(filePath)) return;

    searchPanel->setDocument(nullptr, nullptr);

    for(auto w : activeRenders) w->waitForFinished();
    activeRenders.clear();

    QMutexLocker locker(&docMutex);

    Poppler::Document *newDoc = Poppler::Document::load(filePath);
    if (!newDoc || newDoc->isLocked()) {
        delete newDoc;
        return;
    }

    if (doc) delete doc;
    doc = newDoc;
    doc->setRenderBackend(Poppler::Document::SplashBackend);

    searchPanel->setDocument(doc, &docMutex);
    
    locker.unlock();
    clearLayout();
    
    locker.relock();
    currentSearchText = "";
    int total = doc->numPages();
    locker.unlock();

    pageSelector->setMaximum(total);
    totalPagesLabel->setText(QString("/ %1").arg(total));

    for (int i = 0; i < total; ++i) {
        QLabel *pageLabel = new QLabel("Загрузка...");
        pageLabel->setAlignment(Qt::AlignCenter);
        pageLabel->setStyleSheet("border: 1px solid #999; background: white; color: #ccc; font-size: 20px;");
        scrollLayout->addWidget(pageLabel);
        pageLabels.append(pageLabel);
    }

    performZoomOrResize();
    
    pageSelector->setValue(1);
    scrollArea->verticalScrollBar()->setValue(0);
}

void MainWindow::clearLayout() {
    activeRenders.clear(); 
    pageLabels.clear();
    QLayoutItem *child;
    while ((child = scrollLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
}

void MainWindow::scanDirectory(const QString &path) {
    treeWidget->clear();
    QDir mainDir(path);
    if (!mainDir.exists()) return;

    QStringList rootPdfs = mainDir.entryList(QStringList() << "*.pdf", QDir::Files);
    if (!rootPdfs.isEmpty()) {
        QTreeWidgetItem *rootItem = new QTreeWidgetItem(treeWidget);
        rootItem->setText(0, "Корневая папка");
        rootItem->setExpanded(true);
        QFont f = rootItem->font(0); f.setBold(true); rootItem->setFont(0, f);
        
        foreach (const QString &fileName, rootPdfs) {
            QTreeWidgetItem *fileItem = new QTreeWidgetItem(rootItem);
            fileItem->setText(0, fileName);
            fileItem->setData(0, Qt::UserRole, mainDir.absoluteFilePath(fileName));
        }
    }
    
    QStringList subDirs = mainDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    foreach (const QString &dirName, subDirs) {
        QTreeWidgetItem *dirItem = new QTreeWidgetItem(treeWidget);
        dirItem->setText(0, dirName.toUpper());
        QFont font = dirItem->font(0); font.setBold(true); dirItem->setFont(0, font);
        
        QDir subDir(mainDir.absoluteFilePath(dirName));
        QStringList pdfFiles = subDir.entryList(QStringList() << "*.pdf", QDir::Files);
        foreach (const QString &fileName, pdfFiles) {
            QTreeWidgetItem *fileItem = new QTreeWidgetItem(dirItem);
            fileItem->setText(0, fileName);
            fileItem->setData(0, Qt::UserRole, subDir.absoluteFilePath(fileName));
        }
        if (!pdfFiles.isEmpty()) dirItem->setExpanded(true);
    }
}
