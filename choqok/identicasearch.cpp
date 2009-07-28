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

#include "identicasearch.h"

#include <KDE/KLocale>
#include <QDomDocument>
#include <kio/jobclasses.h>
#include <kio/job.h>
#include <kurl.h>
#include <kio/netaccess.h>
#include <kdebug.h>

#include "backend.h"

IdenticaSearch::IdenticaSearch( Account* account, const QString & searchUrl, QObject *parent ) :
    Search(account, "tag:search.twitter.com,[0-9]+:([0-9]+)", searchUrl, parent)
{
    kDebug();
    mIdRegExp.setPattern("(?:user|(?:.*notice))/([0-9]+)");
    mSearchTypes[ToUser].first = i18nc( "Dents are Identica posts", "Dents To This User" );
    mSearchTypes[ToUser].second = false;

    mSearchTypes[FromUser].first = i18nc( "Dents are Identica posts", "Dents From This User" );
    mSearchTypes[FromUser].second = false;

    mSearchTypes[ReferenceGroup].first = i18nc( "Dents are Identica posts", "Dents Including This Group" );
    mSearchTypes[ReferenceGroup].second = false;

    mSearchTypes[ReferenceHashtag].first = i18nc( "Dents are Identica posts", "Dents Including This Hashtag" );
    mSearchTypes[ReferenceHashtag].second = true;
}

IdenticaSearch::~IdenticaSearch()
{
    kDebug();
}

KUrl IdenticaSearch::buildUrl( QString query, int option, qulonglong sinceStatusId, qulonglong count, qulonglong page )
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
        url.setUrl( mSearchUrl );
        url.addPath("/api/search.atom");
        url.addQueryItem("q", formattedQuery);
        if( sinceStatusId )
            url.addQueryItem( "since_id", QString::number( sinceStatusId ) );
        if( count && count <= 100 )
            url.addQueryItem( "rpp", QString::number( count ) );
        url.addQueryItem( "page", QString::number( page ) );
    } else {
        url.setUrl( mSearchUrl + formattedQuery );
    }
    return url;
}

void IdenticaSearch::requestSearchResults( QString query, int option, qulonglong sinceStatusId, qulonglong count, qulonglong page )
{
    kDebug();
    Q_UNUSED(count);
    Q_UNUSED(page);

    KUrl url = buildUrl( query, option, sinceStatusId, count, page );

    KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::Reload, KIO::HideProgressInfo );
    if( !job ) {
        kDebug() << "Cannot create a http GET request!";
        emit error( i18n( "Unable to fetch search results." ) );
        return;
    }

    mSinceStatusId = sinceStatusId;
    mLatestSearch = option;

    connect( job, SIGNAL( result( KJob* ) ), this, SLOT( searchResultsReturned( KJob* ) ) );
    job->start();
}

void IdenticaSearch::searchResultsReturned( KJob* job )
{
    kDebug();
    if( job == 0 ) {
        kDebug() << "job is a null pointer";
        emit error( i18n( "Unable to fetch search results." ) );
        return;
    }

    if( job->error() ) {
        kError() << "Error: " << job->errorString();
        emit error( i18n( "Unable to fetch search results: %1", job->errorString() ) );
        return;
    }
    KIO::StoredTransferJob *jj = qobject_cast<KIO::StoredTransferJob *>( job );
    QList<Status>* statusList;
    if( mLatestSearch == ReferenceHashtag )
        statusList = parseAtom( jj->data() );
    else
        statusList = parseRss( jj->data() );

    emit searchResultsReceived( *statusList );
}

QList<Status>* IdenticaSearch::parseRss( const QByteArray &buffer )
{
    kDebug();
    QDomDocument document;
    QList<Status> *statusList = new QList<Status>;

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

        Status status;

        QDomAttr statusIdAttr = node.toElement().attributeNode( "rdf:about" );
        qulonglong statusId = 0;
	if(mIdRegExp.exactMatch(statusIdAttr.value())) {
	  statusId = mIdRegExp.cap(1).toULongLong();
	}
//         sscanf( qPrintable( statusIdAttr.value() ),
//                 qPrintable( mSearchUrl + "notice/%d" ), &statusId );

        if( statusId <= mSinceStatusId )
        {
            node = node.nextSibling();
            continue;
        }

        status.statusId = statusId;

        QDomNode itemNode = node.firstChild();

        while( !itemNode.isNull() )
        {
            if( itemNode.toElement().tagName() == "title" )
            {
                QString content = itemNode.toElement().text();

                int nameSep = content.indexOf( ':', 0 );
                QString screenName = content.left( nameSep );
                QString statusText = content.right( content.size() - nameSep - 2 );

                status.user.screenName = screenName;
                status.content = statusText;
            } else if ( itemNode.toElement().tagName() == "dc:date" ) {
                int year, month, day, hour, minute, second;
                sscanf( qPrintable( itemNode.toElement().text() ),
                        "%d-%d-%dT%d:%d:%d%*s", &year, &month, &day, &hour, &minute, &second);
                QDateTime recognized( QDate( year, month, day), QTime( hour, minute, second ) );
                recognized.setTimeSpec( Qt::UTC );
                status.creationDateTime = recognized;
            } else if ( itemNode.toElement().tagName() == "dc:creator" ) {
                status.user.name = itemNode.toElement().text();
            } else if ( itemNode.toElement().tagName() == "sioc:has_creator" ) {
                QDomAttr userIdAttr = itemNode.toElement().attributeNode( "rdf:resource" );
                qulonglong id = 0;
		if(mIdRegExp.exactMatch(userIdAttr.value())) {
		  id = mIdRegExp.cap(1).toULongLong();
		}
/*                sscanf( qPrintable( userIdAttr.value() ),
                        qPrintable( mSearchUrl + "user/%d" ), &id );*/
                status.user.userId = id;
            } else if ( itemNode.toElement().tagName() == "laconica:postIcon" ) {
                QDomAttr imageAttr = itemNode.toElement().attributeNode( "rdf:resource" );
                status.user.profileImageUrl = imageAttr.value();
            }

            itemNode = itemNode.nextSibling();
        }

        status.isDMessage = false;
        status.isFavorited = false;
        status.isTruncated = false;
        status.replyToStatusId = 0;
        statusList->insert( 0, status );
        node = node.nextSibling();
    }

    return statusList;
}
