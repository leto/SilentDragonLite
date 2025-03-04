#include "controller.h"

#include "addressbook.h"
#include "settings.h"
#include "version.h"
#include "camount.h"
#include "websockets.h"

using json = nlohmann::json;

Controller::Controller(MainWindow* main) {
    auto cl = new ConnectionLoader(main, this);

    // Execute the load connection async, so we can set up the rest of RPC properly. 
    QTimer::singleShot(1, [=]() { cl->loadConnection(); });

    this->main = main;
    this->ui = main->ui;

    // Setup balances table model
    balancesTableModel = new BalancesTableModel(main->ui->balancesTable);
    main->ui->balancesTable->setModel(balancesTableModel);

    // Setup transactions table model
    transactionsTableModel = new TxTableModel(ui->transactionsTable);
    main->ui->transactionsTable->setModel(transactionsTableModel);
    
    // Set up timer to refresh Price
    priceTimer = new QTimer(main);
    QObject::connect(priceTimer, &QTimer::timeout, [=]() {
        if (Settings::getInstance()->getAllowFetchPrices()) 
            refreshZECPrice();          
        
    });
    priceTimer->start(Settings::priceRefreshSpeed);  // Every 5 Min

    // Set up a timer to refresh the UI every few seconds
    timer = new QTimer(main);
    QObject::connect(timer, &QTimer::timeout, [=]() {
        refresh();
    });
    timer->start(Settings::updateSpeed);    

    // Create the data model
    model = new DataModel();

    // Crate the hushdRPC 
    zrpc = new LiteInterface();
}

Controller::~Controller() {
    delete timer;
    delete txTimer;

    delete transactionsTableModel;
    delete balancesTableModel;

    delete model;
    delete zrpc;
}

// Called when a connection to hushd is available. 
void Controller::setConnection(Connection* c) {
    if (c == nullptr) return;

    this->zrpc->setConnection(c);

    ui->statusBar->showMessage("");

    // If we're allowed to get the Hush Price, get the prices
    if (Settings::getInstance()->getAllowFetchPrices())
        refreshZECPrice();

    // If we're allowed to check for updates, check for a new release
    if (Settings::getInstance()->getCheckForUpdates())
        checkForUpdate();

    // Force update, because this might be coming from a settings update
    // where we need to immediately refresh
    refresh(true);
}

// Build the RPC JSON Parameters for this tx
void Controller::fillTxJsonParams(json& allRecepients, Tx tx) {   
    Q_ASSERT(allRecepients.is_array());

    // For each addr/amt/memo, construct the JSON and also build the confirm dialog box    
    for (int i=0; i < tx.toAddrs.size(); i++) {
        auto toAddr = tx.toAddrs[i];

        // Construct the JSON params
        json rec = json::object();
        rec["address"]      = toAddr.addr.toStdString();
        rec["amount"]       = toAddr.amount.toqint64();
        if (Settings::isZAddress(toAddr.addr) && !toAddr.memo.trimmed().isEmpty())
            rec["memo"]     = toAddr.memo.toStdString();

        allRecepients.push_back(rec);
    }
}

void Controller::noConnection() {    
    QIcon i = QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical);
    main->statusIcon->setPixmap(i.pixmap(16, 16));
    main->statusIcon->setToolTip("");
    main->statusLabel->setText(QObject::tr("No Connection"));
    main->statusLabel->setToolTip("");
    main->ui->statusBar->showMessage(QObject::tr("No Connection"), 1000);

    // Clear balances table.
    QMap<QString, CAmount> emptyBalances;
    QList<UnspentOutput>  emptyOutputs;
    QList<QString>        emptyAddresses;
    balancesTableModel->setNewData(emptyAddresses, emptyAddresses, emptyBalances, emptyOutputs);

    // Clear Transactions table.
    QList<TransactionItem> emptyTxs;
    transactionsTableModel->replaceData(emptyTxs);

    // Clear balances
    ui->balSheilded->setText("");
    ui->balTransparent->setText("");
    ui->balTotal->setText("");

    ui->balSheilded->setToolTip("");
    ui->balTransparent->setToolTip("");
    ui->balTotal->setToolTip("");
}

/// This will refresh all the balance data from hushd
void Controller::refresh(bool force) {
    if (!zrpc->haveConnection()) 
        return noConnection();

    getInfoThenRefresh(force);
}

