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
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QApplication>
#include <QFrame>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QShortcut>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    loadSettings();
    setWindowTitle("Orion PDF Reader");
    resize(900, 650);

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
        zoomSpinBox->setValue(currentVal + 25.0);
    } else {
        zoomSpinBox->setValue(currentVal - 25.0);
    }
});

     sidebar->scanDirectory(m_libraryPath); 
}

MainWindow::~MainWindow() {
    viewPort->stopAllRenders();
    QMutexLocker locker(&docMutex);
    delete doc;
}

void MainWindow::setupUI() {
    QMenu *settingsMenu = menuBar()->addMenu("Настройки");
    
    QAction *setPathAction = new QAction("Путь к библиотеке...", this);
    connect(setPathAction, &QAction::triggered, this, &MainWindow::onChangeLibraryPath);
    settingsMenu->addAction(setPathAction);
    
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    sidebar = new LibrarySidebar();
    sidebar->setMinimumWidth(150);
    sidebar->setMaximumWidth(500); 
    
    QWidget *rightContainer = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QWidget *topBar = new QWidget();
    topBar->setStyleSheet("background: #f5f5f5; border-bottom: 1px solid #ddd;");
    topBar->setFixedHeight(50);
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);

    QPushButton *btnShowSearch = new QPushButton("🔍");
    btnShowSearch->setFixedSize(35, 35);
    btnShowSearch->setToolTip("Поиск (Ctrl+F)");
    btnShowSearch->setStyleSheet("QPushButton { border: 1px solid #ccc; border-radius: 4px; background: white; font-size: 16px; } QPushButton:hover { background: #eee; }");
    topLayout->addWidget(btnShowSearch);
    topLayout->addStretch();

    fitWidthCheck = new QCheckBox("По ширине");
    fitWidthCheck->setChecked(true);
    connect(fitWidthCheck, &QCheckBox::toggled, this, &MainWindow::onFitWidthToggled);
    
    zoomSpinBox = new QDoubleSpinBox();
    zoomSpinBox->setRange(25.0, 800.0);
    zoomSpinBox->setValue(100.0);
    zoomSpinBox->setSingleStep(25.0);
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

    searchPanel = new PdfSearchPanel();
    searchPanel->hide();
    searchPanel->setStyleSheet("background: #eee; border-top: 1px solid #ccc; padding: 5px;");

    rightLayout->addWidget(topBar);
    rightLayout->addWidget(viewPort, 1);
    rightLayout->addWidget(searchPanel);

    mainSplitter->addWidget(sidebar);
    mainSplitter->addWidget(rightContainer);
    mainSplitter->setCollapsible(0, false);
    mainSplitter->setStretchFactor(1, 1);
    
    setCentralWidget(mainSplitter);

    auto toggleSearch = [this]() {
        if (searchPanel->isVisible()) {
            searchPanel->hide();
        } else {
            searchPanel->show();
            searchPanel->focusIn();
        }
    };

    connect(btnShowSearch, &QPushButton::clicked, toggleSearch);

    QShortcut *searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, toggleSearch);

}

void MainWindow::openFile(const QString &filePath) {

    searchPanel->cancelSearch();
    viewPort->stopAllRenders();

    searchPanel->setFilePath(filePath); 

    Poppler::Document *newDoc = Poppler::Document::load(filePath);
    if (!newDoc || newDoc->isLocked()) {
        delete newDoc;
        return;
    }

    sidebar->markOpenedFile(filePath);

    {
        QMutexLocker locker(&docMutex);
        if (doc) delete doc;
        doc = newDoc;
        doc->setRenderBackend(Poppler::Document::SplashBackend);
    }

    viewPort->setDocument(doc, &docMutex);
    viewPort->setFilePath(filePath);
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

void MainWindow::loadSettings() {
    QSettings settings("OrionCorp", "PDFReader");
    
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (defaultPath.isEmpty()) defaultPath = QDir::homePath();

    m_libraryPath = settings.value("libPath", defaultPath).toString();
    
    if (!QDir(m_libraryPath).exists()) {
        m_libraryPath = defaultPath;
    }
}

void MainWindow::saveSettings() {
    QSettings settings("OrionCorp", "PDFReader");
    settings.setValue("libPath", m_libraryPath);
}

void MainWindow::onChangeLibraryPath() {
    QString dir = QFileDialog::getExistingDirectory(this, "Выбрать папку библиотеки", m_libraryPath);
    if (!dir.isEmpty()) {
        m_libraryPath = dir;
        saveSettings();
        sidebar->scanDirectory(m_libraryPath);
    }
}