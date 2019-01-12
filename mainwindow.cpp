#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->tree->setUniformRowHeights(true);
    ui->lineEdit->setPlaceholderText(QString("Type here..."));
    setWindowTitle(QString("Google"));

    qRegisterMetaType<QVector<int> >("QVector<int>");
}

MainWindow::~MainWindow()
{
    delete ui;
    delete tri_map;
    delete tri;
}

void MainWindow::on_open_clicked()
{
    if (!finished) {
        aborted = true;
    }

    directory = QFileDialog::getExistingDirectory(this, "Select directory", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    dirView = "\'..." + directory.mid(qMax(0, directory.length() - 50), 50) + "\'";
    ui->directoryName->setText("Selected " + dirView);

    ui->tree->clear();
    ui->lineEdit->setEnabled(false);
    ui->search_button->setEnabled(false);
    ui->lineEdit->setPlaceholderText(QString("Type here..."));
}

void MainWindow::startScan()
{
    timer.start();
    aborted = false;
    finished = false;

    sizeSum = 0;
    sizeProcessed = 0;
    filesNumber = 0;
    filesNumberProcessed = 0;
    filesProcessedText = 0;

    ui->tree->clear();
    ui->lineEdit->clear();
    ui->lineEdit->setEnabled(false);
    ui->search_button->setEnabled(false);
    ui->lineEdit->setPlaceholderText(QString("Type here..."));
}

void MainWindow::finishScan()
{
    finished = true;
    status = Status::CHILLING;

    if (!aborted) {
        ui->status->setText("Finished in " + QString::number(timer.elapsed() / 1000.0) + " sec");
    } else {
        ui->status->setText("Scanning aborted, took " + QString::number(timer.elapsed() / 1000.0) + " sec");
    }

    if (filesProcessedText > 0) {
        ui->lineEdit->setPlaceholderText(QString("Type here... Search in " + QString::number(filesProcessedText) + " files"));
        ui->lineEdit->setEnabled(true);
        ui->search_button->setEnabled(true);
    } else {
        ui->lineEdit->setPlaceholderText(QString("No files for search, choose another directory"));
    }
    refresher.waitForFinished();
}

void MainWindow::on_scan_clicked()
{
    if (directory.length() == 0) {
        ui->directoryName->setText("Choose directory to scan!");
        return;
    }
    if (!finished) {
        finishScan();
    }
    startScan();

    QFuture<void> searcher = QtConcurrent::run(this, &MainWindow::scan);
    refresher = QtConcurrent::run(this, &MainWindow::refreshStatus);
}

bool MainWindow::cancelRequested() {
    if (aborted) {
        if (!finished) {
            if (status == Status::INDEXING || status == Status::SCANNING_FILES) {
                finishScan();
            } else {
                finishSearch();
            }
        }
        return true;
    }
    return false;
}

void MainWindow::scan() {
    status = Status::SCANNING_FILES;
    if (tri_map != nullptr) {
        delete tri_map;
    }
    tri_map = new QHash<QString, QSet<int64_t>>();

    QDirIterator it(directory, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    QVector<QString> files;
    while (it.hasNext()) {
        if (cancelRequested()) {
            return;
        }

        auto file = it.next();
        QFile check_file(file);
        check_file.open(QIODevice::ReadOnly);
        if (!check_file.isOpen()) {
            continue;
        }

        tri_map->insert(file, QSet<qint64>());
        filesNumber++;
    }

    status = Status::INDEXING;
    if (tri != nullptr) {
        delete tri;
    }
    tri = new trigram(tri_map);

    QThread* thread = new QThread();
    tri->moveToThread(thread);

    connect(thread, &QThread::started, tri, &trigram::find_trigrams);
    connect(tri, &trigram::file_scanned, this, &MainWindow::file_scanned);
    connect(tri, &trigram::scanning_finished, this, &MainWindow::finishScan);
    connect(this, &MainWindow::stop, tri, &trigram::abort);
    connect(thread, &QThread::finished, this, &MainWindow::finishScan);
    thread->start();
}

void MainWindow::on_cancel_clicked()
{
    aborted = true;
    if (status == Status::SEARCHING || status == Status::INDEXING) {
        QFuture<void> aborter = QtConcurrent::run(tri, &trigram::abort);
        aborter.waitForFinished();
        cancelRequested();
    }
}

void MainWindow::refreshStatus()
{
    qint16 dots = 1;
    while (!finished) {
        QString dot = "";
        for (int i = 0; i < dots / 5; i++) {
            dot += '.';
        }
        switch (status) {
            case Status::CHILLING : {
                ui->status->setText("Finished");
                break;
            }
            case Status::SCANNING_FILES : {
                qint32 time = timer.elapsed() / 1000;
                ui->status->setText("Indexing files " + QString::number(time) + " sec" + dot);
                break;
            }
            case Status::INDEXING : {
                double progress = qMin(100.0, filesNumberProcessed * 100.0 / filesNumber);
                ui->status->setText(QString().sprintf("%.1f", progress) + "%" + dot);
                break;
            }
            case Status::SEARCHING : {
                ui->status->setText("Searching" + dot);
                break;
            }
        }
        dots = (dots % 15) + 1;
        QThread::msleep(100);
    }
}

void MainWindow::addFileWithEntry(QString str, qint64 number) {
    auto info = new QTreeWidgetItem(ui->tree);
    info->setText(0, str);
    info->setText(1, QString::number(number));
    ui->tree->addTopLevelItem(info);
}

void MainWindow::on_tree_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(item->text(column)));
}

void MainWindow::on_search_button_clicked()
{
    QString str = ui->lineEdit->text();
    if (str.length() < 3) {
        ui->status->setText("Input length must be not less than 3");
        return;
    }
    startSearch();

    tri = new trigram(tri_map, str);
    QThread* thread = new QThread();
    tri->moveToThread(thread);

    connect(thread, &QThread::started, tri, &trigram::search_files);
    connect(tri, &trigram::file_OK, this, &MainWindow::file_searched);
    connect(tri, &trigram::searching_finished, this, &MainWindow::finishSearch);
    connect(this, &MainWindow::stop, tri, &trigram::abort);
    connect(thread, &QThread::finished, this, &MainWindow::finishSearch);
    thread->start();

    refresher = QtConcurrent::run(this, &MainWindow::refreshStatus);
}

void MainWindow::startSearch() {
    if (!finished) {
        finishSearch();
    }
    ui->tree->clear();
    status = Status::SEARCHING;
    aborted = false;
    finished = false;

    filesOK = 0;
    timer.start();
}


void MainWindow::finishSearch() {
    status = Status::CHILLING;
    finished = true;
    refresher.waitForFinished();

    if (!aborted) {
        ui->status->setText("Searhing finished in " + QString::number(timer.elapsed() / 1000.0) + " sec, " +
                            QString::number(filesOK) + " files found");
    } else {
        ui->status->setText("Searcing aborted, took " + QString::number(timer.elapsed() / 1000.0) + " sec, " +
                            QString::number(filesOK) + " files found");
    }
}

void MainWindow::file_scanned(bool is_text) {
    filesNumberProcessed++;
    if (is_text) {
        filesProcessedText++;
    }
}

void MainWindow::file_searched(QString file, qint32 number) {
    addFileWithEntry(file, number);
    filesOK++;
}

