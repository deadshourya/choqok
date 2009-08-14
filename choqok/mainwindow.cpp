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

#include "mainwindow.h"
#include "ui_prefs_base.h"
#include "ui_appears_base.h"
#include "settings.h"
#include "accountswidget.h"
#include "accountmanager.h"
// #include "searchwindow.h"
#include "systrayicon.h"
#include "quickpost.h"

#include <KTabWidget>
#include <kconfigdialog.h>
#include <kstatusbar.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kstandardaction.h>
#include <KDE/KLocale>
#include <KMessageBox>
#include <QTimer>
#include "advancedconfig.h"
#include <postwidget.h>
#include <microblogwidget.h>
#include <pluginmanager.h>
#include <passwordmanager.h>
#include <mediamanager.h>
#include <QWheelEvent>
#include <QMenu>
#include <KXMLGUIFactory>
#include <choqokuiglobal.h>

static const int TIMEOUT = 5000;

MainWindow::MainWindow()
    : KXmlGuiWindow(), quickWidget(0)
{
    kDebug();
    setAttribute ( Qt::WA_DeleteOnClose, false );
    setAttribute ( Qt::WA_QuitOnClose, false );
    timelineTimer = new QTimer( this );
    setWindowTitle( i18n("Choqok") );
    Choqok::UI::Global::setMainWindow(this);
    mainWidget = new KTabWidget( this );
    mainWidget->setDocumentMode(true);
    mainWidget->setMovable(true);
    setCentralWidget( mainWidget );
    sysIcon = new SysTrayIcon(this);
    setupActions();
    statusBar()->show();
    setupGUI();

    if ( Settings::updateInterval() > 0 )
        mPrevUpdateInterval = Settings::updateInterval();
    else
        mPrevUpdateInterval = 10;

    connect( timelineTimer, SIGNAL( timeout() ), this, SIGNAL( updateTimelines() ) );
    connect( Choqok::AccountManager::self(), SIGNAL( accountAdded(Choqok::Account*)),
             this, SLOT( addBlog(Choqok::Account*)));
    connect( Choqok::AccountManager::self(), SIGNAL( accountRemoved( const QString& ) ),
             this, SLOT( removeBlog(QString)) );
    connect( Choqok::AccountManager::self(), SIGNAL(allAccountsLoaded()),
             SLOT(loadAllAccounts()) );
    QTimer::singleShot(0, Choqok::PluginManager::self(), SLOT( loadAllPlugins() ) );
    settingsChanged();
    Choqok::AccountManager::self()->loadAllAccounts();

    QPoint pos = Settings::position();
    if(pos.x() != -1 && pos.y() != -1) {
        move(pos);
    }

    connect( Choqok::PluginManager::self(), SIGNAL(pluginLoaded(Choqok::Plugin*)),
             this, SLOT(newPluginAvailable(Choqok::Plugin*)) );
}

MainWindow::~MainWindow()
{
    kDebug();
}

void MainWindow::loadAllAccounts()
{
    kDebug();
    QList<Choqok::Account*> accList = Choqok::AccountManager::self()->accounts();
    if(accList.count()>0) {
        foreach(Choqok::Account *acc, accList){
            addBlog(acc, true);
        }
        kDebug()<<"All accounts loaded. Emitting updateTimelines()";
        emit updateTimelines();
    }
    createQuickPostDialog();
}

void MainWindow::newPluginAvailable( Choqok::Plugin *plugin )
{
    kDebug();
    guiFactory()->addClient(plugin);
}

void MainWindow::nextTab(const QWheelEvent & event)
{
  if(!isVisible())
    return;
  KTabWidget * widget = 0;
  switch(event.orientation()) {
  case Qt::Vertical:
    widget = mainWidget;
  break;
  case Qt::Horizontal:
      ///Commented for now!
//     Choqok::MicroBlogWidget * t = qobject_cast<Choqok::MicroBlogWidget*>( mainWidget->widget( mainWidget->currentIndex() ));
//     if(t)
//       widget = t->tabs;
//     else
      return;
  break;
  }
  if(!widget) return;

  int count = widget->count();
  int index = widget->currentIndex();
  int page;
  if(event.delta() > 0) {
    page = index>0?index-1:count-1;
  } else {
    page = index<count-1?index+1:0;
  }
  widget->setCurrentIndex(page);
}

