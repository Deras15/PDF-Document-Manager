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
#include <QFileInfo>

PdfTab::PdfTab(const QString &path, QWidget *parent) 
    : QWidget(parent), filePath(path) 
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    viewPort = new PdfViewPort(this);
    searchPanel = new PdfSearchPanel(this);
    searchPanel->hide();
    searchPanel->setStyleSheet("background: #eee; border-top: 1px solid #ccc; padding: 5px;");

    layout->addWidget(viewPort, 1);
    layout->addWidget(searchPanel);
}

PdfTab::~PdfTab() {
    viewPort->stopAllRenders();
    QMutexLocker locker(&docMutex);
    if (doc) {
        delete doc;
        doc = nullptr;
    }
}

bool PdfTab::loadDocument() {
    Poppler::Document *newDoc = Poppler::Document::load(filePath);
    if (!newDoc || newDoc->isLocked()) {
        delete newDoc;
        return false;
    }
    
    {
        QMutexLocker locker(&docMutex);
        doc = newDoc;
        doc->setRenderBackend(Poppler::Document::SplashBackend);
    }

    viewPort->setFilePath(filePath);
    viewPort->setDocument(doc, &docMutex);
    
    searchPanel->setFilePath(filePath);
    searchPanel->setDocument(doc, &docMutex);

    return true;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    loadSettings();
    setWindowTitle("Orion PDF Reader");
    resize(1030, 700);

    setupUI();
    sidebar->scanDirectory(m_libraryPath); 
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    QMenu *settingsMenu = menuBar()->addMenu("Настройки");
    
    QAction *setPathAction = new QAction("Путь к библиотеке...", this);
    connect(setPathAction, &QAction::triggered, this, &MainWindow::onChangeLibraryPath);
    settingsMenu->addAction(setPathAction);
    
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    sidebar = new LibrarySidebar();
    sidebar->setMinimumWidth(150);
    sidebar->setMaximumWidth(500); 
    connect(sidebar, &LibrarySidebar::fileSelected, this, &MainWindow::openFile);
    
    QWidget *rightContainer = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QWidget *topBar = new QWidget();
    topBar->setStyleSheet("background: #f5f5f5; border-bottom: 1px solid #ddd;");
    topBar->setFixedHeight(50);
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);

    btnShowSearch = new QPushButton("🔍");
    btnShowSearch->setFixedSize(35, 35);
    btnShowSearch->setToolTip("Поиск (Ctrl+F)");
    btnShowSearch->setStyleSheet("QPushButton { border: 1px solid #ccc; border-radius: 4px; background: white; font-size: 16px; } QPushButton:hover { background: #eee; }");
    connect(btnShowSearch, &QPushButton::clicked, this, &MainWindow::toggleSearchPanel);
    topLayout->addWidget(btnShowSearch);
    topLayout->addStretch();

    zoomSpinBox = new QDoubleSpinBox();
    zoomSpinBox->setRange(10.0, 300.0);
    zoomSpinBox->setValue(100.0);
    zoomSpinBox->setSingleStep(10.0);
    zoomSpinBox->setSuffix("%");
    zoomSpinBox->setEnabled(true);
    connect(zoomSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onZoomSpinChanged);

    topLayout->addWidget(zoomSpinBox);

    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFixedHeight(20);
    topLayout->addWidget(line);

    pageSelector = new InvertedSpinBox();
    pageSelector->setMinimum(1);
    pageSelector->setFixedWidth(60);
    connect(pageSelector, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onPageSpinChanged);
    
    totalPagesLabel = new QLabel("/ 0");
    
    topLayout->addWidget(pageSelector);
    topLayout->addWidget(totalPagesLabel);

    tabWidget = new QTabWidget();
    tabWidget->setTabsClosable(true);
    tabWidget->setMovable(true);
    tabWidget->setDocumentMode(true); 
    
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    rightLayout->addWidget(topBar);
    rightLayout->addWidget(tabWidget, 1); 

    mainSplitter->addWidget(sidebar);
    mainSplitter->addWidget(rightContainer);
    mainSplitter->setCollapsible(0, false);
    mainSplitter->setStretchFactor(1, 1);
    
    setCentralWidget(mainSplitter);

    QShortcut *searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, &MainWindow::toggleSearchPanel);
}

PdfTab* MainWindow::currentTab() const {
    return qobject_cast<PdfTab*>(tabWidget->currentWidget());
}

