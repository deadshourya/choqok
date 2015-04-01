/*
    This file is part of Choqok, the KDE micro-blogging client

    Copyright (C) 2013-2014 Andrea Scarpino <scarpino@kde.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License or (at your option) version 3 or any later version
    accepted by the membership of KDE e.V. (or its successor approved
    by the membership of KDE e.V.), which shall act as a proxy
    defined in Section 14 of version 3 of the license.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see http://www.gnu.org/licenses/
*/

#include "pumpiomessagedialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QPointer>
#include <QPushButton>

#include "pumpioaccount.h"
#include "pumpiodebug.h"
#include "pumpiomicroblog.h"
#include "pumpiopost.h"

class PumpIOMessageDialog::Private
{
public:
    Choqok::Account *account;
    QString mediumToAttach;
    QPointer<QLabel> mediumName;
    QPointer<QPushButton> btnCancel;
};

PumpIOMessageDialog::PumpIOMessageDialog(Choqok::Account *theAccount, QWidget *parent,
        Qt::WindowFlags flags)
    : QDialog(parent, flags)
    , d(new Private)
{
    d->account = theAccount;

    setupUi(this);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    verticalLayout->addWidget(buttonBox);

    PumpIOAccount *acc = qobject_cast<PumpIOAccount *>(theAccount);
    if (acc) {
        Q_FOREACH (const QVariant &list, acc->lists()) {
            QVariantMap l = list.toMap();
            QListWidgetItem *item = new QListWidgetItem;
            item->setText(l.value("name").toString());
            item->setData(Qt::UserRole, l.value("id").toString());
            toList->addItem(item);
            ccList->addItem(item->clone());
        }
        //Lists are not sorted
        toList->sortItems();
        ccList->sortItems();

        Q_FOREACH (const QString &username, acc->following()) {
            QListWidgetItem *item = new QListWidgetItem;
            item->setText(PumpIOMicroBlog::userNameFromAcct(username));
            item->setData(Qt::UserRole, username);
            toList->addItem(item);
            ccList->addItem(item->clone());
        }
    }

    connect(btnReload, SIGNAL(clicked(bool)), this, SLOT(fetchFollowing()));
    connect(btnAttach, SIGNAL(clicked(bool)), this, SLOT(attachMedia()));
}

PumpIOMessageDialog::~PumpIOMessageDialog()
{
    delete d;
}

void PumpIOMessageDialog::fetchFollowing()
{
    qCDebug(CHOQOK);
    toList->clear();
    ccList->clear();
    PumpIOMicroBlog *microblog = qobject_cast<PumpIOMicroBlog *>(d->account->microblog());
    if (microblog) {
        microblog->fetchFollowing(d->account);
        connect(microblog, SIGNAL(followingFetched(Choqok::Account*)), this,
                SLOT(slotFetchFollowing(Choqok::Account*)));
    }
}

void PumpIOMessageDialog::slotFetchFollowing(Choqok::Account *theAccount)
{
    qCDebug(CHOQOK);
    if (theAccount == d->account) {
        PumpIOAccount *acc = qobject_cast<PumpIOAccount *>(theAccount);
        if (acc) {
            Q_FOREACH (const QVariant &list, acc->lists()) {
                QVariantMap l = list.toMap();
                QListWidgetItem *item = new QListWidgetItem;
                item->setText(l.value("name").toString());
                item->setData(Qt::UserRole, l.value("id").toString());
                toList->addItem(item);
                ccList->addItem(item->clone());
            }
            toList->sortItems();
            ccList->sortItems();

            Q_FOREACH (const QString &username, acc->following()) {
                QListWidgetItem *item = new QListWidgetItem;
                item->setText(PumpIOMicroBlog::userNameFromAcct(username));
                item->setData(Qt::UserRole, username);
                toList->addItem(item);
                ccList->addItem(item->clone());
            }
        }
    }
}

void PumpIOMessageDialog::accept()
{
    qCDebug(CHOQOK);
    PumpIOAccount *acc = qobject_cast<PumpIOAccount *>(d->account);
    if (acc) {
        if (acc->following().isEmpty() || txtMessage->toPlainText().isEmpty()
                || (toList->selectedItems().isEmpty() && ccList->selectedItems().isEmpty())) {
            return;
        }
        hide();
        PumpIOMicroBlog *microblog = qobject_cast<PumpIOMicroBlog *>(d->account->microblog());
        if (microblog) {
            PumpIOPost *post = new PumpIOPost;
            post->content = txtMessage->toPlainText();

            QVariantList to;
            Q_FOREACH (QListWidgetItem *item, toList->selectedItems()) {
                QVariantMap user;
                QString id = item->data(Qt::UserRole).toString();
                if (id.contains("acct:")) {
                    user.insert("objectType", "person");
                } else {
                    user.insert("objectType", "collection");
                }
                user.insert("id", id);
                to.append(user);
            }

            QVariantList cc;
            Q_FOREACH (QListWidgetItem *item, ccList->selectedItems()) {
                QVariantMap user;
                QString id = item->data(Qt::UserRole).toString();
                if (id.contains("acct:")) {
                    user.insert("objectType", "person");
                } else {
                    user.insert("objectType", "collection");
                }
                user.insert("id", id);
                cc.append(user);
            }

            microblog->createPost(acc, post, to, cc);
        }
    }
}

void PumpIOMessageDialog::attachMedia()
{
    qCDebug(CHOQOK);
    d->mediumToAttach = QFileDialog::getOpenFileName(this, i18n("Select Media to Upload"),
                                                     QString(), QStringLiteral("Images"));
    if (d->mediumToAttach.isEmpty()) {
        qCDebug(CHOQOK) << "No file selected";
        return;
    }
    const QString fileName = QUrl(d->mediumToAttach).fileName();
    if (!d->mediumName) {
        d->mediumName = new QLabel(this);
        d->btnCancel = new QPushButton(this);
        d->btnCancel->setIcon(QIcon::fromTheme("list-remove"));
        d->btnCancel->setToolTip(i18n("Discard Attachment"));
        d->btnCancel->setMaximumWidth(d->btnCancel->height());
        connect(d->btnCancel, SIGNAL(clicked(bool)), SLOT(cancelAttach()));

        horizontalLayout->insertWidget(1, d->mediumName);
        horizontalLayout->insertWidget(2, d->btnCancel);
    }
    d->mediumName->setText(i18n("Attaching <b>%1</b>", fileName));
    txtMessage->setFocus();
}

void PumpIOMessageDialog::cancelAttach()
{
    qCDebug(CHOQOK);
    delete d->mediumName;
    d->mediumName = 0;
    delete d->btnCancel;
    d->btnCancel = 0;
    d->mediumToAttach.clear();
}