void MainWindow::setupActions()
{
    KStandardAction::quit( this, SLOT( slotQuit() ), actionCollection() );
    KAction *prefs = KStandardAction::preferences( this, SLOT( optionsPreferences() ), actionCollection() );

    KAction *actUpdate = new KAction( KIcon( "view-refresh" ), i18n( "Update Timelines" ), this );
    actionCollection()->addAction( QLatin1String( "update_timeline" ), actUpdate );
    actUpdate->setShortcut( Qt::Key_F5 );
    actUpdate->setGlobalShortcutAllowed( true );
    KShortcut updateGlobalShortcut( Qt::CTRL | Qt::META | Qt::Key_F5 );
//     updateGlobalShortcut.setAlternate ( Qt::MetaModifier | Qt::Key_F5 );
    actUpdate->setGlobalShortcut( updateGlobalShortcut );
    connect( actUpdate, SIGNAL( triggered( bool ) ), this, SIGNAL( updateTimelines() ) );
//     connect( actUpdate, SIGNAL( triggered( bool ) ), this, SIGNAL( updateSearchResults() ) );

    KAction *newTwit = new KAction( KIcon( "document-new" ), i18n( "Quick Post" ), this );
    actionCollection()->addAction( QLatin1String( "choqok_new_post" ), newTwit );
    newTwit->setShortcut( KShortcut( Qt::CTRL | Qt::Key_T ) );
    newTwit->setGlobalShortcutAllowed( true );
    KShortcut quickTwitGlobalShortcut( Qt::CTRL | Qt::META | Qt::Key_T );
    newTwit->setGlobalShortcut( quickTwitGlobalShortcut );
    connect( newTwit, SIGNAL( triggered(bool) ), this, SLOT( triggerQuickPost()) );

    KAction *markRead = new KAction( KIcon( "mail-mark-read" ), i18n( "Mark All As Read" ), this );
    actionCollection()->addAction( QLatin1String( "choqok_mark_as_read" ), markRead );
    markRead->setShortcut( KShortcut( Qt::CTRL | Qt::Key_R ) );
    connect( markRead, SIGNAL( triggered( bool ) ), this, SIGNAL( markAllAsRead()) );

    KAction *showMain = new KAction( this );
    actionCollection()->addAction( QLatin1String( "toggle_mainwin" ), showMain );
    KShortcut toggleMainGlobalShortcut( Qt::CTRL | Qt::META | Qt::Key_C );
    showMain->setGlobalShortcutAllowed( true );
    showMain->setGlobalShortcut( toggleMainGlobalShortcut/*, KAction::DefaultShortcut, KAction::NoAutoloading*/ );
    showMain->setText( i18n( "Minimize" ) );
    connect( showMain, SIGNAL( triggered( bool ) ), this, SLOT( toggleMainWindow() ) );

    KAction *enableUpdates = new KAction( i18n( "Enable Update Timer" ), this );
    enableUpdates->setCheckable( true );
    actionCollection()->addAction( QLatin1String( "choqok_enable_updates" ), enableUpdates );
    enableUpdates->setShortcut( KShortcut( Qt::CTRL | Qt::Key_U ) );
    enableUpdates->setGlobalShortcutAllowed( true );
    connect( enableUpdates, SIGNAL( toggled( bool ) ), this, SLOT( setTimeLineUpdatesEnabled( bool ) ) );

    KAction *enableNotify = new KAction( i18n( "Enable Notifications" ), this );
    enableNotify->setCheckable( true );
    actionCollection()->addAction( QLatin1String( "choqok_enable_notify" ), enableNotify );
    enableNotify->setShortcut( KShortcut( Qt::CTRL | Qt::Key_N ) );
    enableNotify->setGlobalShortcutAllowed( true );
    connect( enableNotify, SIGNAL( toggled( bool ) ), this, SLOT( setNotificationsEnabled( bool ) ) );

    KAction *clearAvatarCache = new KAction(KIcon("edit-clear"), i18n( "Clear Avatar cache" ), this );
    actionCollection()->addAction( QLatin1String( "choqok_clear_avatar_cache" ), clearAvatarCache );
    QString tip = i18n( "You have to restart Choqok to load avatars again" );
    clearAvatarCache->setToolTip(tip);
    clearAvatarCache->setStatusTip(tip);
    connect( clearAvatarCache, SIGNAL( triggered() ),
             Choqok::MediaManager::self(), SLOT(clearImageCache()) );

    ///SysTray Actions:
    sysIcon->contextMenu()->addAction( newTwit );

    sysIcon->contextMenu()->addAction( actUpdate );
    sysIcon->contextMenu()->addSeparator();

    connect( enableUpdates, SIGNAL( toggled( bool ) ), sysIcon, SLOT( setTimeLineUpdatesEnabled( bool ) ) );
    sysIcon->contextMenu()->addAction( enableUpdates );
    sysIcon->setTimeLineUpdatesEnabled( enableUpdates->isChecked() );
    sysIcon->contextMenu()->addAction( enableNotify );
    sysIcon->contextMenu()->addAction( prefs );

    connect( sysIcon, SIGNAL(quitSelected()), this, SLOT(slotQuit()) );
    connect(sysIcon,SIGNAL(wheelEvent(const QWheelEvent&)),this,SLOT(nextTab(const QWheelEvent&)));
    sysIcon->show();
}