void Controller::processInfo(const json& info) {
    // Testnet?
    QString chainName;
    if (!info["chain_name"].is_null()) {
        chainName = QString::fromStdString(info["chain_name"].get<json::string_t>());
        Settings::getInstance()->setTestnet(chainName == "test");
    };


    QString version = QString::fromStdString(info["version"].get<json::string_t>());
    Settings::getInstance()->sethushdVersion(version);

    // Recurring pamynets are testnet only
    if (!Settings::getInstance()->isTestnet())
        main->disableRecurring();
}

void Controller::getInfoThenRefresh(bool force) {
    if (!zrpc->haveConnection()) 
        return noConnection();

    static bool prevCallSucceeded = false;

    zrpc->fetchInfo([=] (const json& reply) {   
        prevCallSucceeded = true;       

        int curBlock  = reply["latest_block_height"].get<json::number_integer_t>();
        bool doUpdate = force || (model->getLatestBlock() != curBlock);
        model->setLatestBlock(curBlock);
        ui->blockHeight->setText(QString::number(curBlock));
        ui->Version->setText(QString::fromStdString(reply["version"].get<json::string_t>())); 
        ui->Vendor->setText(QString::fromStdString(reply["vendor"].get<json::string_t>()));

        main->logger->write(QString("Refresh. curblock ") % QString::number(curBlock) % ", update=" % (doUpdate ? "true" : "false") );

        // Connected, so display checkmark.
        auto tooltip = Settings::getInstance()->getSettings().server + "\n" + 
                            QString::fromStdString(zrpc->getConnection()->getInfo().dump());
        QIcon i(":/icons/res/connected.gif");
        QString chainName = Settings::getInstance()->isTestnet() ? "test" : "main";
        main->statusLabel->setText(chainName + "(" + QString::number(curBlock) + ")");

        // use currency ComboBox as input 

        if (Settings::getInstance()->get_currency_name() == "USD") {
        main->statusLabel->setText(" HUSH/USD=$ " + QString::number( (double) Settings::getInstance()->getZECPrice() ,'f',2));
    }   else if (Settings::getInstance()->get_currency_name() == "EUR") {
        main->statusLabel->setText(" HUSH/EUR=€ " + QString::number( (double) Settings::getInstance()->getEURPrice() ,'f',2));
    }   else if  (Settings::getInstance()->get_currency_name() == "BTC") {
        main->statusLabel->setText(" HUSH/BTC=BTC " + QString::number((double)  Settings::getInstance()->getBTCPrice() ,'f',8));
    }   else if (Settings::getInstance()->get_currency_name() == "CNY") {
        main->statusLabel->setText(" HUSH/CNY=¥ /元 " + QString::number( (double) Settings::getInstance()->getCNYPrice() ,'f',2));
    }   else if  (Settings::getInstance()->get_currency_name() == "RUB") {
        main->statusLabel->setText(" HUSH/RUB=₽ " + QString::number((double)  Settings::getInstance()->getRUBPrice() ,'f',2));
    }   else if (Settings::getInstance()->get_currency_name() == "CAD") {
        main->statusLabel->setText(" HUSH/CAD=$ " + QString::number( (double) Settings::getInstance()->getCADPrice() ,'f',2));
    }   else if  (Settings::getInstance()->get_currency_name() == "SGD") {
        main->statusLabel->setText(" HUSH/SGD=$ " + QString::number((double)  Settings::getInstance()->getSGDPrice() ,'f',2));
    }   else if  (Settings::getInstance()->get_currency_name() == "CHF") {
        main->statusLabel->setText(" HUSH/CHF=CHF " + QString::number((double)  Settings::getInstance()->getCHFPrice() ,'f',2));
    }   else if (Settings::getInstance()->get_currency_name() == "INR") {
        main->statusLabel->setText(" HUSH/INR=₹ " + QString::number( (double) Settings::getInstance()->getINRPrice() ,'f',2));
    }   else if  (Settings::getInstance()->get_currency_name() == "GBP") {
        main->statusLabel->setText(" HUSH/GBP=£ " + QString::number((double)  Settings::getInstance()->getGBPPrice() ,'f',2));
        }else if  (Settings::getInstance()->get_currency_name() == "AUD") {
        main->statusLabel->setText(" HUSH/AUD=$ " + QString::number((double)  Settings::getInstance()->getAUDPrice() ,'f',2));
        
    } else {
    main->statusLabel->setText(" HUSH/USD=$" + QString::number(Settings::getInstance()->getZECPrice(),'f',2 ));
    }
        main->statusLabel->setToolTip(tooltip);
        main->statusIcon->setPixmap(i.pixmap(16, 16));
        main->statusIcon->setToolTip(tooltip);
        // See if recurring payments needs anything
        Recurring::getInstance()->processPending(main);

        // Check if the wallet is locked/encrypted
        zrpc->fetchWalletEncryptionStatus([=] (const json& reply) {
            bool isEncrypted = reply["encrypted"].get<json::boolean_t>();
            bool isLocked = reply["locked"].get<json::boolean_t>();

            model->setEncryptionStatus(isEncrypted, isLocked);
        });

        if ( doUpdate ) {
            // Something changed, so refresh everything.
            refreshBalances();        
            refreshAddresses();     // This calls refreshZSentTransactions() and refreshReceivedZTrans()
            refreshTransactions();
        }
    }, [=](QString err) {
        // hushd has probably disappeared.
        this->noConnection();

        // Prevent multiple dialog boxes, because these are called async
        static bool shown = false;
        if (!shown && prevCallSucceeded) { // show error only first time
            shown = true;
            QMessageBox::critical(main, QObject::tr("Connection Error"), QObject::tr("There was an error connecting to hushd. The error was") + ": \n\n"
                + err, QMessageBox::StandardButton::Ok);
            shown = false;
        }

        prevCallSucceeded = false;
    });
}

