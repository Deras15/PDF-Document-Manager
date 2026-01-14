/*
 * PDF Reader
 * Copyright (c) 2026 [Muzon4ik]
 * 
 * Restricted License:
 * This project is for portfolio demonstration and educational use only.
 * Commercial use, resale, or distribution for profit is strictly prohibited.
 */
//pdfsearchpanel.cpp
#include "pdfsearchpanel.h"
#include <QtConcurrent>

PdfSearchPanel::PdfSearchPanel(QWidget *parent) : QWidget(parent) {
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    searchField = new QLineEdit();
    searchField->setPlaceholderText("Поиск по документу...");
    searchField->setFixedWidth(300);
    
    btnStart = new QPushButton("Найти");
    lblStatus = new QLabel("");
    lblStatus->setStyleSheet("font-weight: bold; color: #2980b9;");

    navWidget = new QWidget();
    QHBoxLayout *navLayout = new QHBoxLayout(navWidget);
    navLayout->setContentsMargins(0,0,0,0);
    
    btnPrev = new QPushButton("<");
    btnNext = new QPushButton(">");
    btnReset = new QPushButton("X");
    
    navLayout->addWidget(btnPrev);
    navLayout->addWidget(lblStatus);
    navLayout->addWidget(btnNext);
    navLayout->addWidget(btnReset);
    navWidget->setVisible(false);

    layout->addWidget(searchField);
    layout->addWidget(btnStart);
    layout->addWidget(navWidget);

    searchWatcher = new QFutureWatcher<QList<QPair<int, QRectF>>>(this);
    connect(searchWatcher, &QFutureWatcher<QList<QPair<int, QRectF>>>::finished, this, &PdfSearchPanel::onSearchFinished);
    connect(btnStart, &QPushButton::clicked, this, &PdfSearchPanel::onFindStart);
    connect(searchField, &QLineEdit::returnPressed, this, &PdfSearchPanel::onFindStart);
    connect(btnNext, &QPushButton::clicked, this, &PdfSearchPanel::onNext);
    connect(btnPrev, &QPushButton::clicked, this, &PdfSearchPanel::onPrev);
    connect(btnReset, &QPushButton::clicked, this, &PdfSearchPanel::onReset);
}

void PdfSearchPanel::setDocument(Poppler::Document *newDoc, QMutex *mutex) {
    cancelSearch(); 
    doc = newDoc;
    docMutex = mutex;
    onReset();
}

void PdfSearchPanel::cancelSearch() {
    currentSearchCanceled.store(1);
    if (searchWatcher->isRunning()) searchWatcher->waitForFinished(); 
}

void PdfSearchPanel::onFindStart() {
    QString text = searchField->text().trimmed();
    if (text.isEmpty() || !doc || !docMutex) return;

    onReset();
    currentSearchCanceled.store(0);
    lblStatus->setText("...");
    btnStart->setEnabled(false);

    QFuture<QList<QPair<int, QRectF>>> future = QtConcurrent::run([this, text]() {
        QList<QPair<int, QRectF>> results;
        int nPages = 0;
        { QMutexLocker locker(this->docMutex); if (this->doc) nPages = this->doc->numPages(); }

        for (int i = 0; i < nPages; ++i) {
            if (this->currentSearchCanceled.load() == 1) break;
            QMutexLocker locker(this->docMutex);
            if (this->doc) {
                Poppler::Page *page = this->doc->page(i);
                if (page) {
                    for (const QRectF &rect : page->search(text, Poppler::Page::IgnoreCase))
                        results.append(qMakePair(i, rect));
                    delete page;
                }
            }
        }
        return results;
    });
    searchWatcher->setFuture(future);
}

void PdfSearchPanel::onSearchFinished() {
    btnStart->setEnabled(true);
    if (currentSearchCanceled.load() == 1) return;
    searchResults = searchWatcher->result();
    navWidget->setVisible(true);
    if (searchResults.isEmpty()) {
        lblStatus->setText("0/0");
    } else {
        currentIndex = 0;
        lblStatus->setText(QString("1/%1").arg(searchResults.size()));
        emit pageFound(searchResults[0].first, searchField->text(), searchResults[0].second);
    }
}

void PdfSearchPanel::onNext() {
    if (searchResults.isEmpty()) return;
    currentIndex = (currentIndex + 1) % searchResults.size();
    lblStatus->setText(QString("%1/%2").arg(currentIndex + 1).arg(searchResults.size()));
    emit pageFound(searchResults[currentIndex].first, searchField->text(), searchResults[currentIndex].second);
}

void PdfSearchPanel::onPrev() {
    if (searchResults.isEmpty()) return;
    currentIndex = (currentIndex - 1 + searchResults.size()) % searchResults.size();
    lblStatus->setText(QString("%1/%2").arg(currentIndex + 1).arg(searchResults.size()));
    emit pageFound(searchResults[currentIndex].first, searchField->text(), searchResults[currentIndex].second);
}

void PdfSearchPanel::onReset() {
    currentSearchCanceled.store(1);
    searchResults.clear();
    currentIndex = -1;
    navWidget->setVisible(false);
    emit searchReset(); 
}