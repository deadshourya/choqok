/*
    This file is part of Choqok, the KDE micro-blogging client

    Copyright (C) 2008-2010 Mehrdad Momeny <mehrdad.momeny@gmail.com>

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

#include "mainwindow.h"
#include "choqokbehaviorsettings.h"
#include "choqokapplication.h"
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <KDE/KLocale>

static const char description[] =
    I18N_NOOP( "KDE Micro-Blogging Client." );

static const char version[] = "0.9.82";

int main( int argc, char **argv )
{
    qDebug()<<"Choqok "<<version;
    KAboutData about( "choqok", 0, ki18n( "Choqok" ), version, ki18n( description ),
                      KAboutData::License_GPL_V3, ki18n( "(C) 2008-2010 Mehrdad Momeny" ),
                      KLocalizedString(), 0  );
    about.addAuthor( ki18n( "Mehrdad Momeny" ), ki18n( "Author and Core Developer" ),
                     "mehrdad.momeny@gmail.com", "http://identi.ca/mtux" );
    about.addCredit( ki18n( "Roozbeh Shafiee" ), ki18n( "Graphic designer" ), "roozbeh@roozbehonline.com" );
    about.addCredit( ki18n( "Daniel Schaal" ), ki18n( "UI improvements" ), "daniel@foto-schaal.de");
    about.addCredit( ki18n( "Stephen Henderson" ), ki18n( "Search API implementation" ), "hendersonsk@gmail.com");
    about.addCredit( ki18n( "Tejas Dinkar" ), ki18n( "Developer" ),
                     "tejasdinkar@gmail.com", "http://twitter.com/tdinkar" );
    about.addCredit( ki18n("Emanuele Bigiarini"), ki18n("DBus and konqueror plugin"), "pulmro@gmail.com");
    about.addCredit( ki18n( "Andrey Esin" ), ki18n( "Bug fixes" ), "gmlastik@gmail.com", "http://twitter.com/la_stik" );
    //TODO before next release, Add new contributers to credits
    KCmdLineArgs::init( argc, argv, &about );

//     KCmdLineOptions options;
//     options.add("+[URL]", ki18n( "Document to open" ));
//     KCmdLineArgs::addCmdLineOptions(options);
    ChoqokApplication app;
    return app.exec();
}