void MainWindow::createQuickPostDialog()
{
    quickWidget = new Choqok::UI::QuickPost( this );
    Choqok::UI::Global::setQuickPostWidget(quickWidget);
    quickWidget->setAttribute(Qt::WA_DeleteOnClose, false);
    connect( quickWidget, SIGNAL( newPostSubmitted(Choqok::JobResult)),
             sysIcon, SLOT( slotJobDone(Choqok::JobResult)) );
}

void MainWindow::triggerQuickPost()
{
    if ( Choqok::AccountManager::self()->accounts().isEmpty() )
    {
        KMessageBox::error( this, i18n ( "Any account created. You must create an account before to make a post." ) );
        return;
    }
    if(!quickWidget)
        createQuickPostDialog();
    if ( quickWidget->isVisible() ) {
        quickWidget->hide();
    } else {
        quickWidget->show();
    }
}

void MainWindow::hideEvent( QHideEvent * event )
{
    Q_UNUSED(event);
    if( !this->isVisible() ) {
        kDebug();
        emit markAllAsRead();
        emit removeOldPosts();
    }
}

void MainWindow::optionsPreferences()
{
    kDebug();

    if ( KConfigDialog::showDialog( "settings" ) )  {
        return;
    }

    KConfigDialog *dialog = new KConfigDialog( this, "settings", Settings::self() );

    QWidget *generalSettingsDlg = new QWidget;
    Ui_prefs_base ui_prefs_base;
    ui_prefs_base.setupUi( generalSettingsDlg );
    dialog->addPage( generalSettingsDlg, i18n( "General" ), "configure" );

    AccountsWidget *accountsSettingsDlg = new AccountsWidget( this );
    dialog->addPage( accountsSettingsDlg, i18n( "Accounts" ), "user-properties" );

    QWidget *appearsSettingsDlg = new QWidget;
    Ui_appears_base ui_appears_base;
    ui_appears_base.setupUi( appearsSettingsDlg );
    dialog->addPage( appearsSettingsDlg, i18n( "Appearance" ), "format-stroke-color" );

    AdvancedConfig *advancedSettingsDlg = new AdvancedConfig( this );
    dialog->addPage( advancedSettingsDlg, i18n("Advanced"), "applications-utilities");

    connect( dialog, SIGNAL( settingsChanged( QString ) ), this, SLOT( settingsChanged() ) );

    dialog->setAttribute( Qt::WA_DeleteOnClose );
    dialog->resize(Settings::configDialogSize());
    dialog->show();
}

