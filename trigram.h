#ifndef TRIGRAM_H
#define TRIGRAM_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QFile>

class trigram : public QObject
{
    Q_OBJECT

public:
    trigram(QHash<QString, QSet<int64_t>> *tri_map);
    trigram(QHash<QString, QSet<int64_t>> *tri_map, QString str);

public slots:
    void find_trigrams();
    void search_files();
    void abort();

signals:
    void file_scanned(bool is_text);
    void file_OK(QString file, qint32 number);
    void scanning_finished();
    void searching_finished();

private:
    QHash<QString, QSet<int64_t>> *tri_map;
    bool parse_file_trigrams(QSet<qint64>* trigrams, QString str);
    qint32 check_file(QString file);

    bool aborted;
    const qint32 buf = 1 << 20;
    const qint32 trigrams_limit = 50000;

    QString str;

    int const p = 31;
    QVector<qint64> pows;
};


#endif // TRIGRAM_H