void Controller::refreshAddresses() {
    if (!zrpc->haveConnection()) 
        return noConnection();
    
    auto newzaddresses = new QList<QString>();
    auto newtaddresses = new QList<QString>();

    zrpc->fetchAddresses([=] (json reply) {
        auto zaddrs = reply["z_addresses"].get<json::array_t>();
        for (auto& it : zaddrs) {   
            auto addr = QString::fromStdString(it.get<json::string_t>());
            newzaddresses->push_back(addr);
        }

        model->replaceZaddresses(newzaddresses);

        auto taddrs = reply["t_addresses"].get<json::array_t>();
        for (auto& it : taddrs) {   
            auto addr = QString::fromStdString(it.get<json::string_t>());
            if (Settings::isTAddress(addr))
                newtaddresses->push_back(addr);
        }

        model->replaceTaddresses(newtaddresses);

        // Refresh the sent and received txs from all these z-addresses
        refreshTransactions();
    });
    
}

// Function to create the data model and update the views, used below.
void Controller::updateUI(bool anyUnconfirmed) {    
    ui->unconfirmedWarning->setVisible(anyUnconfirmed);

    // Update balances model data, which will update the table too
    balancesTableModel->setNewData(model->getAllZAddresses(), model->getAllTAddresses(), model->getAllBalances(), model->getUTXOs());
};

// Function to process reply of the listunspent and z_listunspent API calls, used below.
void Controller::processUnspent(const json& reply, QMap<QString, CAmount>* balancesMap, QList<UnspentOutput>* unspentOutputs) {
    auto processFn = [=](const json& array) {        
        for (auto& it : array) {
            QString qsAddr  = QString::fromStdString(it["address"]);
            int block       = it["created_in_block"].get<json::number_unsigned_t>();
            QString txid    = QString::fromStdString(it["created_in_txid"]);
            CAmount amount  = CAmount::fromqint64(it["value"].get<json::number_unsigned_t>());

            bool spendable = it["unconfirmed_spent"].is_null() && it["spent"].is_null();    // TODO: Wait for 4 confirmations
            bool pending   = !it["unconfirmed_spent"].is_null();

            unspentOutputs->push_back(UnspentOutput{ qsAddr, txid, amount, block, spendable, pending });
            if (spendable) {
                (*balancesMap)[qsAddr] = (*balancesMap)[qsAddr] +
                                         CAmount::fromqint64(it["value"].get<json::number_unsigned_t>());
            }
        }
    };

    processFn(reply["unspent_notes"].get<json::array_t>());
    processFn(reply["utxos"].get<json::array_t>());
    processFn(reply["pending_notes"].get<json::array_t>());
    processFn(reply["pending_utxos"].get<json::array_t>());
};

