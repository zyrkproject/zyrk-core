// Copyright (c) 2017 The Particl Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mnemonicdialog.h>
#include <qt/forms/ui_mnemonicdialog.h>

#include <qt/guiutil.h>

#include <qt/walletmodel.h>
#include <wallet/wallet.h>

#include <util.h>
#include <univalue.h>
#include <stealth/mnemonic.h>

#include <QDebug>

MnemonicDialog::MnemonicDialog(QWidget *parent, WalletModel *wm) :
    QDialog(parent), walletModel(wm),
    ui(new Ui::MnemonicDialog)
{
    ui->setupUi(this);
    CWallet *phdw = wm->getWallet();
    if (!phdw)
        return;

    if (phdw->GetHDChain().masterKeyID.IsNull())
        ui->lblHelp->setText(
            "This wallet has no HD account loaded.\n"
            "An account must first be loaded in order to generate receiving addresses.\n"
            "Importing a recovery phrase will load a new master key and account.\n"
            "You can generate a new recovery phrase from the 'Create' page below.\n");
    else
        ui->lblHelp->setText(
            "This wallet already has an HD account loaded.\n"
            "By importing another recovery phrase a new account will be created and set as the default.\n"
            "The wallet will receive on addresses from the new and existing account/s.\n"
            "New addresses will be generated from the new account.\n");

    ui->cbxLanguage->clear();
    for (int l = 1; l < WLL_MAX; ++l)
        ui->cbxLanguage->addItem(mnLanguagesDesc[l], QString(mnLanguagesTag[l]));
}

MnemonicDialog::~MnemonicDialog()
{

}

void MnemonicDialog::on_btnCancel_clicked()
{
    close();
}

void MnemonicDialog::on_btnCancel2_clicked()
{
    close();
}

void MnemonicDialog::on_btnImport_clicked()
{
    QString sCommand = (ui->chkImportChain->checkState() == Qt::Unchecked)
        ? "extkeyimportmaster" : "extkeygenesisimport";
    sCommand += " \"" + ui->tbxMnemonic->toPlainText() + "\"";

    QString sPassword = ui->edtPassword->text();
    if (!sPassword.isEmpty())
        sCommand += " \"" + sPassword + "\"";

    UniValue rv;
    if (walletModel->tryCallRpc(sCommand, rv))
    {
        close();
        if (!rv["warnings"].isNull())
        {
            //for (size_t i = 0; i < rv["warnings"].size(); ++i)
                //walletModel->warningBox(tr("Import"), QString::fromStdString(rv["warnings"][i].get_str()));
        }
    }
}

void MnemonicDialog::on_btnGenerate_clicked()
{
    int nBytesEntropy = ui->spinEntropy->value();
    QString sLanguage = ui->cbxLanguage->itemData(ui->cbxLanguage->currentIndex()).toString();

    QString sCommand = "mnemonic new  \"\" " + sLanguage + " " + QString::number(nBytesEntropy);

    UniValue rv;
    if (walletModel->tryCallRpc(sCommand, rv))
    {
        ui->tbxMnemonicOut->setText(QString::fromStdString(rv["mnemonic"].get_str()));
    }
}
