#include "chattablemodel.h"
#include "settings.h"
#include "controller.h"

ChatTableModel::ChatTableModel(QObject *parent)
     : QAbstractTableModel(parent) {
    headers << QObject::tr("Address") << QObject::tr("Date/Time") << QObject::tr("Memo");
}

ChatTableModel::~ChatTableModel() {
    delete modeldata;
}

void ChatTableModel::replaceData(const QList<ChatItem>& data) {
    delete modeldata;
    modeldata = new QList<ChatItem>();

    // Copy over the data and sort it    
    std::copy(data.begin(), data.end(), std::back_inserter(*modeldata));
    std::sort(modeldata->begin(), modeldata->end(), [=] (auto a, auto b) {
        return a.datetime > b.datetime; // reverse sort
    });

    dataChanged(index(0, 0), index(modeldata->size()-1, columnCount(index(0,0))-1));
    layoutChanged();
}

bool ChatTableModel::exportToCsv(QString fileName) const {
    if (!modeldata)
        return false;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
        return false;

    QTextStream out(&file);   // we will serialize the data into the file

    // Write headers
    for (int i = 0; i < headers.length(); i++) {
        out << "\"" << headers[i] << "\",";
    }
    out << "\"Memo\"";
    out << endl;
    
    // Write out each row
    for (int row = 0; row < modeldata->length(); row++) {
        for (int col = 0; col < headers.length(); col++) {
            out << "\"" << data(index(row, col), Qt::DisplayRole).toString() << "\",";
        }

        // Memo
        out << "\"" << this->getMemo(row) << "\"";
        out << endl;
    }

    file.close();
    return true;
}


 int ChatTableModel::rowCount(const QModelIndex&) const
 {
    if (modeldata == nullptr) return 0;
    return modeldata->size();
 }

 int ChatTableModel::columnCount(const QModelIndex&) const
 {
    return headers.size();
 }

QString ChatTableModel::concatMultipleMemos(const ChatItem& dat) const {
    // Concat all the memos
    QString memo;
    for (auto item : dat.items) {
        if (!item.memo.trimmed().isEmpty()) {
            memo += item.address + ": \"" + item.memo + "\"\n";
        }
    }

    return memo;
};

QVariant ChatTableModel::data(const QModelIndex &index, int role) const {
    // Align numeric columns (confirmations, amount) right
    if (role == Qt::TextAlignmentRole && 
         (index.column() == Column::Confirmations || index.column() == Column::Amount))
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);

    auto dat = modeldata->at(index.row());
    if (role == Qt::ForegroundRole) {
        if (dat.confirmations <= 0) {
            QBrush b;
            b.setColor(Qt::red);
            return b;
        }

        // Else, just return the default brush
        QBrush b;
        b.setColor(Qt::black);
        return b;        
    }

 // Concat all the memos

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Column::Type: return dat.type;
        case Column::Memo:{

    QString memo;
    for (auto item : dat.items) {
        if (!item.memo.trimmed().isEmpty()) {
            memo +=  item.memo + "\"\n";
        }
    }

     return memo;
    

 }}}
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Column::Type: return dat.type;
        case Column::Address: {
                    auto addr = dat.address;
                    if (addr.trimmed().isEmpty()) 
                        return "(Shielded)";
                    else 
                        return addr;
                }
     
      case Column::Time: return QDateTime::fromMSecsSinceEpoch(dat.datetime *  (qint64)1000).toLocalTime().toString();
        
        case Column::Confirmations: return QString::number(dat.confirmations);
        case Column::Amount: {
            // Sum up all the amounts
            CAmount total;
            for (int i=0; i < dat.items.length(); i++) {
                total = total + dat.items[i].amount;
            }
            return total.toDecimalhushString();
        }
        }
    } 

    if (role == Qt::ToolTipRole) {
        switch (index.column()) {
        case Column::Type: {
                    // If there are multiple memos, then mark them as such
                    if (dat.items.length() == 1) {
                        auto memo = dat.items[0].memo;
                        if (memo.startsWith("hush:")) {
                            return Settings::paymentURIPretty(Settings::parseURI(memo));
                        } else {
                            return modeldata->at(index.row()).type + 
                            (memo.isEmpty() ? "" : " tx memo: \"" + memo + "\"");
                        }
                    } else {
                        return concatMultipleMemos(dat);
                    }
                }
        case Column::Address: {
                    auto addr = modeldata->at(index.row()).address;
                    if (addr.trimmed().isEmpty()) 
                        return "(Shielded)";
                    else 
                        return addr;
                }
        case Column::Time: return QDateTime::fromMSecsSinceEpoch(modeldata->at(index.row()).datetime * (qint64)1000).toLocalTime().toString();
        case Column::Confirmations: return QString("%1 Network Confirmations").arg(QString::number(dat.confirmations));
        case Column::Amount: {
            // Sum up all the amounts
            CAmount total;
            for (int i=0; i < dat.items.length(); i++) {
                total = total + dat.items[i].amount;
            }
            return total.toDecimalUSDString();
        }    
        }
    }

    if (role == Qt::DecorationRole && index.column() == 0) {        
        bool hasMemo = false;
        for (int i=0; i < dat.items.length(); i++) {
            if (!dat.items[i].memo.isEmpty()) {
                hasMemo = true;
            }
        }
        
        // If the memo is a Payment URI, then show a payment request icon
        if (dat.items.length() == 1 && dat.items[0].memo.startsWith("hush:")) {
            QIcon icon(":/icons/res/paymentreq.gif");
            return QVariant(icon.pixmap(16, 16));
        } else if (hasMemo) {
            // Return the info pixmap to indicate memo
            QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);            
            return QVariant(icon.pixmap(16, 16));
        } else {
            // Empty pixmap to make it align
            QPixmap p(16, 16);
            p.fill(Qt::white);
            return QVariant(p);
        }
    }

    return QVariant();
 }


 QVariant ChatTableModel::headerData(int section, Qt::Orientation orientation, int role) const
 {
     if (role == Qt::TextAlignmentRole && (section == Column::Confirmations || section == Column::Amount))
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);

     if (role == Qt::FontRole) {
         QFont f;
         f.setBold(true);
         return f;
     }

     if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
         return headers.at(section);
     }

     return QVariant();
 }

QString ChatTableModel::getTxId(int row) const {
    return modeldata->at(row).txid;
}

QString ChatTableModel::getMemo(int row) const {
  auto dat = modeldata->at(row);
    //return modeldata->at(row).memo;
    return concatMultipleMemos(dat);
}

qint64 ChatTableModel::getConfirmations(int row) const {
    return modeldata->at(row).confirmations;
}

QString ChatTableModel::getAddr(int row) const {
    return modeldata->at(row).address.trimmed();
}

qint64 ChatTableModel::getDate(int row) const {
    return modeldata->at(row).datetime;
}

QString ChatTableModel::getType(int row) const {
    return modeldata->at(row).type;
}

QString ChatTableModel::getAmt(int row) const {
    auto dat = modeldata->at(row);
    
    CAmount total;
    for (int i=0; i < dat.items.length(); i++) {
        total = total + dat.items[i].amount;
    }
    return total.toDecimalString();
}