void Controller::updateUIBalances() {
    CAmount balT = getModel()->getBalT();
    CAmount balZ = getModel()->getBalZ();
    CAmount balVerified = getModel()->getBalVerified();

    // Reduce the BalanceZ by the pending outgoing amount. We're adding
    // here because totalPending is already negative for outgoing txns.
    balZ = balZ + getModel()->getTotalPending();

    CAmount balTotal     = balT + balZ;
    CAmount balAvailable = balT + balVerified;

    // Balances table
    ui->balSheilded   ->setText(balZ.toDecimalhushString());
    ui->balVerified   ->setText(balVerified.toDecimalhushString());
    ui->balTransparent->setText(balT.toDecimalhushString());
    ui->balTotal      ->setText(balTotal.toDecimalhushString());

    if (Settings::getInstance()->get_currency_name() == "USD") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalUSDString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalUSDString());
    ui->balTransparent->setToolTip(balT.toDecimalUSDString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalUSDString());

   } else if (Settings::getInstance()->get_currency_name() == "EUR") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalEURString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalEURString());
    ui->balTransparent->setToolTip(balT.toDecimalEURString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalEURString());

    } else if (Settings::getInstance()->get_currency_name() == "BTC") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalBTCString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalBTCString());
    ui->balTransparent->setToolTip(balT.toDecimalBTCString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalBTCString()); 
    
     } else if (Settings::getInstance()->get_currency_name() == "CNY") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalCNYString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalCNYString());
    ui->balTransparent->setToolTip(balT.toDecimalCNYString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalCNYString()); 

    } else if (Settings::getInstance()->get_currency_name() == "RUB") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalRUBString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalRUBString());
    ui->balTransparent->setToolTip(balT.toDecimalRUBString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalRUBString()); 

    } else if (Settings::getInstance()->get_currency_name() == "CAD") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalCADString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalCADString());
    ui->balTransparent->setToolTip(balT.toDecimalCADString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalCADString()); 

    } else if (Settings::getInstance()->get_currency_name() == "SGD") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalSGDString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalSGDString());
    ui->balTransparent->setToolTip(balT.toDecimalSGDString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalSGDString()); 

    } else if (Settings::getInstance()->get_currency_name() == "CHF") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalCHFString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalCHFString());
    ui->balTransparent->setToolTip(balT.toDecimalCHFString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalCHFString()); 

    } else if (Settings::getInstance()->get_currency_name() == "INR") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalINRString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalINRString());
    ui->balTransparent->setToolTip(balT.toDecimalINRString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalINRString()); 

    } else if (Settings::getInstance()->get_currency_name() == "GBP") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalGBPString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalGBPString());
    ui->balTransparent->setToolTip(balT.toDecimalGBPString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalGBPString()); 

    } else if (Settings::getInstance()->get_currency_name() == "AUD") {
    ui->balSheilded   ->setToolTip(balZ.toDecimalAUDString());
    ui->balVerified   ->setToolTip(balVerified.toDecimalAUDString());
    ui->balTransparent->setToolTip(balT.toDecimalAUDString());
    ui->balTotal      ->setToolTip(balTotal.toDecimalAUDString()); 
    }
    // Send tab
    ui->txtAvailablehush->setText(balAvailable.toDecimalhushString());

    if (Settings::getInstance()->get_currency_name() == "USD") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalUSDString());
   } else if (Settings::getInstance()->get_currency_name() == "EUR") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalEURString()); 
    } else if (Settings::getInstance()->get_currency_name() == "BTC") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalBTCString()); 
     } else if (Settings::getInstance()->get_currency_name() == "CNY") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalCNYString()); 
    } else if (Settings::getInstance()->get_currency_name() == "RUB") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalRUBString()); 
     } else if (Settings::getInstance()->get_currency_name() == "CAD") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalCADString()); 
    } else if (Settings::getInstance()->get_currency_name() == "SGD") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalSGDString()); 
     } else if (Settings::getInstance()->get_currency_name() == "CHF") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalCHFString()); 
    } else if (Settings::getInstance()->get_currency_name() == "INR") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalINRString()); 
    } else if (Settings::getInstance()->get_currency_name() == "GBP") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalGBPString()); 
    } else if (Settings::getInstance()->get_currency_name() == "AUD") {
        ui->txtAvailableUSD->setText(balAvailable.toDecimalAUDString()); 
    }
}

