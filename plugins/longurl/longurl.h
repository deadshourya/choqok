/*
    This file is part of Choqok, the KDE micro-blogging client

    Copyright (C) 2014 Eugene Shalygin <eugene.shalygin@gmail.com>

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

#ifndef CHOQOK_LONGURL_H
#define CHOQOK_LONGURL_H

#include <QPointer>
#include <QQueue>
#include <QSharedPointer>
#include <QUrlQuery>

#include <KIO/Job>

#include "plugin.h"

class QUrl;

namespace Choqok
{
namespace UI
{
class PostWidget;
}
}

class LongUrl : public Choqok::Plugin
{
    typedef Choqok::Plugin base;
    Q_OBJECT
public:
    LongUrl(QObject *parent, const QList< QVariant > &args);
    ~LongUrl();

protected Q_SLOTS:
    void slotAddNewPostWidget(Choqok::UI::PostWidget *newWidget);
    void startParsing();
    void dataReceived(KIO::Job *job, QByteArray data);
    void jobResult(KJob *job);
    virtual void aboutToUnload() override;
    void servicesDataReceived(KIO::Job *job, QByteArray data);
    void servicesJobResult(KJob *job);
private:
    enum ParserState { Running = 0, Stopped };
    ParserState state;

    typedef QPointer<Choqok::UI::PostWidget> PostWidgetPointer;

    void sheduleSupportedServicesFetch();
    bool isServiceSupported(const QString &host);
    void processJobResults(KJob *job);

    void parse(PostWidgetPointer postToParse);
    KJob *sheduleParsing(const QString &shortUrl);
    void suspendJobs();

    void replaceUrl(PostWidgetPointer post, const QUrl &fromUrl, const QUrl &toUrl);

    PostWidgetPointer takeJob(KJob *job)
    {
        return mParsingList.take(job);
    }

    void insertJob(KJob *job, PostWidgetPointer post)
    {
        mParsingList.insert(job, post);
    }

    QQueue< PostWidgetPointer > postsQueue;
    QMap<KJob *, PostWidgetPointer > mParsingList;
    QStringList supportedServices;
    typedef QMap<KJob *, QByteArray> DataMap;
    DataMap mData;
    typedef QMap<KJob *, QString> UrlsMap;
    UrlsMap mShortUrls;
    QSharedPointer<QByteArray> mServicesData;
    bool mServicesAreFetched;
};

#endif //CHOQOK_LONGURL_H
