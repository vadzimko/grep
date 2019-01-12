#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTime>
#include <QCommonStyle>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QDirIterator>
#include <QDesktopServices>
#include <QtGlobal>
#include <QDebug>
#include <QThread>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QTreeWidgetItem>
#include <trigram.h>

namespace Ui {
class MainWindow;
}

enum class Status
{
    CHILLING,
    SCANNING_FILES,
    INDEXING,
    SEARCHING
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_open_clicked();
    void on_cancel_clicked();
    void on_search_button_clicked();
    void on_scan_clicked();

    void scan();

    void on_tree_itemDoubleClicked(QTreeWidgetItem *item, int column);

    void file_scanned(bool is_text);
    void file_searched(QString file, qint32 number);

signals:
    void stop();

private:
    Ui::MainWindow *ui;
    QTime timer;
    QString directory;
    QString dirView;

    bool cancelRequested();
    void startScan();
    void finishScan();
    void startSearch();
    void finishSearch();
    void refreshStatus();
    void addFileWithEntry(QString str, qint64 number);

    bool aborted;
    bool finished = true;
    Status status;

    qint64 sizeSum;
    qint64 sizeProcessed;

    qint32 filesNumber;
    qint32 filesNumberProcessed;
    qint32 filesProcessedText;
    qint32 filesOK;

    QHash<QString, QSet<int64_t>> *tri_map = nullptr;
    trigram* tri = nullptr;
    QFuture<void> refresher;
};

#endif // MAINWINDOW_H