void Controller::refreshBalances() {    
    if (!zrpc->haveConnection()) 
        return noConnection();

    // 1. Get the Balances
    zrpc->fetchBalance([=] (json reply) {    
        CAmount balT        = CAmount::fromqint64(reply["tbalance"].get<json::number_unsigned_t>());
        CAmount balZ        = CAmount::fromqint64(reply["zbalance"].get<json::number_unsigned_t>());
        CAmount balVerified = CAmount::fromqint64(reply["verified_zbalance"].get<json::number_unsigned_t>());
        
        model->setBalT(balT);
        model->setBalZ(balZ);
        model->setBalVerified(balVerified);

        // This is for the websockets
        AppDataModel::getInstance()->setBalances(balT, balZ);
        
        // This is for the datamodel
        CAmount balAvailable = balT + balVerified;
        model->setAvailableBalance(balAvailable);

        updateUIBalances();
    });

    // 2. Get the UTXOs
    // First, create a new UTXO list. It will be replacing the existing list when everything is processed.
    auto newUnspentOutputs = new QList<UnspentOutput>();
    auto newBalances = new QMap<QString, CAmount>();

    // Call the Transparent and Z unspent APIs serially and then, once they're done, update the UI
    zrpc->fetchUnspent([=] (json reply) {
        processUnspent(reply, newBalances, newUnspentOutputs);

        // Swap out the balances and UTXOs
        model->replaceBalances(newBalances);
        model->replaceUTXOs(newUnspentOutputs);

        // Find if any output is not spendable or is pending
        bool anyUnconfirmed = std::find_if(newUnspentOutputs->constBegin(), newUnspentOutputs->constEnd(), 
                                    [=](const UnspentOutput& u) -> bool { 
                                        return !u.spendable ||  u.pending; 
                              }) != newUnspentOutputs->constEnd();

        updateUI(anyUnconfirmed);

        main->balancesReady();
    });
}

void Controller::refreshTransactions() {    
    if (!zrpc->haveConnection()) 
        return noConnection();

    zrpc->fetchTransactions([=] (json reply) {
        QList<TransactionItem> txdata;        

        for (auto& it : reply.get<json::array_t>()) {  
            QString address;
            CAmount total_amount;
            QList<TransactionItemDetail> items;

            long confirmations;
            if (it.find("unconfirmed") != it.end() && it["unconfirmed"].get<json::boolean_t>()) {
                confirmations = 0;
            } else {
                confirmations = model->getLatestBlock() - it["block_height"].get<json::number_integer_t>() + 1;
            }
            
            auto txid = QString::fromStdString(it["txid"]);
            auto datetime = it["datetime"].get<json::number_integer_t>();

            // First, check if there's outgoing metadata
            if (!it["outgoing_metadata"].is_null()) {
            
                for (auto o: it["outgoing_metadata"].get<json::array_t>()) {
                    QString address = QString::fromStdString(o["address"]);
                    
                    // Sent items are -ve
                    CAmount amount = CAmount::fromqint64(-1 * o["value"].get<json::number_unsigned_t>()); 
                    
                   // Check for Memos
                   
                    QString memo;
                    if (!o["memo"].is_null()) {
                        memo = QString::fromStdString(o["memo"]);

                     }
                    
                    items.push_back(TransactionItemDetail{address, amount, memo});
                    total_amount = total_amount + amount;
                }

                {
                    // Concat all the addresses
                    QList<QString> addresses;
                    for (auto item : items) {
                        addresses.push_back(item.address);
                    }
                    address = addresses.join(",");
                }

                txdata.push_back(TransactionItem{
                   "send", datetime, address, txid,confirmations, items
                });
            } else {
                // Incoming Transaction
                address = (it["address"].is_null() ? "" : QString::fromStdString(it["address"]));
                model->markAddressUsed(address);

                QString memo;
                if (!it["memo"].is_null()) {
                    memo = QString::fromStdString(it["memo"]);
                }

                items.push_back(TransactionItemDetail{
                    address,
                    CAmount::fromqint64(it["amount"].get<json::number_integer_t>()),
                    memo
                });

  
                TransactionItem tx{
                    "Receive", datetime, address, txid,confirmations, items
                };

                txdata.push_back(tx);
            }
            
        }

        // Calculate the total unspent amount that's pending. This will need to be 
        // shown in the UI so the user can keep track of pending funds
        CAmount totalPending;
        for (auto txitem : txdata) {
            if (txitem.confirmations == 0) {
                for (auto item: txitem.items) {
                    totalPending = totalPending + item.amount;
                }
            }
        }
        getModel()->setTotalPending(totalPending);

        // Update UI Balance
        updateUIBalances();

        // Update model data, which updates the table view
        transactionsTableModel->replaceData(txdata);        
    });
}

