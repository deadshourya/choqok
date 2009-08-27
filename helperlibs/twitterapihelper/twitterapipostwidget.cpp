/*
    This file is part of Choqok, the KDE micro-blogging client

    Copyright (C) 2008-2009 Mehrdad Momeny <mehrdad.momeny@gmail.com>

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

#include "twitterapipostwidget.h"
#include <microblog.h>
#include <klocalizedstring.h>
#include "twitterapiaccount.h"
#include <kicon.h>
#include <KPushButton>
#include "twitterapimicroblog.h"
#include <KDebug>
#include <mediamanager.h>
#include <choqokappearancesettings.h>

const QRegExp TwitterApiPostWidget::mUserRegExp("([\\s]|^)@([^\\s\\W]+)");
const QRegExp TwitterApiPostWidget::mHashtagRegExp("([\\s]|^)#([^\\s\\W]+)");
const KIcon TwitterApiPostWidget::unFavIcon(Choqok::MediaManager::convertToGrayScale(KIcon("rating").pixmap(16)) );

class TwitterApiPostWidget::Private
{
public:
    Private()
    {}
    KPushButton *btnFav;
    bool isBasePostShowed;
};

TwitterApiPostWidget::TwitterApiPostWidget(Choqok::Account* account, const Choqok::Post &post, QWidget* parent)
    : PostWidget(account, post, parent), d(new Private)
{
}

TwitterApiPostWidget::~TwitterApiPostWidget()
{
    delete d;
}

void TwitterApiPostWidget::initUi()
{
    Choqok::UI::PostWidget::initUi();
    if( !mCurrentPost.isPrivate ) {
        d->btnFav = addButton( "btnFavorite",i18nc( "@info:tooltip", "Favorite" ), "rating" );
        d->btnFav->setCheckable(true);
        connect( d->btnFav, SIGNAL(clicked(bool)), SLOT(setFavorite()) );
        updateFavStat();
    }
    if( mCurrentAccount->username().toLower() != mCurrentPost.author.userName.toLower()) {
        KPushButton *btnRe = addButton( "btnReply",i18nc( "@info:tooltip", "Reply" ), "edit-undo" );
        connect( btnRe, SIGNAL(clicked(bool)), SLOT(slotReply()) );
    }
}

QString TwitterApiPostWidget::prepareStatus(const QString& text)
{
    QString res = Choqok::UI::PostWidget::prepareStatus(text);
    res.replace(mUserRegExp,"\\1@<a href='user://\\2'>\\2</a> <a href='"+
    mCurrentAccount->microblog()->profileUrl( currentAccount(), "\\2") + "' title='" +
    mCurrentAccount->microblog()->profileUrl( currentAccount(), "\\2") + "'>"+ webIconText +"</a>");

    return res;
}

QString TwitterApiPostWidget::generateSign()
{
    QString sign;
    QString profUrl = mCurrentAccount->microblog()->profileUrl(currentAccount(), mCurrentPost.author.userName);
    sign = "<b><a href='user://"+mCurrentPost.author.userName+"' title=\"" +
    mCurrentPost.author.description + "\">" + mCurrentPost.author.userName +
    "</a> <a href=\"" + profUrl + "\" title=\"" + profUrl
    + "\">"+ webIconText +"</a> - </b>";
    //<img src=\"icon://web\" />
    sign += "<a href=\"" + mCurrentPost.link +
    "\" title=\"" + mCurrentPost.creationDateTime.toString() + "\">%1</a>";
    if ( mCurrentPost.isPrivate ) {
        if( mCurrentPost.replyToUserName.toLower() == mCurrentAccount->username().toLower() ) {
            sign.prepend( "From " );
        } else {
            sign.prepend( "To " );
        }
    } else {
        if( !mCurrentPost.source.isNull() )
            sign += " - " + mCurrentPost.source;
        if ( !mCurrentPost.replyToPostId.isEmpty() ) {
            QString link = mCurrentAccount->microblog()->postUrl( currentAccount(), mCurrentPost.replyToUserName,
                                                                  mCurrentPost.replyToPostId );
            sign += " - <a href='replyto://" + mCurrentPost.replyToPostId + "'>" +
            i18n("in reply to")+ "</a>&nbsp;<a href=\"" + link +  "\" title=\""+ link +"\">"+webIconText+"</a>";
        }
    }
    sign.prepend("<p dir='ltr'>");
    sign.append( "</p>" );
    return sign;
}

void TwitterApiPostWidget::slotReply()
{
    if(currentPost().isPrivate){
        TwitterApiAccount *account= qobject_cast<TwitterApiAccount*>( currentAccount() );
        TwitterApiMicroBlog *microblog = qobject_cast<TwitterApiMicroBlog*>( currentAccount()->microblog() );
        microblog->showDirectMessageDialog( account, mCurrentPost.author.userName );
    } else {
        emit reply( QString("@%1 ").arg(mCurrentPost.author.userName), mCurrentPost.postId );
    }
}

void TwitterApiPostWidget::setFavorite()
{
    TwitterApiMicroBlog *mic = qobject_cast<TwitterApiMicroBlog*>(mCurrentAccount->microblog());
    if(mCurrentPost.isFavorited){
        connect(mic, SIGNAL(favoriteRemoved(Choqok::Account*,QString)),
                this, SLOT(slotSetFavorite(Choqok::Account*,QString)) );
        mic->removeFavorite(currentAccount(), mCurrentPost.postId);
    } else {
        connect(mic, SIGNAL(favoriteRemoved(Choqok::Account*,QString)),
                this, SLOT(slotSetFavorite(Choqok::Account*,QString)) );
        mic->createFavorite(currentAccount(), mCurrentPost.postId);
    }
}

void TwitterApiPostWidget::slotSetFavorite(Choqok::Account *theAccount, const QString& postId)
{
    if(currentAccount() == theAccount && postId == mCurrentPost.postId){
        kDebug()<<postId;
        mCurrentPost.isFavorited = !mCurrentPost.isFavorited;
        updateFavStat();
        TwitterApiMicroBlog *mic = qobject_cast<TwitterApiMicroBlog*>(mCurrentAccount->microblog());
        disconnect(mic, SIGNAL(favoriteRemoved(Choqok::Account*,QString)),
                   this, SLOT(slotSetFavorite(Choqok::Account*,QString)) );
        disconnect(mic, SIGNAL(favoriteCreated(Choqok::Account*,QString)),
                   this, SLOT(slotSetFavorite(Choqok::Account*,QString)) );
        ///TODO Notify!
    }
}

void TwitterApiPostWidget::updateFavStat()
{
    if(mCurrentPost.isFavorited){
        d->btnFav->setChecked(true);
        d->btnFav->setIcon(KIcon("rating"));
    } else {
        d->btnFav->setChecked(false);
        d->btnFav->setIcon(unFavIcon);
    }
}

void TwitterApiPostWidget::checkAnchor(const QUrl & url)
{
    QString scheme = url.scheme();
    if(scheme == "user") {
        kDebug()<<"NOT IMPLEMENTED YET";
    } else if( scheme == "replyto" ) {
        if(d->isBasePostShowed) {
            mContent = prepareStatus(currentPost().content);
            updateUi();
            d->isBasePostShowed = false;
            return;
        } else {
            connect(currentAccount()->microblog(), SIGNAL(postFetched(Choqok::Account*,Choqok::Post*)),
                    this, SLOT(slotBasePostFetched(Choqok::Account*,Choqok::Post*)) );
            Choqok::Post *ps = new Choqok::Post;
            ps->postId = url.host();
            currentAccount()->microblog()->fetchPost(currentAccount(), ps);
        }
    } else if (scheme == "thread") {
        kDebug()<<"NOT IMPLEMENTED YET";
    } else {
        Choqok::UI::PostWidget::checkAnchor(url);
    }

}

void TwitterApiPostWidget::slotBasePostFetched(Choqok::Account* theAccount, Choqok::Post* post)
{
    if(theAccount == currentAccount()){
        kDebug();
        disconnect( currentAccount()->microblog(), SIGNAL(postFetched(Choqok::Account*,Choqok::Post*)),
                   this, SLOT(slotBasePostFetched(Choqok::Account*,Choqok::Post*)) );
        if(d->isBasePostShowed)
            return;
        d->isBasePostShowed = true;
        QString color;
        if( Choqok::AppearanceSettings::isCustomUi() ) {
            color = Choqok::AppearanceSettings::readForeColor().lighter().name();
        } else {
            color = this->palette().dark().color().name();
        }
        QString baseStatusText = "<p style=\"margin-top:10px; margin-bottom:10px; margin-left:20px;\
        margin-right:20px; -qt-block-indent:0; text-indent:0px\"><span style=\" color:" + color + ";\">";
        baseStatusText += "<b><a href='user://"+ post->author.userName +"'>" +
        post->author.userName + "</a> :</b> ";

        baseStatusText += prepareStatus( post->content ) + "</p>";
        mContent.prepend( baseStatusText );
        updateUi();
        delete post;
    }
}

#include "twitterapipostwidget.moc"