void MainWindow::settingsChanged()
{
    kDebug();
    ///TODO Check if there is any account and show a message if there isn't any!
//     if ( AccountManager::self()->accounts().count() < 1 ) {
//         if ( KMessageBox::questionYesNo( this, i18n( "<qt>In order to use this program you need at \
// least one account on <a href='http://identi.ca'>Identi.ca</a> or \
// <a href='http://twitter.com'>Twitter.com</a> services.<br/>Would you like to add your account now?</qt>" )
//                                        ) == KMessageBox::Yes ) {
//             AccountsWizard *dia = new AccountsWizard( QString(), this );
//             dia->setAttribute( Qt::WA_DeleteOnClose );
//             dia->show();
//         }
//     }

    QWidget *w = qobject_cast< QWidget* >(sender());
    if( w ) {
        Settings::setConfigDialogSize(w->size());
    }

    if ( Settings::isCustomUi() ) {
    Choqok::UI::PostWidget::setStyle( Settings::unreadForeColor() , Settings::unreadBackColor(),
                            Settings::readForeColor() , Settings::readBackColor());
    } else {
    QPalette p = window()->palette();
    Choqok::UI::PostWidget::setStyle( p.color(QPalette::WindowText) , p.color(QPalette::Window).lighter() ,
                            p.color(QPalette::WindowText) , p.color(QPalette::Window));
    }

    int count = mainWidget->count();
    for ( int i = 0; i < count; ++i ) {
        qobject_cast<Choqok::UI::MicroBlogWidget *>( mainWidget->widget( i ) )->settingsChanged();
    }
    if ( Settings::notifyEnabled() ) {
        actionCollection()->action( "choqok_enable_notify" )->setChecked( true );
    } else {
        actionCollection()->action( "choqok_enable_notify" )->setChecked( false );
    }
    if ( Settings::updateInterval() > 0 ) {
        timelineTimer->setInterval( Settings::updateInterval() *60000 );
        timelineTimer->start();
//         kDebug()<<"timelineTimer started";
        actionCollection()->action( "choqok_enable_updates" )->setChecked( true );
    } else {
        timelineTimer->stop();
//         kDebug()<<"timelineTimer stoped";
        actionCollection()->action( "choqok_enable_updates" )->setChecked( false );
    }
}

void MainWindow::showStatusMessage( const QString &message, bool isPermanent )
{
    if ( isPermanent ) {
        statusBar()->showMessage( message );
    } else {
        statusBar()->showMessage( message, TIMEOUT );
    }
}

void MainWindow::slotQuit()
{
    kDebug();
    Settings::setPosition( pos() );
    timelineTimer->stop();
    Settings::self()->writeConfig();
    kDebug () << " shutting down plugin manager";
    Choqok::PluginManager::self()->shutdown();
//     Choqok::PasswordManager::self()->deleteLater();
//     Choqok::MediaManager::self()->deleteLater();
    deleteLater();
//     ChoqokApplication *app = qobject_cast<ChoqokApplication*>(kapp);
//     app->quitChoqok();
}

bool MainWindow::queryClose()
{
    return true;
}

bool MainWindow::queryExit()
{
    kDebug();
    return true;
//     ChoqokApplication *app = qobject_cast<ChoqokApplication*>(kapp);
//     if( app->sessionSaving() || app->isShuttingDown() ) {
//         Settings::setPosition( pos() );
//         timelineTimer->stop();
//         Settings::self()->writeConfig();
//         kDebug () << " shutting down plugin manager";
//         Choqok::PluginManager::self()->shutdown();
//         Choqok::PasswordManager::self()->deleteLater();
//         Choqok::MediaManager::self()->deleteLater();
//         return true;
//     } else
//         return false;
}

void MainWindow::disableApp()
{
    kDebug();
    timelineTimer->stop();
//     kDebug()<<"timelineTimer stoped";
    actionCollection()->action( "update_timeline" )->setEnabled( false );
    actionCollection()->action( "choqok_new_post" )->setEnabled( false );
//     actionCollection()->action( "choqok_search" )->setEnabled( false );
    actionCollection()->action( "choqok_mark_as_read" )->setEnabled( false );
//     actionCollection()->action( "choqok_now_listening" )->setEnabled( false );
}

void MainWindow::enableApp()
{
    kDebug();
    if ( Settings::updateInterval() > 0 ) {
        timelineTimer->start();
//         kDebug()<<"timelineTimer started";
    }
    actionCollection()->action( "update_timeline" )->setEnabled( true );
    actionCollection()->action( "choqok_new_post" )->setEnabled( true );
//     actionCollection()->action( "choqok_search" )->setEnabled( true );
    actionCollection()->action( "choqok_mark_as_read" )->setEnabled( true );
//     actionCollection()->action( "choqok_now_listening" )->setEnabled( true );
}