// If the wallet is encrpyted and locked, we need to unlock it 
void Controller::unlockIfEncrypted(std::function<void(void)> cb, std::function<void(void)> error) {
    auto encStatus = getModel()->getEncryptionStatus();
    if (encStatus.first && encStatus.second) {
        // Wallet is encrypted and locked. Ask for the password and unlock.
        QString password = QInputDialog::getText(main, main->tr("Wallet Password"), 
                            main->tr("Your wallet is encrypted.\nPlease enter your wallet password"), QLineEdit::Password);

        if (password.isEmpty()) {
            QMessageBox::critical(main, main->tr("Wallet Decryption Failed"),
                main->tr("Please enter a valid password"),
                QMessageBox::Ok
            );
            error();
            return;
        }

        zrpc->unlockWallet(password, [=](json reply) {
            if (isJsonResultSuccess(reply)) {
                cb();

                // Refresh the wallet so the encryption status is now in sync.
                refresh(true);
            } else {
                QMessageBox::critical(main, main->tr("Wallet Decryption Failed"),
                    QString::fromStdString(reply["error"].get<json::string_t>()),
                    QMessageBox::Ok
                );
                error();
            }
        });
    } else {
        // Not locked, so just call the function
        cb();
    }
}

/**
 * Execute a transaction with the standard UI. i.e., standard status bar message and standard error
 * handling
 */
void Controller::executeStandardUITransaction(Tx tx) {
    executeTransaction(tx,
        [=] (QString txid) { 
            ui->statusBar->showMessage(Settings::txidStatusMessage + " " + txid);
        },
        [=] (QString opid, QString errStr) {
            ui->statusBar->showMessage(QObject::tr(" Tx ") % opid % QObject::tr(" failed"), 15 * 1000);

            if (!opid.isEmpty())
                errStr = QObject::tr("The transaction with id ") % opid % QObject::tr(" failed. The error was") + ":\n\n" + errStr; 

            QMessageBox::critical(main, QObject::tr("Transaction Error"), errStr, QMessageBox::Ok);            
        }
    );
}


// Execute a transaction!
void Controller::executeTransaction(Tx tx, 
        const std::function<void(QString txid)> submitted,
        const std::function<void(QString txid, QString errStr)> error) {
    unlockIfEncrypted([=] () {
        // First, create the json params
        json params = json::array();
        fillTxJsonParams(params, tx);
        std::cout << std::setw(2) << params << std::endl;

        zrpc->sendTransaction(QString::fromStdString(params.dump()), [=](const json& reply) {
            if (reply.find("txid") == reply.end()) {
                error("", "Couldn't understand Response: " + QString::fromStdString(reply.dump()));
            } else {
                QString txid = QString::fromStdString(reply["txid"].get<json::string_t>());
                submitted(txid);
            }
        },
        [=](QString errStr) {
            error("", errStr);
        });
    }, [=]() {
        error("", main->tr("Failed to unlock wallet"));
    });
}


