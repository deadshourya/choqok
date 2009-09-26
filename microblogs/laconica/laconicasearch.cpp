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

#include "laconicasearch.h"
#include <KDebug>
#include <klocalizedstring.h>
#include <twitterapihelper/twitterapiaccount.h>
#include <kio/jobclasses.h>
#include <kio/job.h>
#include <QDomElement>

const QRegExp LaconicaSearch::mIdRegExp("(?:user|(?:.*notice))/([0-9]+)");

LaconicaSearch::LaconicaSearch(QObject* parent): TwitterApiSearch(parent)
{
    kDebug();
    mSearchCode[ReferenceGroup] = '!';
    mSearchCode[ToUser] = '@';
    mSearchCode[FromUser] = "from:";
    mSearchCode[ReferenceHashtag] = '#';

    mSearchTypes[ReferenceHashtag].first = i18nc( "Dents are Identica posts", "Dents Including This Hashtag" );
    mSearchTypes[ReferenceHashtag].second = true;

    mSearchTypes[ReferenceGroup].first = i18nc( "Dents are Identica posts", "Dents Including This Group" );
    mSearchTypes[ReferenceGroup].second = false;

    mSearchTypes[FromUser].first = i18nc( "Dents are Identica posts", "Dents From This User" );
    mSearchTypes[FromUser].second = false;

    mSearchTypes[ToUser].first = i18nc( "Dents are Identica posts", "Dents To This User" );
    mSearchTypes[ToUser].second = false;


}

LaconicaSearch::~LaconicaSearch()
{

}

KUrl LaconicaSearch::buildUrl(TwitterApiAccount* theAccount, QString query, int option,
                              Choqok::ChoqokId sinceStatusId, uint count, uint page)
{
    kDebug();

    QString formattedQuery;
    switch ( option ) {
        case ToUser:
            formattedQuery = query + "/replies/rss";
            break;
        case FromUser:
            formattedQuery = query + "/rss";
            break;
        case ReferenceGroup:
            formattedQuery = "group/" + query + "/rss";
            break;
        case ReferenceHashtag:
            formattedQuery = "#" + query;
            break;
        default:
            formattedQuery = query + "/rss";
            break;
    };

    KUrl url;
    if( option == ReferenceHashtag ) {
        url = theAccount->apiUrl();
        url.addPath("/api/search.atom");
        url.addQueryItem("q", formattedQuery);
        if( !sinceStatusId.isEmpty() )
            url.addQueryItem( "since_id", sinceStatusId );
        if( count && count <= 100 )
            url.addQueryItem( "rpp", QString::number( count ) );
        url.addQueryItem( "page", QString::number( page ) );
    } else {
        url = theAccount->homepageUrl();
        url.addPath( formattedQuery );
    }
    return url;
}

void LaconicaSearch::requestSearchResults(TwitterApiAccount* theAccount, const QString& query,
                                          int option, const Choqok::ChoqokId& sinceStatusId,
                                          uint count, uint page)
{
    kDebug();

    KUrl url = buildUrl( theAccount, query, option, sinceStatusId, count, page );

    KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::Reload, KIO::HideProgressInfo );
    if( !job ) {
        kError() << "Cannot create an http GET request!";
        return;
    }
    AccountQueryOptionContainer m;
    m.option = option;
    m.query = query;
    m.account = theAccount;
    mSearchJobs[job] = m;
    connect( job, SIGNAL( result( KJob* ) ), this, SLOT( searchResultsReturned( KJob* ) ) );
    job->start();
}

void LaconicaSearch::searchResultsReturned(KJob* job)
{
    kDebug();
    if( job == 0 ) {
        kDebug() << "job is a null pointer";
        emit error( i18n( "Unable to fetch search results." ) );
        return;
    }

    AccountQueryOptionContainer m = mSearchJobs.take(job);

    if( job->error() ) {
        kError() << "Error: " << job->errorString();
        emit error( i18n( "Unable to fetch search results: %1", job->errorString() ) );
        return;
    }
    KIO::StoredTransferJob *jj = qobject_cast<KIO::StoredTransferJob *>( job );
    QList<Choqok::Post*> postsList;
    if(m.option == ReferenceHashtag)
        postsList = parseAtom( jj->data() );
    else
        postsList = parseRss( jj->data() );


    emit searchResultsReceived( m.account, m.query, m.option, postsList );
}

QString LaconicaSearch::optionCode(int option)
{
    return mSearchCode[option];
}