void MainWindow::openFile(const QString &filePath) {
    for (int i = 0; i < tabWidget->count(); ++i) {
        PdfTab *tab = qobject_cast<PdfTab*>(tabWidget->widget(i));
        if (tab && tab->filePath == filePath) {
            tabWidget->setCurrentIndex(i);
            return;
        }
    }

    PdfTab *newTab = new PdfTab(filePath, this);
    if (!newTab->loadDocument()) {
        delete newTab;
        return;
    }

    connect(newTab->viewPort, &PdfViewPort::pageInViewChanged, this, &MainWindow::onPageInViewChanged);
    connect(newTab->viewPort, &PdfViewPort::zoomRequested, this, &MainWindow::onZoomRequested);
    
    connect(newTab->searchPanel, &PdfSearchPanel::pageFound, 
            [this, newTab](int index, QString text, QRectF rect){
        if (currentTab() == newTab) { 
            this->onPageFoundInTab(index, text, rect);
        }
    });
    
    connect(newTab->searchPanel, &PdfSearchPanel::searchReset, this, &MainWindow::onSearchReset);
    newTab->viewPort->setZoom(1.0);

    QFileInfo fi(filePath);
    int index = tabWidget->addTab(newTab, fi.fileName());

    tabWidget->setCurrentIndex(index); 
    tabWidget->setTabToolTip(index, filePath);

    updateSidebarMarkers();

    sidebar->selectFile(filePath);
}

void MainWindow::onTabCloseRequested(int index) {
    QWidget *w = tabWidget->widget(index);
    tabWidget->removeTab(index);
    delete w; 
    
    updateSidebarMarkers();

    if (tabWidget->count() == 0) {
        pageSelector->setValue(1);
        pageSelector->setMaximum(1);
        totalPagesLabel->setText("/ 0");
        setWindowTitle("Orion PDF Reader");
    }
}

void MainWindow::onTabChanged(int index) {
    if (index < 0) return;

    PdfTab *tab = currentTab();
    if (!tab || !tab->doc) return;

    setWindowTitle(QString("Orion PDF Reader - %1").arg(QFileInfo(tab->filePath).fileName()));

    int total = tab->doc->numPages();
    pageSelector->setMaximum(total);
    totalPagesLabel->setText(QString("/ %1").arg(total));
    
    zoomSpinBox->blockSignals(true);
    zoomSpinBox->setValue(tab->viewPort->getZoom() * 100.0);
    zoomSpinBox->blockSignals(false);
    
    sidebar->selectFile(tab->filePath);
}

void MainWindow::updateSidebarMarkers() {
    QStringList paths;
    for (int i = 0; i < tabWidget->count(); ++i) {
        PdfTab *tab = qobject_cast<PdfTab*>(tabWidget->widget(i));
        if (tab) {
            paths.append(tab->filePath);
        }
    }
    sidebar->updateOpenedFiles(paths);
}

void MainWindow::onPageInViewChanged(int page) {
    PdfTab *tab = currentTab();
    if (tab && sender() == tab->viewPort) {
        if (pageSelector->value() != page) {
            pageSelector->blockSignals(true);
            pageSelector->setValue(page);
            pageSelector->blockSignals(false);
        }
    }
}

void MainWindow::onZoomRequested(bool zoomIn) {
    PdfTab *tab = currentTab();
    if (tab && sender() == tab->viewPort) {
        double currentZoomPercent = tab->viewPort->getZoom() * 100.0;
        double step = 10.0;

        if (zoomIn) {
            currentZoomPercent += step;
        } else {
            currentZoomPercent -= step;
        }
        
        zoomSpinBox->setValue(currentZoomPercent);
    }
}

void MainWindow::onPageFoundInTab(int index, QString text, QRectF rect) {
    PdfTab *tab = currentTab();
    if (!tab) return;
    
    double yFraction = 0.0;
    {
        QMutexLocker locker(&tab->docMutex);
        if (tab->doc) {
            Poppler::Page *p = tab->doc->page(index);
            if (p) {
                yFraction = rect.top() / p->pageSize().height();
                delete p;
            }
        }
    }
    
    tab->viewPort->updateHighlight(text, rect);
    tab->viewPort->goToPage(index + 1, yFraction - 0.1);
}

void MainWindow::onSearchReset() {
    PdfTab *tab = currentTab();
    if (tab && sender() == tab->searchPanel) {
        tab->viewPort->clearSearch();
    }
}

void MainWindow::toggleSearchPanel() {
    PdfTab *tab = currentTab();
    if (!tab) return;

    if (tab->searchPanel->isVisible()) {
        tab->searchPanel->hide();
    } else {
        tab->searchPanel->show();
        tab->searchPanel->focusIn();
    }
}

void MainWindow::onZoomSpinChanged(double value) {
    PdfTab *tab = currentTab();
    if (tab) {
        tab->viewPort->setZoom(value / 100.0);
    }
}

void MainWindow::onPageSpinChanged(int page) {
    PdfTab *tab = currentTab();
    if (tab) {
        tab->viewPort->goToPage(page);
    }
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
