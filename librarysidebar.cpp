/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
//librarysidebar.cpp
#include "librarysidebar.h"
#include <QDirIterator>
#include <QDebug>
#include <QTreeWidgetItemIterator>

LibrarySidebar::LibrarySidebar(QWidget *parent) : QTreeWidget(parent) {
    setHeaderLabel("Библиотека");
    setColumnCount(1);
    
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
    if (!mainDir.exists()) return;

    QStringList filters = {"*.pdf", "*.PDF"};

    QStringList rootFiles = mainDir.entryList(filters, QDir::Files);
    for (const QString &file : rootFiles) {
        QTreeWidgetItem *item = new QTreeWidgetItem(this);
        item->setText(0, file);
        item->setData(0, Qt::UserRole, mainDir.absoluteFilePath(file));
    }

    QStringList subDirs = mainDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dirName : subDirs) {
        QDir subDir(mainDir.absoluteFilePath(dirName));
        QStringList subFiles = subDir.entryList(filters, QDir::Files);

        if (!subFiles.isEmpty()) {
            QTreeWidgetItem *dirItem = new QTreeWidgetItem(this);
            dirItem->setText(0, dirName.toUpper());
            dirItem->setExpanded(true);

            for (const QString &file : subFiles) {
                QTreeWidgetItem *fileItem = new QTreeWidgetItem(dirItem);
                fileItem->setText(0, file);
                fileItem->setData(0, Qt::UserRole, subDir.absoluteFilePath(file));
            }
        }
    }
}

void LibrarySidebar::markOpenedFile(const QString &filePath) {
    QTreeWidgetItemIterator it(this);
    while (*it) {
        QString itemPath = (*it)->data(0, Qt::UserRole).toString();
        QString currentText = (*it)->text(0);

        if (currentText.startsWith("● ")) {
            currentText = currentText.mid(2);
            (*it)->setText(0, currentText);
            (*it)->setFont(0, font());
        }

        if (!itemPath.isEmpty() && itemPath == filePath) {
            (*it)->setText(0, "● " + currentText);
            
            QFont boldFont = (*it)->font(0);
            boldFont.setBold(true);
            (*it)->setFont(0, boldFont);
            
            scrollToItem(*it);
        }
        ++it;
    }
}