void Controller::checkForUpdate(bool silent) {
    if (!zrpc->haveConnection()) 
        return noConnection();

    QUrl cmcURL("https://api.github.com/repos/DenioD/SilentDragonLite/releases");

    QNetworkRequest req;
    req.setUrl(cmcURL);
    
    QNetworkAccessManager *manager = new QNetworkAccessManager(this->main);
    QNetworkReply *reply = manager->get(req);

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        reply->deleteLater();
        manager->deleteLater();

        try {
            if (reply->error() == QNetworkReply::NoError) {

                auto releases = QJsonDocument::fromJson(reply->readAll()).array();
                QVersionNumber maxVersion(0, 0, 0);

                for (QJsonValue rel : releases) {
                    if (!rel.toObject().contains("tag_name"))
                        continue;

                    QString tag = rel.toObject()["tag_name"].toString();
                    if (tag.startsWith("v"))
                        tag = tag.right(tag.length() - 1);

                    if (!tag.isEmpty()) {
                        auto v = QVersionNumber::fromString(tag);
                        if (v > maxVersion)
                            maxVersion = v;
                    }
                }

                auto currentVersion = QVersionNumber::fromString(APP_VERSION);
                
                // Get the max version that the user has hidden updates for
                QSettings s;
                auto maxHiddenVersion = QVersionNumber::fromString(s.value("update/lastversion", "0.0.0").toString());

                qDebug() << "Version check: Current " << currentVersion << ", Available " << maxVersion;

                if (maxVersion > currentVersion && (!silent || maxVersion > maxHiddenVersion)) {
                    auto ans = QMessageBox::information(main, QObject::tr("Update Available"), 
                        QObject::tr("A new release v%1 is available! You have v%2.\n\nWould you like to visit the releases page?")
                            .arg(maxVersion.toString())
                            .arg(currentVersion.toString()),
                        QMessageBox::Yes, QMessageBox::Cancel);
                    if (ans == QMessageBox::Yes) {
                        QDesktopServices::openUrl(QUrl("https://github.com/DenioD/SilentDragonLite/releases"));
                    } else {
                        // If the user selects cancel, don't bother them again for this version
                        s.setValue("update/lastversion", maxVersion.toString());
                    }
                } else {
                    if (!silent) {
                        QMessageBox::information(main, QObject::tr("No updates available"), 
                            QObject::tr("You already have the latest release v%1")
                                .arg(currentVersion.toString()));
                    }
                } 
            }
        }
        catch (...) {
            // If anything at all goes wrong, just set the price to 0 and move on.
            qDebug() << QString("Caught something nasty");
        }       
    });
}

