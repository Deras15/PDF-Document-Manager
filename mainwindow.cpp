/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
//mainwindow.cpp
#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QApplication>
#include <QFrame>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Orion PDF Reader");
    resize(1400, 900);

    setupUI();

    connect(sidebar, &LibrarySidebar::fileSelected, this, &MainWindow::openFile);
    connect(searchPanel, &PdfSearchPanel::pageFound, this, &MainWindow::onPageFound);
    connect(searchPanel, &PdfSearchPanel::searchReset, this, &MainWindow::onSearchReset);

    connect(viewPort, &PdfViewPort::pageInViewChanged, [this](int page){
        if (pageSelector->value() != page) {
            pageSelector->blockSignals(true);
            pageSelector->setValue(page);
            pageSelector->blockSignals(false);
        }
    });

    connect(pageSelector, QOverload<int>::of(&QSpinBox::valueChanged), [this](int page){
        viewPort->goToPage(page);
    });

    connect(viewPort, &PdfViewPort::zoomRequested, [this](bool zoomIn){
    double currentVal = zoomSpinBox->value();
    if (zoomIn) {
        zoomSpinBox->setValue(currentVal + 10.0);
    } else {
        zoomSpinBox->setValue(currentVal - 10.0);
    }
});

    QString libPath = QDir::homePath() + "/Документы/PDF_Library";
    sidebar->scanDirectory(libPath);
}

MainWindow::~MainWindow() {
    viewPort->stopAllRenders();
    QMutexLocker locker(&docMutex);
    delete doc;
}

void MainWindow::setupUI() {
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    sidebar = new LibrarySidebar();
    
    QWidget *rightContainer = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QWidget *topBar = new QWidget();
    topBar->setStyleSheet("background: #f5f5f5; border-bottom: 1px solid #ddd;");
    topBar->setFixedHeight(50);
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);

    searchPanel = new PdfSearchPanel();
    topLayout->addWidget(searchPanel);
    topLayout->addStretch();

    fitWidthCheck = new QCheckBox("По ширине");
    fitWidthCheck->setChecked(true);
    connect(fitWidthCheck, &QCheckBox::toggled, this, &MainWindow::onFitWidthToggled);
    
    zoomSpinBox = new QDoubleSpinBox();
    zoomSpinBox->setRange(25.0, 400.0);
    zoomSpinBox->setValue(100.0);
    zoomSpinBox->setSingleStep(10.0);
    zoomSpinBox->setSuffix("%");
    zoomSpinBox->setEnabled(false);
    connect(zoomSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onZoomChanged);

    topLayout->addWidget(fitWidthCheck);
    topLayout->addWidget(zoomSpinBox);

    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFixedHeight(20);
    topLayout->addWidget(line);

    pageSelector = new InvertedSpinBox();
    pageSelector->setMinimum(1);
    pageSelector->setFixedWidth(60);
    totalPagesLabel = new QLabel("/ 0");
    
    topLayout->addWidget(pageSelector);
    topLayout->addWidget(totalPagesLabel);

    viewPort = new PdfViewPort();

    rightLayout->addWidget(topBar);
    rightLayout->addWidget(viewPort);

    mainSplitter->addWidget(sidebar);
    mainSplitter->addWidget(rightContainer);
    mainSplitter->setStretchFactor(1, 1);
    
    setCentralWidget(mainSplitter);
}

void MainWindow::openFile(const QString &filePath) {

    searchPanel->cancelSearch();
    viewPort->stopAllRenders();

    Poppler::Document *newDoc = Poppler::Document::load(filePath);
    if (!newDoc || newDoc->isLocked()) {
        delete newDoc;
        return;
    }

    {
        QMutexLocker locker(&docMutex);
        if (doc) delete doc;
        doc = newDoc;
        doc->setRenderBackend(Poppler::Document::SplashBackend);
    }

    viewPort->setDocument(doc, &docMutex);
    searchPanel->setDocument(doc, &docMutex);
    
    int total = doc->numPages();
    pageSelector->setMaximum(total);
    pageSelector->setValue(1);
    totalPagesLabel->setText(QString("/ %1").arg(total));
}

void MainWindow::onPageFound(int index, QString text, QRectF rect) {
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
    
    viewPort->updateHighlight(text, rect);
    viewPort->goToPage(index + 1, yFraction - 0.1);
}

void MainWindow::onSearchReset() {
    viewPort->clearSearch();
}

void MainWindow::onZoomChanged(double value) {
    viewPort->setZoom(value / 100.0);
}

void MainWindow::onFitWidthToggled(bool checked) {
    zoomSpinBox->setEnabled(!checked);
    viewPort->setFitWidth(checked);
}