QList< Choqok::Post* > LaconicaSearch::parseAtom(const QByteArray& buffer)
{
    kDebug();
    QDomDocument document;
    QList<Choqok::Post*> statusList;

    document.setContent( buffer );

    QDomElement root = document.documentElement();

    if ( root.tagName() != "feed" ) {
        kDebug() << "There is no feed element in Atom feed " << buffer.data();
        return statusList;
    }

    QDomNode node = root.firstChild();
    QString timeStr;
    while ( !node.isNull() ) {
        if ( node.toElement().tagName() != "entry" ) {
            node = node.nextSibling();
            continue;
        }

        QDomNode entryNode = node.firstChild();
        Choqok::Post *status = new Choqok::Post;
        status->isPrivate = false;

        while ( !entryNode.isNull() ) {
            QDomElement elm = entryNode.toElement();
            if ( elm.tagName() == "id" ) {
                // Fomatting example: "tag:search.twitter.com,2005:1235016836"
                Choqok::ChoqokId id;
                if(m_rId.exactMatch(elm.text())) {
                    id = m_rId.cap(1);
                }
                /*                sscanf( qPrintable( elm.text() ),
                "tag:search.twitter.com,%*d:%d", &id);*/
                status->postId = id;
            } else if ( elm.tagName() == "published" ) {
                // Formatting example: "2009-02-21T19:42:39Z"
                // Need to extract date in similar fashion to dateFromString
                int year, month, day, hour, minute, second;
                sscanf( qPrintable( elm.text() ),
                        "%d-%d-%dT%d:%d:%d%*s", &year, &month, &day, &hour, &minute, &second);
                        QDateTime recognized( QDate( year, month, day), QTime( hour, minute, second ) );
                        recognized.setTimeSpec( Qt::UTC );
                        status->creationDateTime = recognized;
            } else if ( elm.tagName() == "title" ) {
                status->content = elm.text();
            } else if ( elm.tagName() == "link" &&
                elm.attributeNode( "rel" ).value() == "related") {
                QDomAttr imageAttr = elm.attributeNode( "href" );
                status->author.profileImageUrl = imageAttr.value();
            } else if ( elm.tagName() == "author") {
                QDomNode userNode = entryNode.firstChild();
                while ( !userNode.isNull() )
                {
                    if ( userNode.toElement().tagName() == "name" )
                    {
                        QString fullName = userNode.toElement().text();
                        int bracketPos = fullName.indexOf( " ", 0 );
                        QString screenName = fullName.left( bracketPos );
                        QString name = fullName.right ( fullName.size() - bracketPos - 2 );
                        name.chop( 1 );
                        status->author.realName = name;
                        status->author.userName = screenName;
                    }
                    userNode = userNode.nextSibling();
                }
            }
            entryNode = entryNode.nextSibling();
        }
        status->isFavorited = false;
        statusList.insert( 0, status );
        node = node.nextSibling();
    }

    return statusList;
}

QList< Choqok::Post* > LaconicaSearch::parseRss(const QByteArray& buffer)
{
    kDebug();
    QDomDocument document;
    QList<Choqok::Post*> statusList;

    document.setContent( buffer );

    QDomElement root = document.documentElement();

    if ( root.tagName() != "rdf:RDF" ) {
        kDebug() << "There is no rdf:RDF element in RSS feed " << buffer.data();
        return statusList;
    }

    QDomNode node = root.firstChild();
    QString timeStr;
    while ( !node.isNull() ) {
        if ( node.toElement().tagName() != "item" ) {
            node = node.nextSibling();
            continue;
        }

        Choqok::Post *status = new Choqok::Post;

        QDomAttr statusIdAttr = node.toElement().attributeNode( "rdf:about" );
        Choqok::ChoqokId statusId;
    if(mIdRegExp.exactMatch(statusIdAttr.value())) {
      statusId = mIdRegExp.cap(1);
    }

//         if( statusId <= mSinceStatusId )
//         {
//             node = node.nextSibling();
//             continue;
//         }

        status->postId = statusId;

        QDomNode itemNode = node.firstChild();

        while( !itemNode.isNull() )
        {
            if( itemNode.toElement().tagName() == "title" )
            {
                QString content = itemNode.toElement().text();

                int nameSep = content.indexOf( ':', 0 );
                QString screenName = content.left( nameSep );
                QString statusText = content.right( content.size() - nameSep - 2 );

                status->author.userName = screenName;
                status->content = statusText;
            } else if ( itemNode.toElement().tagName() == "dc:date" ) {
                int year, month, day, hour, minute, second;
                sscanf( qPrintable( itemNode.toElement().text() ),
                        "%d-%d-%dT%d:%d:%d%*s", &year, &month, &day, &hour, &minute, &second);
                QDateTime recognized( QDate( year, month, day), QTime( hour, minute, second ) );
                recognized.setTimeSpec( Qt::UTC );
                status->creationDateTime = recognized;
            } else if ( itemNode.toElement().tagName() == "dc:creator" ) {
                status->author.realName = itemNode.toElement().text();
            } else if ( itemNode.toElement().tagName() == "sioc:has_creator" ) {
                QDomAttr userIdAttr = itemNode.toElement().attributeNode( "rdf:resource" );
                Choqok::ChoqokId id;
        if(mIdRegExp.exactMatch(userIdAttr.value())) {
          id = mIdRegExp.cap(1);
        }
/*                sscanf( qPrintable( userIdAttr.value() ),
                        qPrintable( mSearchUrl + "user/%d" ), &id );*/
                status->author.userId = id;
            } else if ( itemNode.toElement().tagName() == "laconica:postIcon" ) {
                QDomAttr imageAttr = itemNode.toElement().attributeNode( "rdf:resource" );
                status->author.profileImageUrl = imageAttr.value();
            }

            itemNode = itemNode.nextSibling();
        }

        status->isPrivate = false;
        status->isFavorited = false;
        statusList.insert( 0, status );
        node = node.nextSibling();
    }

    return statusList;
}


#include "laconicasearch.moc"