// Get the hush->USD price from coinmarketcap using their API
void Controller::refreshZECPrice() {
    if (!zrpc->haveConnection()) 
        return noConnection();

       // TODO: use/render all this data
    QUrl cmcURL("https://api.coingecko.com/api/v3/simple/price?ids=hush&vs_currencies=btc%2Cusd%2Ceur%2Ceth%2Cgbp%2Ccny%2Cjpy%2Crub%2Ccad%2Csgd%2Cchf%2Cinr%2Caud%2Cinr&include_market_cap=true&include_24hr_vol=true&include_24hr_change=true");
   
    QNetworkRequest req;
    req.setUrl(cmcURL);

    QNetworkAccessManager *manager = new QNetworkAccessManager(this->main);
    QNetworkReply *reply = manager->get(req);

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        reply->deleteLater();
        manager->deleteLater();

        try {
            if (reply->error() != QNetworkReply::NoError) {
                auto parsed = json::parse(reply->readAll(), nullptr, false);
                if (!parsed.is_discarded() && !parsed["error"]["message"].is_null()) {
                    qDebug() << QString::fromStdString(parsed["error"]["message"]);
                } else {
                    qDebug() << reply->errorString();
                }
                Settings::getInstance()->setZECPrice(0);
                Settings::getInstance()->setEURPrice(0);
                Settings::getInstance()->setBTCPrice(0);
                Settings::getInstance()->setCNYPrice(0);
                Settings::getInstance()->setRUBPrice(0);
                Settings::getInstance()->setCADPrice(0);
                Settings::getInstance()->setSGDPrice(0);
                Settings::getInstance()->setCHFPrice(0);
                Settings::getInstance()->setGBPPrice(0);
                Settings::getInstance()->setAUDPrice(0);
                Settings::getInstance()->setINRPrice(0);
               
                return;
            }

            qDebug() << "No network errors";

            auto all = reply->readAll();
            auto parsed = json::parse(all, nullptr, false);
            if (parsed.is_discarded()) {
                Settings::getInstance()->setZECPrice(0);
                Settings::getInstance()->setEURPrice(0);
                Settings::getInstance()->setBTCPrice(0);
                Settings::getInstance()->setCNYPrice(0);
                Settings::getInstance()->setRUBPrice(0);
                Settings::getInstance()->setCADPrice(0);
                Settings::getInstance()->setSGDPrice(0);
                Settings::getInstance()->setCHFPrice(0);
                Settings::getInstance()->setGBPPrice(0);
                Settings::getInstance()->setAUDPrice(0);
                Settings::getInstance()->setINRPrice(0);
                return;
            }

            qDebug() << "Parsed JSON";

            const json& item  = parsed.get<json::object_t>();
            const json& hush  = item["hush"].get<json::object_t>();

            if (hush["usd"] >= 0) {
                qDebug() << "Found hush key in price json";
                
                qDebug() << "HUSH = $" << QString::number((double)hush["usd"]);
                Settings::getInstance()->setZECPrice( hush["usd"] );
            }
            if (hush["eur"] >= 0)
            {
               
                qDebug() << "HUSH = €" << QString::number((double)hush["eur"]);
                Settings::getInstance()->setEURPrice(hush["eur"]);
            }
            if (hush["btc"] >= 0)
            {
            
                qDebug() << "HUSH = BTC" << QString::number((double)hush["btc"]);
                Settings::getInstance()->setBTCPrice( hush["btc"]);
            }
            if (hush["cny"] >= 0)
              
            {
                qDebug() << "HUSH = CNY" << QString::number((double)hush["cny"]);
                Settings::getInstance()->setCNYPrice( hush["cny"]);
            }
            if (hush["rub"] >= 0)
            {
                qDebug() << "HUSH = RUB" << QString::number((double)hush["rub"]);
                Settings::getInstance()->setRUBPrice( hush["rub"]);
            }
             if (hush["cad"] >= 0)
            
            {
                qDebug() << "HUSH = CAD" << QString::number((double)hush["cad"]);
                Settings::getInstance()->setCADPrice( hush["cad"]);
            }
            if (hush["sgd"] >= 0)
            {
              
                qDebug() << "HUSH = SGD" << QString::number((double)hush["sgd"]);
                Settings::getInstance()->setSGDPrice( hush["sgd"]);
            }
            if (hush["chf"] >= 0)
            {
              
                qDebug() << "HUSH = CHF" << QString::number((double)hush["chf"]);
                Settings::getInstance()->setCHFPrice( hush["chf"]);
            }
            if (hush["inr"] >= 0)
            {
              
                qDebug() << "HUSH = INR" << QString::number((double)hush["inr"]);
                Settings::getInstance()->setINRPrice( hush["inr"]);
            }
            if (hush["gbp"] >= 0)
            {
              
                qDebug() << "HUSH = GBP" << QString::number((double)hush["gbp"]);
                Settings::getInstance()->setGBPPrice( hush["gbp"]);
            }
            if (hush["aud"] >= 0)
            {
              
                qDebug() << "HUSH = AUD" << QString::number((double)hush["aud"]);
                Settings::getInstance()->setAUDPrice( hush["aud"]);
            }


            return;
         } catch (const std::exception& e) {
            // If anything at all goes wrong, just set the price to 0 and move on.
            qDebug() << QString("Caught something nasty: ") << e.what();
        }

        // If nothing, then set the price to 0;
                Settings::getInstance()->setZECPrice(0);
                Settings::getInstance()->setEURPrice(0);
                Settings::getInstance()->setBTCPrice(0);
                Settings::getInstance()->setCNYPrice(0);
                Settings::getInstance()->setRUBPrice(0);
                Settings::getInstance()->setCADPrice(0);
                Settings::getInstance()->setSGDPrice(0);
                Settings::getInstance()->setCHFPrice(0);
                Settings::getInstance()->setGBPPrice(0);
                Settings::getInstance()->setAUDPrice(0);
                Settings::getInstance()->setINRPrice(0);
    });

}

void Controller::shutdownhushd() {
    // Save the wallet and exit the lightclient library cleanly.
    if (zrpc->haveConnection()) {
        QDialog d(main);
        Ui_ConnectionDialog connD;
        connD.setupUi(&d);
        connD.topIcon->setBasePixmap(QIcon(":/icons/res/icon.ico").pixmap(256, 256));
        connD.status->setText(QObject::tr("Please wait for SilentDragonLite to exit"));
        connD.statusDetail->setText(QObject::tr("Waiting for hushd to exit"));

        bool finished = false;

        zrpc->saveWallet([&] (json) {        
            if (!finished)
                d.accept();
            finished = true;
        });

        if (!finished)
            d.exec();
    }
}

/** 
 * Get a Sapling address from the user's wallet
 */ 
QString Controller::getDefaultSaplingAddress() {
    for (QString addr: model->getAllZAddresses()) {
        if (Settings::getInstance()->isSaplingAddress(addr))
            return addr;
    }

    return QString();
}

QString Controller::getDefaultTAddress() {
    if (model->getAllTAddresses().length() > 0)
        return model->getAllTAddresses().at(0);
    else 
        return QString();
}