void MainWindow::addBlog( Choqok::Account * account, bool isStartup )
{
    kDebug() << "Adding new Blog, Alias: " << account->alias() << "Blog: " << account->microblog()->serviceName();

    Choqok::UI::MicroBlogWidget *widget = account->microblog()->createMicroBlogWidget(account, this);

//     connect( widget, SIGNAL( sigSetUnread( int ) ), sysIcon, SLOT( slotSetUnread( int ) ) );
    connect( widget, SIGNAL( showStatusMessage(QString,bool)),
             this, SLOT( showStatusMessage( const QString&, bool ) ) );
    connect( widget, SIGNAL( showMe() ), this, SLOT( showBlog()) );
    connect( widget, SIGNAL(updateUnreadCount(int,int)), SLOT(slotUpdateUnreadCount(int,int)) );

    connect( this, SIGNAL( updateTimelines() ), widget, SLOT( updateTimelines() ) );
    connect( this, SIGNAL( markAllAsRead() ), widget, SIGNAL( markAllAsRead() ) );
    connect( this, SIGNAL(removeOldPosts()), widget, SLOT(removeOldPosts()) );

    mainWidget->addTab( widget, account->alias() );

//     if( !isStartup )///FIXME Cause a crash!
//         QTimer::singleShot( 1500, widget, SLOT( updateTimelines() ) );
    enableApp();
    if( mainWidget->count() > 1)
        mainWidget->setTabBarHidden(false);
    else
        mainWidget->setTabBarHidden(true);
}

void MainWindow::removeBlog( const QString & alias )
{
    kDebug();
    int count = mainWidget->count();
    for ( int i = 0; i < count; ++i ) {
        Choqok::UI::MicroBlogWidget * tmp = qobject_cast<Choqok::UI::MicroBlogWidget *>( mainWidget->widget( i ) );
        if ( tmp->currentAccount()->alias() == alias ) {
            mainWidget->removeTab( i );
        if ( mainWidget->count() < 1 )
            disableApp();
        tmp->deleteLater();
        if( mainWidget->count() > 1)
            mainWidget->setTabBarHidden(false);
        else
            mainWidget->setTabBarHidden(true);
        return;
        }
    }
}

void MainWindow::slotUpdateUnreadCount(int change, int sum)
{
    kDebug()<<"Change: "<<change<<" Sum: "<<sum;
    Choqok::UI::MicroBlogWidget *wd = qobject_cast<Choqok::UI::MicroBlogWidget*>(sender());
    Q_ASSERT(wd);
    sysIcon->UpdateUnreadCount(change);
    if(wd) {
        int tabIndex = mainWidget->indexOf(wd);
        if(tabIndex == -1)
            return;
        if(sum > 0)
            mainWidget->setTabText( tabIndex, wd->currentAccount()->alias() + QString("(%1)").arg(sum) );
        else
            mainWidget->setTabText( tabIndex, wd->currentAccount()->alias() );
    }
}

void MainWindow::showBlog()
{
    mainWidget->setCurrentWidget( qobject_cast<QWidget*>( sender() ) );
    if ( !this->isVisible() )
        this->show();
    this->raise();
}

void MainWindow::setTimeLineUpdatesEnabled( bool isEnabled )
{
    kDebug();
    if ( isEnabled ) {
        if( mPrevUpdateInterval > 0 )
            Settings::setUpdateInterval( mPrevUpdateInterval );
        timelineTimer->start( Settings::updateInterval() *60000 );
//         kDebug()<<"timelineTimer started";
    } else {
        mPrevUpdateInterval = Settings::updateInterval();
        timelineTimer->stop();
//         kDebug()<<"timelineTimer stoped";
        Settings::setUpdateInterval( 0 );
    }
}

void MainWindow::setNotificationsEnabled( bool isEnabled )
{
    kDebug();
    if ( isEnabled ) {
        Settings::setNotifyEnabled( true );
    } else {
        Settings::setNotifyEnabled( false );
    }
}

void MainWindow::toggleMainWindow()
{
    if( this->isVisible() )
        hide();
    else
        show();
}

QSize MainWindow::sizeHint() const
{
    return QSize( 350, 380 );
}

#include "mainwindow.moc"