#include "trigram.h"

trigram::trigram(QHash<QString, QSet<int64_t>> *tri_map) : tri_map(tri_map) {}
trigram::trigram(QHash<QString, QSet<int64_t>> *tri_map, QString str) : tri_map(tri_map), str(str) {
    pows.resize(str.length() + 1);
    pows[0] = 1;
    for (int i = 1; i <= str.length(); i++)
        pows[i] = pows[i - 1] * p;
}

void trigram::find_trigrams() {
    aborted = false;

    for (auto &file_name : tri_map->keys()) {
        if (aborted)
            return;
        QFile file(file_name);
        QSet<qint64>* trigrams = &tri_map->find(file_name).value();

        bool is_text = true;
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream stream(&file);
            QString str;
            while (!stream.atEnd()) {
                if (aborted)
                    return;

                str += stream.read(buf);
                if (!trigram::parse_file_trigrams(trigrams, str)) {
                    tri_map->remove(file_name);
                    is_text = false;
                    break;
                }
                str = str.mid(str.length() - 2, 2);
            }
            emit file_scanned(is_text);
            file.close();
        }
    }

    emit scanning_finished();
}

bool trigram::parse_file_trigrams(QSet<qint64>* trigrams, QString str) {
    if (str.size() <= 2)
        return false;

    auto data = str.data();
    qint64 tri = (qint64(data[0].unicode()) << 16) + (qint64(data[1].unicode()) << 32);
    for (int i = 2; i < str.length(); i++) {
        tri = (tri >> 16) + (qint64(data[i].unicode()) << 32);
        trigrams->insert(tri);

        if (trigrams->size() > trigrams_limit)
            return false;
    }
    return true;
}

void trigram::search_files() {
    aborted = false;

    QSet<qint64>* str_trigrams = new QSet<qint64>();
    parse_file_trigrams(str_trigrams, str);
    for (auto &file_name : tri_map->keys()) {
        if (aborted)
            return;
        QFile file(file_name);

        QSet<qint64>* trigrams = &tri_map->find(file_name).value();

        bool flag = true;
        for (auto &tri : *str_trigrams) {
            if (!trigrams->contains(tri)) {
                flag = false;
                break;
            }
        }

        if (flag) {
            qint32 number = check_file(file_name);
            if (number > 0) {
                emit file_OK(file_name, number);
            }
        }
    }

    emit searching_finished();
}

void trigram::abort() {
   aborted = true;
}

qint32 trigram::check_file(QString file_name) {
    QFile file(file_name);
    qint32 number = 0;

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        QString file_str = stream.read(str.length());
        QString prev;
        if (file_str.length() < str.length()) {
            return 0;
        }

        qint64 hash = 0;
        for (int i = 0; i < str.length(); i++) {
            hash = (hash + str[i].unicode()) * p;
        }

        qint64 file_hash = 0;
        for (int i = 0; i < file_str.length(); i++) {
            file_hash = (file_hash + file_str[i].unicode()) * p;
        }

        if (hash == file_hash) {
            number++;
        }
        qint32 n = str.length();

        while (!stream.atEnd()) {
            prev = file_str;
            file_str = stream.read(buf);
            for (int i = 0; i < file_str.length(); i++) {
                qint32 ch;
                if (i - n < 0) {
                    ch = prev[prev.length() - n + i].unicode();
                } else {
                    ch = file_str[i - n].unicode();
                }

                file_hash = (file_hash - ch * pows[n] + file_str[i].unicode()) * p;
                if (hash == file_hash) {
                    number++;
                }
            }
        }
    }
    file.close();
    return number;
}
