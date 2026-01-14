/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
#include "librarysidebar.h"

LibrarySidebar::LibrarySidebar(QWidget *parent) : QTreeWidget(parent) {
    setHeaderLabel("Библиотека");
    setFixedWidth(300);
    connect(this, &LibrarySidebar::itemClicked, this, &LibrarySidebar::onItemClickedInternal);
}

void LibrarySidebar::onItemClickedInternal(QTreeWidgetItem *item, int column) {
    Q_UNUSED(column);
    QString path = item->data(0, Qt::UserRole).toString();
    if (!path.isEmpty() && QFile::exists(path)) {
        emit fileSelected(path);
    }
}

void LibrarySidebar::scanDirectory(const QString &path) {
    clear();
    QDir mainDir(path);
    if (!mainDir.exists()) mainDir.mkpath(".");

    auto addFiles = [&](QTreeWidgetItem *parentItem, QDir dir) {
        QStringList pdfs = dir.entryList({"*.pdf"}, QDir::Files);
        for (const QString &file : pdfs) {
            QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
            item->setText(0, file);
            item->setData(0, Qt::UserRole, dir.absoluteFilePath(file));
        }
    };

    QTreeWidgetItem *rootItem = new QTreeWidgetItem(this);
    rootItem->setText(0, "Корневая папка");
    rootItem->setExpanded(true);
    addFiles(rootItem, mainDir);

    for (const QString &dirName : mainDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QTreeWidgetItem *dirItem = new QTreeWidgetItem(this);
        dirItem->setText(0, dirName.toUpper());
        QDir subDir(mainDir.absoluteFilePath(dirName));
        addFiles(dirItem, subDir);
        if (dirItem->childCount() > 0) dirItem->setExpanded(true);
    }
}
