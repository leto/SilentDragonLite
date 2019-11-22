#ifndef CHATTABLEMODEL_H
#define CHATTABLEMODEL_H

#include "precompiled.h"

struct ChatItem;

class ChatTableModel: public QAbstractTableModel
{
public:
    ChatTableModel(QObject* parent);    
    ~ChatTableModel();

    enum Column
     {
        Type = 5,
        Address = 0,
        Time = 1,
        Confirmations = 4,
        Amount = 3,
        Memo = 2
    };

    void replaceData    (const QList<ChatItem>& data);

    QString  getTxId(int row) const;
    QString  getMemo(int row) const;
    QString  getAddr(int row) const;
    qint64   getDate(int row) const;
    QString  getType(int row) const;
    qint64   getConfirmations(int row) const;
    QString  getAmt (int row) const;

    bool     exportToCsv(QString fileName) const;

    int      rowCount(const QModelIndex &parent) const;
    int      columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private:
    QString concatMultipleMemos(const ChatItem&) const;

    QList<ChatItem>* modeldata    = nullptr;

    QList<QString>           headers;
};


#endif // CHATSTABLEMODEL_H
