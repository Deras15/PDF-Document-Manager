/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
//mainwindow.h  
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QMutex>
#include <QTabWidget> 
#include <QPointer>

#include "librarysidebar.h"
#include "pdfviewport.h"
#include "pdfsearchpanel.h"
#include "custom_widgets.h"

class PdfTab : public QWidget {
    Q_OBJECT
public:
    explicit PdfTab(const QString &path, QWidget *parent = nullptr);
    ~PdfTab();

    QString filePath;
    Poppler::Document *doc = nullptr;
    QMutex docMutex;

    PdfViewPort *viewPort = nullptr;
    PdfSearchPanel *searchPanel = nullptr;

    bool loadDocument();

signals:
    void pinRequested();
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFilePreview(const QString &filePath);
    void openFilePinned(const QString &filePath);
    void openFilesPinned(const QStringList &filePaths);
    void pinPreviewTab();
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onPageInViewChanged(int page);
    void onZoomRequested(bool zoomIn);
    void onPageFoundInTab(int index, QString text, QRectF rect); 
    void onSearchReset();
    void onZoomSpinChanged(double value);
    void onPageSpinChanged(int page);
    void toggleSearchPanel();

    void onChangeLibraryPath();

private:
    void loadSettings();
    void saveSettings();
    void setupUI();
    void updateSidebarMarkers();
    void internalOpenFile(const QString &filePath, bool preview);

    PdfTab *m_previewTab = nullptr;
    QString m_libraryPath;
    QTabWidget *tabWidget; 
    LibrarySidebar *sidebar;
    InvertedSpinBox *pageSelector;
    QLabel *totalPagesLabel;
    QDoubleSpinBox *zoomSpinBox;
    QPushButton *btnShowSearch;

    PdfTab* currentTab() const;
};

#endif // MAINWINDOW_H