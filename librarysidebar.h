    
/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
 //librarysidebar.h
#ifndef LIBRARYSIDEBAR_H
#define LIBRARYSIDEBAR_H

#include <QTreeWidget>
#include <QDir>
#include <QHeaderView>

class LibrarySidebar : public QTreeWidget {
    Q_OBJECT
public:
    explicit LibrarySidebar(QWidget *parent = nullptr);
    void scanDirectory(const QString &path);
    void updateOpenedFiles(const QStringList &filePaths);
    void selectFile(const QString &filePath);

signals:
    void fileSelected(const QString &filePath);
    void fileDoubleClicked(const QString &filePath);
    void folderDoubleClicked(const QStringList &filePaths);

private slots:
    void onItemClickedInternal(QTreeWidgetItem *item, int column);
    void onItemDoubleClickedInternal(QTreeWidgetItem *item, int column);
};

#endif // LIBRARYSIDEBAR_H