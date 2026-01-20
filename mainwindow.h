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

#include "librarysidebar.h"
#include "pdfviewport.h"
#include "pdfsearchpanel.h"
#include "custom_widgets.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile(const QString &filePath);
    void onPageFound(int index, QString text, QRectF rect);
    void onSearchReset();
    void onZoomChanged(double value);
    void onFitWidthToggled(bool checked);
    void onChangeLibraryPath();

private:
    void loadSettings();
    void saveSettings();
    void setupUI();

    Poppler::Document *doc = nullptr;
    QMutex docMutex;
    QString m_libraryPath;

    LibrarySidebar *sidebar;
    PdfViewPort *viewPort;
    PdfSearchPanel *searchPanel;

    InvertedSpinBox *pageSelector;
    QLabel *totalPagesLabel;
    QDoubleSpinBox *zoomSpinBox;
    QCheckBox *fitWidthCheck;
};

#endif // MAINWINDOW_H

  