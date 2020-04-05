/****************************************************************
 *
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2011, csoler
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 ****************************************************************/


#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QWidgetAction>

#include "UnseenGxsChatLobbyDialog.h"
#include "gui/gxschats/GxsChatDialog.h"

#include "gui/chat/ChatTabWidget.h"
#include "gui/ChatLobbyWidget.h"
#include "gui/FriendsDialog.h"
#include "gui/MainWindow.h"
#include "gui/chat/ChatWidget.h"
#include "gui/common/rshtml.h"
#include "gui/common/FriendSelectionDialog.h"
#include "gui/common/RSTreeWidgetItem.h"
#include "gui/gxs/GxsIdChooser.h"
#include "gui/gxs/GxsIdDetails.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"
#include "gui/Identity/IdDialog.h"
#include "gui/msgs/MessageComposer.h"
#include "gui/settings/RsharePeerSettings.h"
#include "gui/settings/rsharesettings.h"
#include "util/HandleRichText.h"
#include "util/QtVersion.h"
#include "gui/common/AvatarDefs.h"

#include "retroshare/rsnotify.h"
#include "util/rstime.h"

#include <time.h>
#include <unistd.h>

//for gxs chat
#include <algorithm>
#include "gui/gxs/RsGxsUpdateBroadcastBase.h"
#include "gui/common/UIStateHelper.h"
#include "gui/gxschats/CreateGxsChatMsg.h"
#include "gui/common/UnseenFriendSelectionDialog.h"

#define COLUMN_NAME      0
#define COLUMN_ACTIVITY  1
#define COLUMN_ID        2
#define COLUMN_ICON      3
#define COLUMN_COUNT     4

#define ROLE_SORT            Qt::UserRole + 1
#define ROLE_ID             Qt::UserRole + 2

#define DEBUG_CHAT 1
#define ENABLE_DEBUG 1

const static uint32_t timeToInactivity = 60 * 10;   // in seconds

/** Default constructor */
UnseenGxsChatLobbyDialog::UnseenGxsChatLobbyDialog( const RsGxsGroupId& id, QWidget *parent, Qt::WindowFlags flags)
        : ChatDialog(parent, flags), mGroupId(id),
          bullet_red_128(":/app/images/statusicons/dnd.png"), bullet_grey_128(":/app/images/statusicons/bad.png"),
          bullet_green_128(":/app/images/statusicons/online.png"), bullet_yellow_128(":/app/images/statusicons/away.png"), bullet_unknown_128(":/app/images/statusicons/ask.png")
{


    //////////////////////////////////////////////////////////////////////////////
    /// //Keep for UnseenGxsChatLobbyDialog
    //////////////////////////////////////////////////////////////////////////////

    /* Invoke Qt Designer generated QObject setup routine */
    ui.setupUi(this);

    lastUpdateListTime = 0;

        //connect(ui.actionChangeNickname, SIGNAL(triggered()), this, SLOT(changeNickname()));
    connect(ui.participantsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(participantsTreeWidgetCustomPopupMenu(QPoint)));
    connect(ui.participantsList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(participantsTreeWidgetDoubleClicked(QTreeWidgetItem*,int)));

    connect(ui.filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));

        int S = QFontMetricsF(font()).height() ;
    ui.participantsList->setIconSize(QSize(1.4*S,1.4*S));

    ui.participantsList->setColumnCount(COLUMN_COUNT);
    ui.participantsList->setColumnWidth(COLUMN_ICON, 1.7*S);
    ui.participantsList->setColumnHidden(COLUMN_ACTIVITY,true);
    ui.participantsList->setColumnHidden(COLUMN_ID,true);

    /* Set header resize modes and initial section sizes */
    QHeaderView * header = ui.participantsList->header();
    QHeaderView_setSectionResizeModeColumn(header, COLUMN_NAME, QHeaderView::Stretch);

//    muteAct = new QAction(QIcon(), tr("Mute participant"), this);
//    voteNegativeAct = new QAction(QIcon(":/icons/png/thumbs-down.png"), tr("Ban this person (Sets negative opinion)"), this);
//    voteNeutralAct = new QAction(QIcon(":/icons/png/thumbs-neutral.png"), tr("Give neutral opinion"), this);
//    votePositiveAct = new QAction(QIcon(":/icons/png/thumbs-up.png"), tr("Give positive opinion"), this);
    //distantChatAct = new QAction(QIcon(":/images/chat_24.png"), tr("Start private chat"), this);
    sendMessageAct = new QAction(QIcon(":/images/mail_new.png"), tr("Send Email"), this);
    //showInPeopleAct = new QAction(QIcon(), tr("Show author in people tab"), this);

    QActionGroup *sortgrp = new QActionGroup(this);
    actionSortByName = new QAction(QIcon(), tr("Sort by Name"), this);
    actionSortByName->setCheckable(true);
    actionSortByName->setChecked(true);
    actionSortByName->setActionGroup(sortgrp);

    actionSortByActivity = new QAction(QIcon(), tr("Sort by Activity"), this);
    actionSortByActivity->setCheckable(true);
    actionSortByActivity->setChecked(false);
    actionSortByActivity->setActionGroup(sortgrp);

    connect(sendMessageAct, SIGNAL(triggered()), this, SLOT(sendMessage()));
    connect(actionSortByName, SIGNAL(triggered()), this, SLOT(sortParcipants()));
    connect(actionSortByActivity, SIGNAL(triggered()), this, SLOT(sortParcipants()));

        /* Add filter actions */
    QTreeWidgetItem *headerItem = ui.participantsList->headerItem();
    QString headerText = headerItem->text(COLUMN_NAME );
    ui.filterLineEdit->setPlaceholderText("Search ");
    ui.filterLineEdit->showFilterIcon();
    // just empiric values
    double scaler_factor = S > 25 ? 2.4 : 1.8;
    QSize icon_size(scaler_factor * S, scaler_factor * S);

    // Add a button to invite friends.
    //
    inviteFriendsButton = new QToolButton ;
    inviteFriendsButton->setMinimumSize(icon_size);
    inviteFriendsButton->setMaximumSize(icon_size);
    inviteFriendsButton->setText(QString()) ;
    inviteFriendsButton->setAutoRaise(true) ;
    inviteFriendsButton->setToolTip(tr("Invite friends to this group"));

    mParticipantCompareRole = new RSTreeWidgetItemCompareRole;
    mParticipantCompareRole->setRole(COLUMN_ACTIVITY, ROLE_SORT);

    {
    QIcon icon ;
    icon.addPixmap(QPixmap(":/icons/png/invite.png")) ;
    inviteFriendsButton->setIcon(icon) ;
    inviteFriendsButton->setIconSize(icon_size);
    }

    connect(inviteFriendsButton, SIGNAL(clicked()), this , SLOT(inviteFriends()));

    getChatWidget()->addTitleBarWidget(inviteFriendsButton) ;

    //This one is for distributed chat (from file distributedchat.h)
//    RsGxsId current_id;
//    rsMsgs->getIdentityForChatLobby(lobbyId, current_id);

//    uint32_t idChooserFlag = IDCHOOSER_ID_REQUIRED;
//    ChatLobbyInfo lobbyInfo ;
//    //This one is for distributed chat (from file distributedchat.h)
//    if(rsMsgs->getChatLobbyInfo(lobbyId,lobbyInfo)) {
//        if (lobbyInfo.lobby_flags & RS_CHAT_LOBBY_FLAGS_PGP_SIGNED) {
//            idChooserFlag |= IDCHOOSER_NON_ANONYMOUS;
//        }
//    }
//    ownIdChooser = new GxsIdChooser() ;
//    ownIdChooser->loadIds(idChooserFlag, current_id) ;

//    QWidgetAction *checkableAction = new QWidgetAction(this);
//    checkableAction->setDefaultWidget(ownIdChooser);

//    ui.chatWidget->addToolsAction(checkableAction);

    connect(ui.chatWidget, SIGNAL(textBrowserAskContextMenu(QMenu*,QString,QPoint)), this, SLOT(textBrowserAskContextMenu(QMenu*,QString,QPoint)));
//    connect(ownIdChooser,SIGNAL(currentIndexChanged(int)),this,SLOT(changeNickname())) ;

// Add a members List Button
    membersListButton = new QToolButton ;
    membersListButton->setMinimumSize(icon_size);
    membersListButton->setMaximumSize(icon_size);
    membersListButton->setText(QString()) ;
    membersListButton->setAutoRaise(true) ;
    membersListButton->setToolTip(tr("Show members list"));

    {
    QIcon icon ;
    icon.addPixmap(QPixmap(":/icons/png/memberslist.png")) ;
    membersListButton->setIcon(icon) ;
    membersListButton->setIconSize(icon_size);
    }

    getChatWidget()->addTitleBarWidget(membersListButton) ;

    membersListButton2 = new QToolButton ;
    membersListButton2->setMinimumSize(icon_size);
    membersListButton2->setMaximumSize(icon_size);
    membersListButton2->setText(QString()) ;
    membersListButton2->setAutoRaise(true) ;
    membersListButton2->setToolTip(tr("Hide members list"));

    {
    QIcon icon ;
    icon.addPixmap(QPixmap(":/icons/png/memberslist2.png")) ;
    membersListButton2->setIcon(icon) ;
    membersListButton2->setIconSize(icon_size);
    }

    getChatWidget()->addTitleBarWidget(membersListButton2) ;

    ui.participantsFrame->hide();
    this->membersListButton2->hide();
    connect(membersListButton, SIGNAL(clicked()), membersListButton  , SLOT(hide()));
    connect(membersListButton, SIGNAL(clicked()), membersListButton2 , SLOT(show()));
    connect(membersListButton, SIGNAL(clicked()), ui.participantsFrame , SLOT(show()));

    connect(membersListButton2, SIGNAL(clicked()), membersListButton2  , SLOT(hide()));
    connect(membersListButton2, SIGNAL(clicked()), membersListButton , SLOT(show()));
    connect(membersListButton2, SIGNAL(clicked()), ui.participantsFrame , SLOT(hide()));


// Add a  Unsubscribe Button
    unsubscribeButton = new QToolButton;
    unsubscribeButton->setMinimumSize(icon_size);
    unsubscribeButton->setMaximumSize(icon_size);
    unsubscribeButton->setText(QString()) ;
    unsubscribeButton->setAutoRaise(true) ;
    unsubscribeButton->setToolTip(tr("Leave this group chat"));

    {
    QIcon icon ;
    icon.addPixmap(QPixmap(":/icons/png/leave.png")) ;
    unsubscribeButton->setIcon(icon) ;
    unsubscribeButton->setIconSize(icon_size);
    }

    /* Initialize splitter */
    ui.splitter->setStretchFactor(0, 1);
    ui.splitter->setStretchFactor(1, 0);
    ui.splitter->setCollapsible(0, false);
    ui.splitter->setCollapsible(1, false);

    connect(unsubscribeButton, SIGNAL(clicked()), this , SLOT(leaveGxsGroupChat()));

    getChatWidget()->addTitleBarWidget(unsubscribeButton) ;

    old_participating_friends.clear();
    allDownloadedMsgs.clear();

    //////////////////////////////////////////////////////////////////////////////
    /// //END of Keep for UnseenGxsChatLobbyDialog
    //////////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////////
    /// //4 classes constructors that this class uses
    //////////////////////////////////////////////////////////////////////////////
    //GxsUpdateBroadcastWidget constructor
    mBase = new RsGxsUpdateBroadcastBase(rsGxsChats, this);
    connect(mBase, SIGNAL(fillDisplay(bool)), this, SLOT(fillDisplay(bool)));
    mInterfaceHelper = rsGxsChats;

    //GxsMessageFramePostsWidget constructor
    mSubscribeFlags = 0;
    mFillThread = NULL;
    mTokenTypeGroupData = nextTokenType();
    mTokenTypeAllPosts = nextTokenType();
    mTokenTypePosts = nextTokenType();

    //GxsMessageFrameWidget constructor
    mNextTokenType = 0;
    mTokenQueue = new TokenQueue(rsGxsChats->getTokenService(), this);
    //mStateHelper = new UIStateHelper(this);

    /* Set read status */
    mTokenTypeAcknowledgeReadStatus = nextTokenType();
    mAcknowledgeReadStatusToken = 0;

    showAllPostOnlyOnce = false;
    //GxsChatPostsWidget constructor
    setGroupId(id);

    /* Add dummy entry to store waiting status */
    //mStateHelper->addWidget(mTokenTypeAcknowledgeReadStatus, NULL, 0);

    /// End for 4 classes constructors

}

void UnseenGxsChatLobbyDialog::leaveGxsGroupChat()
{
    //TODO: change leave group with gxs groupchat
    //Need to update the group with gxsChat->updateGroup with new member list for other before unsubscribe
    std::list<RsGxsGroupId> groupChatId;
    groupChatId.push_back(mGroupId);
    std::vector<RsGxsChatGroup> chatsInfo;
    if (rsGxsChats->getChatsInfo(groupChatId, chatsInfo))
    {
        if (chatsInfo.size() > 0)
        {
            GxsChatMember myown;
            rsGxsChats->getOwnMember(myown);
            if(chatsInfo[0].members.find(myown)!= chatsInfo[0].members.end())
                chatsInfo[0].members.erase(myown);
            uint32_t token;
            rsGxsChats->updateGroup(token, chatsInfo[0]);
        }
    }
    emit gxsGroupLeave(groupId()) ;
}
void UnseenGxsChatLobbyDialog::inviteFriends()
{
    std::cerr << "Inviting friends" << std::endl;

    std::set<GxsChatMember> invitedMemberList = UnseenFriendSelectionDialog::selectFriends_GxsChatMember(NULL,mGroupId, tr("Invite friends"),tr(""), UnseenFriendSelectionWidget::MODUS_CHECK,UnseenFriendSelectionWidget::SHOW_CONTACTS) ;

    //UnseenFriendSelectionDialog dialog(parent,"",UnseenFriendSelectionWidget::MODUS_CHECK,UnseenFriendSelectionWidget::SHOW_CONTACTS,UnseenFriendSelectionWidget::IDTYPE_SSL,psids) ;

    std::cerr << " All new invited friends: " << std::endl;
    for(std::set<GxsChatMember>::iterator it = invitedMemberList.begin(); it != invitedMemberList.end(); ++it)
    {
        std::cerr << " Member: " << (*it).nickname <<  std::endl;
    }

    //logic for member invite members:
    std::list<RsGxsGroupId> groupChatId;
    groupChatId.push_back(mGroupId);
    std::vector<RsGxsChatGroup> chatsInfo;
    if (rsGxsChats->getChatsInfo(groupChatId, chatsInfo))
    {
        if (chatsInfo.size() > 0)
        {
            if(chatsInfo[0].mMeta.mInternalCircle.isNull())
            {
                chatsInfo[0].mMeta.mInternalCircle = RsGxsCircleId::random();
            }

            for(std::set<GxsChatMember>::iterator it = invitedMemberList.begin(); it != invitedMemberList.end(); ++it)
            {
                if(chatsInfo[0].members.find(*it)== chatsInfo[0].members.end())
                    chatsInfo[0].members.insert(*it);
            }
            uint32_t token;
            rsGxsChats->updateGroup(token, chatsInfo[0]);


        }
    }
}

void UnseenGxsChatLobbyDialog::participantsTreeWidgetCustomPopupMenu(QPoint)
{
//    QList<QTreeWidgetItem*> selectedItems = ui.participantsList->selectedItems();
//    QList<RsGxsId> idList;
//    QList<QTreeWidgetItem*>::iterator item;
//    for (item = selectedItems.begin(); item != selectedItems.end(); ++item)
//    {
//        RsGxsId gxs_id ;
//        dynamic_cast<GxsIdRSTreeWidgetItem*>(*item)->getId(gxs_id) ;
//        idList.append(gxs_id);
//    }

//    QMenu contextMnu(this);
//    contextMnu.addAction(actionSortByActivity);
//    contextMnu.addAction(actionSortByName);

//    contextMnu.addSeparator();

//    initParticipantsContextMenu(&contextMnu, idList);

//    contextMnu.exec(QCursor::pos());
}

void UnseenGxsChatLobbyDialog::textBrowserAskContextMenu(QMenu* contextMnu, QString anchorForPosition, const QPoint /*point*/)
{
    if (anchorForPosition.startsWith(PERSONID)) {
        QString strId = anchorForPosition.replace(PERSONID, "");
        if (strId.contains(" "))
            strId.truncate(strId.indexOf(" "));

        contextMnu->addSeparator();

        QList<RsGxsId> idList;
        idList.append(RsGxsId(strId.toStdString()));
        initParticipantsContextMenu(contextMnu, idList);
    }
}

void UnseenGxsChatLobbyDialog::initParticipantsContextMenu(QMenu *contextMnu, QList<RsGxsId> idList)
{
    if (!contextMnu)
        return;
    if (idList.isEmpty())
        return;

    contextMnu->addAction(sendMessageAct);
    sendMessageAct->setEnabled(true);
    sendMessageAct->setData(QVariant::fromValue(idList));
}

void UnseenGxsChatLobbyDialog::voteParticipant()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }
    updateParticipantsList();
}

void UnseenGxsChatLobbyDialog::showInPeopleTab()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }

    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();
    if (idList.count() != 1)
        return;

    RsGxsId nickname = idList.at(0);

    IdDialog *idDialog = dynamic_cast<IdDialog*>(MainWindow::getPage(MainWindow::People));
    if (!idDialog)
        return ;
    MainWindow::showWindow(MainWindow::People);

    idDialog->navigate(nickname);
}

void UnseenGxsChatLobbyDialog::init(const gxsChatId &id, const QString &/*title*/)
{
    ChatLobbyInfo linfo ;

    QString title;

    //TODO:  use id (gxsChatId) to get the gxsGroupChat Info: replace this by gxs groupchat function

//    if(rsMsgs->getChatLobbyInfo(lobbyId,linfo))
//    {
//        title = QString::fromUtf8(linfo.lobby_name.c_str());

//        QString msg = tr("Welcome to group chat: %1").arg(RsHtml::plainText(linfo.lobby_name));
//        _lobby_name = QString::fromUtf8(linfo.lobby_name.c_str()) ;
//        if (!linfo.lobby_topic.empty()) {
//            msg += "\n" + tr("Topic: %1").arg(RsHtml::plainText(linfo.lobby_topic));
//        }
//        ui.chatWidget->setWelcomeMessage(msg);
//    }

    //TODO: need to check, if we can use the chatId here for the gxs chat?
    //
    ChatDialog::init(id, title);

//    RsGxsId gxs_id;
//    rsMsgs->getIdentityForChatLobby(lobbyId, gxs_id);

    RsIdentityDetails details ;

    // This lets the backend some time to load our own identity in cache.
    // It will only loop for at most 1 second the first time.

//    for(int i=0;i<3;++i)
//        if(rsIdentity->getIdDetails(gxs_id,details))
//            break ;
//        else
//            rstime::rs_usleep(1000*300) ;

    if(rsIdentity->getIdDetails((RsGxsId)id.toGxsGroupId(),details))
        ui.chatWidget->setName(QString::fromUtf8(details.mNickname.c_str()));
    //ui.chatWidget->addToolsAction(ui.actionChangeNickname);
    ui.chatWidget->setDefaultExtraFileFlags(RS_FILE_REQ_ANONYMOUS_ROUTING);

    lastUpdateListTime = 0;

    // add to window

    GxsChatDialog  *unseenGxsChatLobbyPage = dynamic_cast<GxsChatDialog*>(MainWindow::getPage(MainWindow::GxsChats));
    if (unseenGxsChatLobbyPage) {
        unseenGxsChatLobbyPage->addChatPage(this) ;
    }

    /** List of muted Participants */
    mutedParticipants.clear() ;

    //try to update the member status on member list
    updateParticipantsList();

    // load settings
    processSettings(true);
}

/** Destructor. */
UnseenGxsChatLobbyDialog::~UnseenGxsChatLobbyDialog()
{
    // announce leaving of lobby

    //unseenp2p - no need to leave group (unsubscribeChatLobby = leave group)
    // check that the lobby still exists.
//    if (mChatId.isLobbyId()) {
//        rsMsgs->unsubscribeChatLobby(mChatId.toLobbyId());
//	}

    // save settings
    processSettings(false);
}

ChatWidget *UnseenGxsChatLobbyDialog::getChatWidget()
{
    return ui.chatWidget;
}

bool UnseenGxsChatLobbyDialog::notifyBlink()
{
    return (Settings->getChatLobbyFlags() & RS_CHATLOBBY_BLINK);
}

void UnseenGxsChatLobbyDialog::processSettings(bool load)
{
    Settings->beginGroup(QString("UnseenGxsChatLobbyDialog"));

    if (load) {
        // load settings

        // state of splitter
        ui.splitter->restoreState(Settings->value("splitter").toByteArray());

        // load sorting
        actionSortByActivity->setChecked(Settings->value("sortbyActivity", QVariant(false)).toBool());
        actionSortByName->setChecked(Settings->value("sortbyName", QVariant(true)).toBool());

        //try to open the last chat window from here
//        ChatLobbyWidget *chatLobbyPage = dynamic_cast<ChatLobbyWidget*>(MainWindow::getPage(MainWindow::ChatLobby));
//        if (chatLobbyPage) {
//            chatLobbyPage->openLastChatWindow();
//        }
    } else {
        // save settings

        // state of splitter
        Settings->setValue("splitter", ui.splitter->saveState());

        //save sorting
        Settings->setValue("sortbyActivity", actionSortByActivity->isChecked());
        Settings->setValue("sortbyName", actionSortByName->isChecked());
    }

    Settings->endGroup();
}

/**
 * Change your Nickname
 *
 * - send a Message to all Members => later: send hidden message to clients, so they can actualize there mutedParticipants list
 */
void UnseenGxsChatLobbyDialog::setIdentity(const RsGxsId& gxs_id)
{
//    rsMsgs->setIdentityForChatLobby(lobbyId, gxs_id) ;

//    // get new nick name
//    RsGxsId newid;

//    if (rsMsgs->getIdentityForChatLobby(lobbyId, newid))
//    {
//        RsIdentityDetails details ;
//        rsIdentity->getIdDetails(gxs_id,details) ;

//        ui.chatWidget->setName(QString::fromUtf8(details.mNickname.c_str()));
//    }
}

/**
 * Dialog: Change your Nickname in the ChatLobby
 */
void UnseenGxsChatLobbyDialog::changeNickname()
{
//    RsGxsId current_id;
//    rsMsgs->getIdentityForChatLobby(lobbyId, current_id);

//    RsGxsId new_id ;
//    ownIdChooser->getChosenId(new_id) ;

//    if(!new_id.isNull() && new_id != current_id)
//        setIdentity(new_id);
}

/**
 * We get a new Message from a chat participant
 *
 * - Ignore Messages from muted chat participants
 */
void UnseenGxsChatLobbyDialog::addChatMsg(const ChatMessage& msg)
{
    QDateTime sendTime = QDateTime::fromTime_t(msg.sendTime);
    QDateTime recvTime = QDateTime::fromTime_t(msg.recvTime);
    QString message = QString::fromUtf8(msg.msg.c_str());
    RsGxsId gxs_id = msg.lobby_peer_gxs_id ;

    //if(!isParticipantMuted(gxs_id))
    {
        // We could change addChatMsg to display the peers icon, passing a ChatId

        RsIdentityDetails details ;

        QString name ;
        if(rsIdentity->getIdDetails(gxs_id,details))
            name = QString::fromUtf8(details.mNickname.c_str()) ;
        else
            name = QString::fromUtf8(msg.peer_alternate_nickname.c_str()) + " (" + QString::fromStdString(gxs_id.toStdString()) + ")" ;

        ui.chatWidget->addChatMsg(msg.incoming, name, gxs_id, sendTime, recvTime, message, ChatWidget::MSGTYPE_NORMAL);
        //emit messageReceived(msg.incoming, gxsChatId(groupId()), sendTime, name, message) ;

        // This is a trick to translate HTML into text.
        QTextEdit editor;
        editor.setHtml(message);
        QString notifyMsg = name + ": " + editor.toPlainText();

        if(msg.incoming)
        {
            if(notifyMsg.length() > 30)
                MainWindow::displayLobbySystrayMsg(tr("Group chat") + ": " + _lobby_name, notifyMsg.left(30) + QString("..."));
            else
                MainWindow::displayLobbySystrayMsg(tr("Group chat") + ": " + _lobby_name, notifyMsg);
        }

    }

    // also update peer list.

    time_t now = time(NULL);

   QList<QTreeWidgetItem*>  qlFoundParticipants=ui.participantsList->findItems(QString::fromStdString(gxs_id.toStdString()),Qt::MatchExactly,COLUMN_ID);
    if (qlFoundParticipants.count()!=0) qlFoundParticipants.at(0)->setText(COLUMN_ACTIVITY,QString::number(now));

    if (now > lastUpdateListTime) {
        lastUpdateListTime = now;
        updateParticipantsList();
    }
}

void UnseenGxsChatLobbyDialog::updateTitle(QString title)
{
     ui.chatWidget->setTitle(title);
}

/**
 * Regenerate the QTreeWidget participant list of a Chat Lobby
 *
 * Show yellow icon for muted Participants
 */
void UnseenGxsChatLobbyDialog::updateParticipantsList()
{
    ChatLobbyInfo linfo;

    bool hasNewMemberJoin = false;

    std::list<RsGxsGroupId> groupChatId;
    groupChatId.push_back(mGroupId);
    std::vector<RsGxsChatGroup> chatsInfo;
    if (rsGxsChats->getChatsInfo(groupChatId, chatsInfo))
    {
        if (chatsInfo.size() > 0)
        {
            RsGxsChatGroup thisGroup = chatsInfo[0];
            //at first we can add the own member to the groupData and sync for others
            std::set<GxsChatMember> members_update2;

            std::set<GxsChatMember> new_participating_friends;
            new_participating_friends.clear();
            bool isIdentical = true;
            if (old_participating_friends.size() > 0) // that mean it already saved the member list at least one time
            {
                //THIS FEATURE NEED TO BE WAITED FROM BACKEND
                //at first check this user has in the member list or not, if you, that mean the admin already remove this user
//                bool isAdminRemoveYou = true;
//                for (auto it(chatsInfo[0].members.begin()); it != chatsInfo[0].members.end(); ++it)
//                {
//                    if ((*it).chatPeerId == rsPeers->getOwnId())
//                    {
//                        isAdminRemoveYou = false;
//                        break;
//                    }
//                }
//                if (isAdminRemoveYou)
//                {
//                    //warning about the admin remove you in the content, disable the inbox, user then remove the window himself
//                    ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(), QDateTime::currentDateTime(), tr("The admin already remove you from this room."), ChatWidget::MSGTYPE_SYSTEM);
//                    //ui.chatWidget-
//                    return;
//                }

                //need to compare the new list and the old list, if there is only one different so need to update all again!
                // if the 2 lists are identical, just return and do nothing. Need to check this option first!
                for (auto it(chatsInfo[0].members.begin()); it != chatsInfo[0].members.end(); ++it)
                {
                    //at first get all new list, and check in the old one
                    if (new_participating_friends.find((*it)) == new_participating_friends.end())
                    {
                        new_participating_friends.insert((*it));
                        //only if check there is one new member that not exist in the old list
                        if (old_participating_friends.find((*it)) == old_participating_friends.end())
                        {
                            isIdentical = false;
                            //update the system message about new users joining
                            ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(), QDateTime::currentDateTime(), tr("%1 joined the room.").arg(RsHtml::plainText((*it).nickname)), ChatWidget::MSGTYPE_SYSTEM);

                        }
                    }

                }

                //The removed member will be in the old list, but not in the current (new) list:
                for (auto it(old_participating_friends.begin()); it != old_participating_friends.end(); ++it)
                {
                     if (new_participating_friends.find((*it)) == new_participating_friends.end())
                     {
                         isIdentical = false;
                         //update the system message about old users leave group
                         ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(), QDateTime::currentDateTime(), tr("%1 leaved the room.").arg(RsHtml::plainText((*it).nickname)), ChatWidget::MSGTYPE_SYSTEM);
                     }
                }


                if (new_participating_friends == old_participating_friends)
                {
                    isIdentical = true;
                }
                else
                {
                    isIdentical = false;
                    //Need to check which new users join, then show joining warnings in the chat content

                }

                // if 2 lists are the same, just return and do nothing
                if (isIdentical)
                {
                    return;
                }

            }

            if (old_participating_friends.size() == 0 || !isIdentical)       // this mean this is the first time, need to do at first time
            {
                GxsChatMember myown;
                for (auto it(chatsInfo[0].members.begin()); it != chatsInfo[0].members.end(); ++it)
                {
                    if (old_participating_friends.find((*it)) == old_participating_friends.end())
                    {
                        old_participating_friends.insert((*it));
                    }
                    if (new_participating_friends.find((*it)) == new_participating_friends.end())
                    {
                        new_participating_friends.insert((*it));
                    }


                    //check own ssld
                    if (it->chatPeerId == rsPeers->getOwnId())
                    {
                        std::list<RsGxsId> own_ids ;
                        rsIdentity->getOwnIds(own_ids) ;
                        if(!own_ids.empty())
                        {
                            myown.chatPeerId = it->chatPeerId;
                            myown.nickname = it->nickname;
                            myown.chatinfo = it->chatinfo;
                            myown.chatGxsId = own_ids.front();
                            myown.status = true;
                        }
                    }
                    else
                    {
                        members_update2.insert(*it);
                    }
                }
               members_update2.insert(myown);
               thisGroup.members = members_update2;

//               std::cerr << "   Participating friends: " << std::endl;
//               std::cerr << "   groupchat name: " << chatsInfo[0].mDescription<< std::endl;
//               std::cerr << "   Participating nick names (sslId): " << std::endl;


               QList<QTreeWidgetItem*>  qlOldParticipants=ui.participantsList->findItems("*",Qt::MatchWildcard,COLUMN_ID);

               std::set<RsGxsId> gxs_ids;
               for (auto it2(thisGroup.members.begin()); it2 != thisGroup.members.end(); ++it2)
                   gxs_ids.insert((*it2).chatGxsId);

               foreach(QTreeWidgetItem *qtwiCur,qlOldParticipants)
                   if(gxs_ids.find(RsGxsId((*qtwiCur).text(COLUMN_ID).toStdString())) == gxs_ids.end())
                   {
                       //Old Participant go out, remove it
                       int index = ui.participantsList->indexOfTopLevelItem(qtwiCur);
                       delete ui.participantsList->takeTopLevelItem(index);
                   }

               for (auto it2(thisGroup.members.begin()); it2 != thisGroup.members.end(); ++it2)
               {
                   //do not show yourself and the empty member
                   if ((*it2).nickname == "" || (*it2).chatPeerId == rsPeers->getOwnId()) continue;

                   std::cerr << " nick:  " <<(*it2).nickname << ", sslId:  " << (*it2).chatPeerId.toStdString() << ", gxsId:  " << (*it2).chatGxsId.toStdString() << std::endl;
                   QString participant = QString::fromUtf8( it2->chatGxsId.toStdString().c_str() );

                   QList<QTreeWidgetItem*>  qlFoundParticipants=ui.participantsList->findItems(participant,Qt::MatchExactly,COLUMN_ID);

                   //GxsIdRSTreeWidgetItem *widgetitem;
                   UnseenMemberTreeWidgetItem *unsWidgetItem;

                   if (qlFoundParticipants.count()==0)
                   {
                       // TE: Add Wigdet to participantsList with Checkbox, to mute Participant
                       unsWidgetItem = new UnseenMemberTreeWidgetItem(mParticipantCompareRole,GxsIdDetails::ICON_TYPE_AVATAR);
                       unsWidgetItem->setMember((*it2), COLUMN_NAME, true) ;
                       unsWidgetItem->setId((*it2).chatGxsId,COLUMN_NAME, true) ;
                       unsWidgetItem->setText(COLUMN_ACTIVITY,QString::number(time(NULL) - timeToInactivity));
                       unsWidgetItem->setText(COLUMN_ID,QString::fromStdString(it2->chatGxsId.toStdString()));

                       ui.participantsList->addTopLevelItem(unsWidgetItem);
                       hasNewMemberJoin = true;
                   }
                   else
                       unsWidgetItem = dynamic_cast<UnseenMemberTreeWidgetItem*>(qlFoundParticipants.at(0));

                   //try to update the avatar
                   unsWidgetItem->forceUpdate();

                   time_t tLastAct=unsWidgetItem->text(COLUMN_ACTIVITY).toInt();
                   time_t now = time(NULL);
                   unsWidgetItem->setSizeHint(COLUMN_ICON, QSize(20,20));

                   //Change the member status using ssl connection here

                   if (!it2->chatPeerId.isNull())
                   {
                       StatusInfo statusContactInfo;
                       rsStatus->getStatus(it2->chatPeerId,statusContactInfo);
                       switch (statusContactInfo.status)
                       {
                       case RS_STATUS_OFFLINE:
                           unsWidgetItem->setIcon(COLUMN_ICON, bullet_grey_128 );
                           break;
                       case RS_STATUS_INACTIVE:
                           unsWidgetItem->setIcon(COLUMN_ICON, bullet_yellow_128 );
                           break;
                       case RS_STATUS_ONLINE:
                           unsWidgetItem->setIcon(COLUMN_ICON, bullet_green_128);
                           break;
                       case RS_STATUS_AWAY:
                           unsWidgetItem->setIcon(COLUMN_ICON, bullet_yellow_128);
                           break;
                       case RS_STATUS_BUSY:
                           unsWidgetItem->setIcon(COLUMN_ICON, bullet_red_128);
                           break;
                       }
                   }
                   else
                   {
                       unsWidgetItem->setIcon(COLUMN_ICON, bullet_unknown_128 );
                   }
               }

            old_participating_friends = new_participating_friends;
            }
        }
    }
}

/**
 * Called when a Participant get Clicked / Changed
 *
 * Check if the Checkbox altered and Mute User
 */
void UnseenGxsChatLobbyDialog::changeParticipationState()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }

    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();

    for (QList<RsGxsId>::iterator item = idList.begin(); item != idList.end(); ++item)
    {
        std::cerr << "check Partipation status for '" << *item << std::endl;
        if (act->isChecked()) {
            muteParticipant(*item);
        } else {
            unMuteParticipant(*item);
        }
    }

    updateParticipantsList();
}

void UnseenGxsChatLobbyDialog::participantsTreeWidgetDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item) {
        return;
    }

    if(column == COLUMN_NAME)
    {
        getChatWidget()->pasteText("@" + RsHtml::plainText(item->text(COLUMN_NAME))) ;
        return ;
    }
}

void UnseenGxsChatLobbyDialog::distantChatParticipant()
{
//    QAction *act = dynamic_cast<QAction*>(sender()) ;
//    if(!act)
//    {
//        std::cerr << "No sender! Some bug in the code." << std::endl;
//        return ;
//    }

//    std::cerr << " initiating distant chat" << std::endl;

//    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();
//    if (idList.count() != 1)
//        return;

//    RsGxsId gxs_id = idList.at(0);
//    if (gxs_id.isNull())
//        return;

//    RsGxsId own_id;
//    rsMsgs->getIdentityForChatLobby(lobbyId, own_id);

//    DistantChatPeerId tunnel_id;
//    uint32_t error_code ;

//    if(! rsMsgs->initiateDistantChatConnexion(gxs_id,own_id,tunnel_id,error_code))
//    {
//        QString error_str ;
//        switch(error_code)
//        {
//            case RS_DISTANT_CHAT_ERROR_DECRYPTION_FAILED   : error_str = tr("Decryption failed.") ; break ;
//            case RS_DISTANT_CHAT_ERROR_SIGNATURE_MISMATCH  : error_str = tr("Signature mismatch") ; break ;
//            case RS_DISTANT_CHAT_ERROR_UNKNOWN_KEY         : error_str = tr("Unknown key") ; break ;
//            case RS_DISTANT_CHAT_ERROR_UNKNOWN_HASH        : error_str = tr("Unknown hash") ; break ;
//            default:
//                error_str = tr("Unknown error.") ;
//        }
//        QMessageBox::warning(NULL,tr("Cannot start distant chat"),tr("Distant chat cannot be initiated:")+" "+error_str
//                             +QString::number(error_code)) ;
//    }
}

void UnseenGxsChatLobbyDialog::sendMessage()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }

    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();

    MessageComposer *nMsgDialog = MessageComposer::newMsg();
    if (nMsgDialog == NULL)
        return;

    for (QList<RsGxsId>::iterator item = idList.begin(); item != idList.end(); ++item)
        nMsgDialog->addRecipient(MessageComposer::TO,  *item);

    nMsgDialog->show();
    nMsgDialog->activateWindow();

    /* window will destroy itself! */
}


void UnseenGxsChatLobbyDialog::muteParticipant(const RsGxsId& nickname)
{
    std::cerr << " Mute " << std::endl;

//    RsGxsId gxs_id;
//    rsMsgs->getIdentityForChatLobby(lobbyId, gxs_id);

//    if (gxs_id!=nickname)
//        mutedParticipants.insert(nickname);
}

void UnseenGxsChatLobbyDialog::unMuteParticipant(const RsGxsId& id)
{
    std::cerr << " UnMute " << std::endl;
    mutedParticipants.erase(id);
}

/**
 * Is this nickName already known in the lobby
 */
bool UnseenGxsChatLobbyDialog::isNicknameInLobby(const RsGxsId& nickname)
{
    ChatLobbyInfo clinfo;

    if(! rsMsgs->getChatLobbyInfo(lobbyId,clinfo))
        return false ;

    return clinfo.gxs_ids.find(nickname) != clinfo.gxs_ids.end() ;
}

/**
 * Should Messages from this Nickname be muted?
 *
 * At the moment it is not possible to 100% know which peer sendet the message, and only
 * the nickname is available. So this couldn't work for 100%. So, for example,  if a peer
 * change his name to the name of a other peer, we couldn't block him. A real implementation
 * will be possible if we transfer a temporary Session ID from the sending Retroshare client
 * version 0.6
 *
 * @param QString nickname to check
 */
bool UnseenGxsChatLobbyDialog::isParticipantMuted(const RsGxsId& participant)
{
    // nickname in Mute list
    return mutedParticipants.find(participant) != mutedParticipants.end();
}

QString UnseenGxsChatLobbyDialog::getParticipantName(const RsGxsId& gxs_id) const
{
    RsIdentityDetails details ;

    QString name ;
    if(rsIdentity->getIdDetails(gxs_id,details))
        name = QString::fromUtf8(details.mNickname.c_str()) ;
    else
        name = QString::fromUtf8("[Unknown] (") + QString::fromStdString(gxs_id.toStdString()) + ")" ;

    return name ;
}


void UnseenGxsChatLobbyDialog::displayLobbyEvent(int event_type, const RsGxsId& gxs_id, const QString& str)
{
    RsGxsId qsParticipant;

    QString name= getParticipantName(gxs_id) ;

    switch (event_type)
    {
    case RS_CHAT_LOBBY_EVENT_PEER_LEFT:
        qsParticipant=gxs_id;
        ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(), QDateTime::currentDateTime(), tr("%1 has left the room.").arg(RsHtml::plainText(name)), ChatWidget::MSGTYPE_SYSTEM);
        emit peerLeft(id()) ;
        break;
    case RS_CHAT_LOBBY_EVENT_PEER_JOINED:
        qsParticipant=gxs_id;
        ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(), QDateTime::currentDateTime(), tr("%1 joined the room.").arg(RsHtml::plainText(name)), ChatWidget::MSGTYPE_SYSTEM);
        emit peerJoined(id()) ;
        break;
    case RS_CHAT_LOBBY_EVENT_PEER_STATUS:
    {

        qsParticipant=gxs_id;

        ui.chatWidget->updateStatusString(RsHtml::plainText(name) + " %1", RsHtml::plainText(str));

        if (!isParticipantMuted(gxs_id))
            emit typingEventReceived(id()) ;

    }
        break;
    case RS_CHAT_LOBBY_EVENT_PEER_CHANGE_NICKNAME:
    {
        qsParticipant=gxs_id;

        QString newname= getParticipantName(RsGxsId(str.toStdString())) ;

        ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(),
                                  QDateTime::currentDateTime(),
                                  tr("%1 changed his name to: %2").arg(RsHtml::plainText(name)).arg(RsHtml::plainText(newname)),
                                  ChatWidget::MSGTYPE_SYSTEM);

        // TODO if a user was muted and changed his name, update mute list, but only, when the muted peer, dont change his name to a other peer in your chat lobby
        if (isParticipantMuted(gxs_id))
            muteParticipant(RsGxsId(str.toStdString())) ;
    }
        break;
    case RS_CHAT_LOBBY_EVENT_KEEP_ALIVE:
        //std::cerr << "Received keep alive packet from " << nickname.toStdString() << " in chat room " << getPeerId() << std::endl;
        break;
    default:
        std::cerr << "UnseenGxsChatLobbyDialog::displayLobbyEvent() Unhandled chat room event type " << event_type << std::endl;
    }

    if (!qsParticipant.isNull())
    {
        QList<QTreeWidgetItem*>  qlFoundParticipants=ui.participantsList->findItems(QString::fromStdString(qsParticipant.toStdString()),Qt::MatchExactly,COLUMN_ID);

        if (qlFoundParticipants.count()!=0)
        qlFoundParticipants.at(0)->setText(COLUMN_ACTIVITY,QString::number(time(NULL)));
    }

    updateParticipantsList() ;
}

bool UnseenGxsChatLobbyDialog::canClose()
{
    // check that the lobby still exists.
    /* TODO
    ChatLobbyId lid;
    if (!rsMsgs->isLobbyId(getPeerId(), lid)) {
        return true;
    }
    */

    if (QMessageBox::Yes == QMessageBox::question(this, tr("Unsubscribe from chat room"), tr("Do you want to unsubscribe to this chat room?"), QMessageBox::Yes | QMessageBox::No)) {
        return true;
    }

    return false;
}

void UnseenGxsChatLobbyDialog::showDialog(uint chatflags)
{
    if (chatflags & RS_CHAT_FOCUS)
    {
        MainWindow::showWindow(MainWindow::GxsChats);
        dynamic_cast<UnseenGxsGroupFrameDialog*>(MainWindow::getPage(MainWindow::GxsChats))->setCurrentChatPage(this) ;
    }
}

void UnseenGxsChatLobbyDialog::sortParcipants()
{

    if (actionSortByActivity->isChecked()) {
        ui.participantsList->sortItems(COLUMN_ACTIVITY, Qt::DescendingOrder);
    } else if (actionSortByName->isChecked()) {
        ui.participantsList->sortItems(COLUMN_NAME, Qt::AscendingOrder);
    }

}

void UnseenGxsChatLobbyDialog::filterChanged(const QString& /*text*/)
{
    filterIds();
}

void UnseenGxsChatLobbyDialog::filterIds()
{
    int filterColumn = ui.filterLineEdit->currentFilter();
    QString text = ui.filterLineEdit->text();

    ui.participantsList->filterItems(filterColumn, text);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// ALL FROM RsGxsUpdateBroadcastWidget                                                       ////
//////////////////////////////////////////////////////////////////////////////////////////////////

void UnseenGxsChatLobbyDialog::fillComplete()
{
    mBase->fillComplete();
}

void UnseenGxsChatLobbyDialog::setUpdateWhenInvisible(bool update)
{
    mBase->setUpdateWhenInvisible(update);
}

const std::set<RsGxsGroupId> &UnseenGxsChatLobbyDialog::getGrpIds()
{
    return mBase->getGrpIds();
}

const std::set<TurtleRequestId>& UnseenGxsChatLobbyDialog::getSearchResults()
{
    return mBase->getSearchResults();
}
const std::set<RsGxsGroupId> &UnseenGxsChatLobbyDialog::getGrpIdsMeta()
{
    return mBase->getGrpIdsMeta();
}

void UnseenGxsChatLobbyDialog::getAllGrpIds(std::set<RsGxsGroupId> &grpIds)
{
    mBase->getAllGrpIds(grpIds);
}

const std::map<RsGxsGroupId, std::set<RsGxsMessageId> > &UnseenGxsChatLobbyDialog::getMsgIds()
{
    return mBase->getMsgIds();
}

const std::map<RsGxsGroupId, std::set<RsGxsMessageId> > &UnseenGxsChatLobbyDialog::getMsgIdsMeta()
{
    return mBase->getMsgIdsMeta();
}

void UnseenGxsChatLobbyDialog::getAllMsgIds(std::map<RsGxsGroupId, std::set<RsGxsMessageId> > &msgIds)
{
    mBase->getAllMsgIds(msgIds);
}

void UnseenGxsChatLobbyDialog::fillDisplay(bool complete)
{
    updateDisplay(complete);
    update(); // Qt flush
}

void UnseenGxsChatLobbyDialog::showEvent(QShowEvent *event)
{
    mBase->showEvent(event);
    QWidget::showEvent(event);
}


////////////////////////////////////////////////////////////////////////////////////////////
///     ALL  from GxsMessageFrameWidget
/////////////////////////////////////////////////////////////////////////////////////////////

const RsGxsGroupId &UnseenGxsChatLobbyDialog::groupId()
{
    return mGroupId;
}

bool UnseenGxsChatLobbyDialog::isLoading()
{
    return false;
}

bool UnseenGxsChatLobbyDialog::isWaiting()
{
//    if (mStateHelper->isLoading(mTokenTypeAcknowledgeReadStatus)) {
//        return true;
//    }

    return false;
}

void UnseenGxsChatLobbyDialog::setGroupId(const RsGxsGroupId &groupId)
{
    if (mGroupId == groupId  && !groupId.isNull())
        return;

    if(!groupId.isNull())
    {
        mAcknowledgeReadStatusToken = 0;
//        if (mStateHelper->isLoading(mTokenTypeAcknowledgeReadStatus)) {
//            mStateHelper->setLoading(mTokenTypeAcknowledgeReadStatus, false);

//            emit waitingChanged(this);
//        }

        mGroupId = groupId;
        groupIdChanged();
    }
    else
    {
        mGroupId.clear();
        //blank();	// clear the displayed data, because no group is selected.
    }
}

void UnseenGxsChatLobbyDialog::setAllMessagesRead(bool read)
{
    uint32_t token = 0;
    setAllMessagesReadDo(read, token);

    if (token) {
        /* Wait for acknowlegde of the token */
        mAcknowledgeReadStatusToken = token;
        mTokenQueue->queueRequest(mAcknowledgeReadStatusToken, 0, 0, mTokenTypeAcknowledgeReadStatus);
        //mStateHelper->setLoading(mTokenTypeAcknowledgeReadStatus, true);

        emit waitingChanged(this);
    }
}

void UnseenGxsChatLobbyDialog::GxsMessageFrameWidgetloadRequest(const TokenQueue *queue, const TokenRequest &req)
{
    if (queue == mTokenQueue)
    {
        if (req.mUserType == mTokenTypeAcknowledgeReadStatus) {
            if (mAcknowledgeReadStatusToken == req.mToken) {
                /* Set read status is finished */
                //mStateHelper->setLoading(mTokenTypeAcknowledgeReadStatus, false);

                emit waitingChanged(this);
            }
            return;
        }
    }

    std::cerr << "UnseenGxsChatLobbyDialog::GxsMessageFrameWidgetloadRequestloadRequest() ERROR: INVALID TYPE";
    std::cerr << std::endl;
}


////////////////////////////////////////////////////////////////////////////////////////////
///     ALL  from GxsUpdateBroadcastWidget
/////////////////////////////////////////////////////////////////////////////////////////////

//COPY from GxsUpdateBroadcastWidget
void sortGxsMsgChat(std::vector<RsGxsChatMsg> &list)
{
    std::sort(list.begin(), list.end(),
              [] (RsGxsChatMsg const& a, RsGxsChatMsg const& b)
    { return a.mMeta.mPublishTs < b.mMeta.mPublishTs; });
}

void UnseenGxsChatLobbyDialog::insertGxsChatPosts(std::vector<RsGxsChatMsg> &posts, GxsMessageFramePostThread2 *thread, bool related)
{
    if (related && thread) {
        std::cerr << "v::insertChannelPosts fill only related posts as thread is not possible" << std::endl;
        return;
    }

    int count = posts.size();
    int pos = 0;

    if (!thread) {
        //TODO: GUI
        //ui->feedWidget->setSortingEnabled(false);
    }

    // collect new versions of posts if any

#ifdef DEBUG_CHAT
    std::cerr << "Inserting chat posts" << std::endl;
    std::cerr << "After sorting..." << std::endl;
#endif

    //sortGxsMsgChat(posts);
    std::vector<uint32_t> new_versions ;
    for (uint32_t i=0;i<posts.size();++i)
    {
        if(posts[i].mMeta.mOrigMsgId == posts[i].mMeta.mMsgId)
            posts[i].mMeta.mOrigMsgId.clear();

#ifdef DEBUG_CHAT
        std::cerr << "  " << i << ": msg_id=" << posts[i].mMeta.mMsgId <<" : msg timestamp= " << posts[i].mMeta.mPublishTs << " : msg = " << posts[i].mMsg << std::endl;
#endif

        if(!posts[i].mMeta.mOrigMsgId.isNull())
            new_versions.push_back(i) ;

        //unseenp2p - try to add msg into chat content
        QDateTime sendTime = QDateTime::fromSecsSinceEpoch(posts[i].mMeta.mPublishTs);
        QDateTime recvTime = QDateTime::fromSecsSinceEpoch(posts[i].mMeta.mPublishTs);
        RsGxsId gxs_id = posts[i].mMeta.mAuthorId;
        QString mmsg = QString::fromUtf8(posts[i].mMsg.c_str());
        bool incomming = !rsIdentity->isOwnId(gxs_id);
        RsIdentityDetails details;
        //get nickname from member list instead of getIdDetails
        QString nickname = "Unknown";
        if (rsIdentity->getIdDetails(gxs_id, details))
        {
            nickname = QString::fromStdString(details.mNickname);
        }

        //try to show all files from post.files: if image, show thumbnail + link, if not, only show link

        std::list<RsGxsFile> fileList; // = posts[i].mFiles;
        std::list<RsGxsFile>::iterator fit;
        for(fit = posts[i].mFiles.begin(); fit != posts[i].mFiles.end(); ++fit)
        {
            RsGxsFile fi;
            fi.mName = fit->mName;
            fi.mSize = fit->mSize;
            fi.mHash = fit->mHash;
            fileList.push_back(fi);
        }
        //save all the msgId to set, to avoid the duplication when request/load or show:
        allDownloadedMsgs.insert(posts[i].mMeta.mMsgId);

        //uneenp2p - need to check if these are files sharing, need to show "Download" link
        //ui.chatWidget->addChatMsg(incomming, nickname, gxs_id, sendTime, recvTime, mmsg, ChatWidget::MSGTYPE_NORMAL);
        //Need to separate the HISTORY MSG and CURRENT MSG HERE: using timestamp:
        // to separate 2 msg type ChatWidget::MSGTYPE_HISTORY and ChatWidget::MSGTYPE_NORMAL
        // need to save the timestamp of the latest history msg,
        // if the current msg go here need to compare with that timestamp of latest history msg (latestHistoryMsgTimetamp)
        // at first the latestHistoryMsgTimetamp of every chat will be the timestamp of latest recent msg
        // if the sendTime (posts[i].mMeta.mPublishTs) < latestHistoryMsgTimetamp then it will be HISTORY msg
        // else it will be the current (NORMAL) msg
        ChatWidget::MsgType msgType;
        if (posts[i].mMeta.mPublishTs <= mLatestHistoryMsgTimestamp)
        {
            msgType = ChatWidget::MSGTYPE_HISTORY;
            recvTime = QDateTime::fromSecsSinceEpoch(posts[i].mMeta.mPublishTs);
        }
        else
        {
            msgType = ChatWidget::MSGTYPE_NORMAL;
            recvTime = QDateTime::currentDateTime();
        }

        ui.chatWidget->addChatMsg(incomming, nickname, posts[i], sendTime, recvTime, mmsg, msgType);
        emit gxsMessageReceived(posts[i], incomming, gxsChatId(groupId()), sendTime, nickname, mmsg) ;

        // This is a trick to translate HTML into text.
        QTextEdit editor;
        editor.setHtml(mmsg);
        QString notifyMsg = nickname + ": " + editor.toPlainText();

        if(incomming && msgType == ChatWidget::MSGTYPE_NORMAL)
        {
            if(notifyMsg.length() > 30)
                MainWindow::displayLobbySystrayMsg(tr("Group chat") + ": " + mGroupName, notifyMsg.left(30) + QString("..."));
            else
                MainWindow::displayLobbySystrayMsg(tr("Group chat") + ": " + mGroupName, notifyMsg);
        }

        updateParticipantsList();
    }

#ifdef DEBUG_CHAT
    std::cerr << "New versions: " << new_versions.size() << std::endl;
#endif

    if(!new_versions.empty())
    {
#ifdef DEBUG_CHAT
        std::cerr << "  New versions present. Replacing them..." << std::endl;
        std::cerr << "  Creating search map."  << std::endl;
#endif

        // make a quick search map
        std::map<RsGxsMessageId,uint32_t> search_map ;
        for (uint32_t i=0;i<posts.size();++i)
            search_map[posts[i].mMeta.mMsgId] = i ;

        for(uint32_t i=0;i<new_versions.size();++i)
        {
#ifdef DEBUG_CHAT
            std::cerr << "  Taking care of new version  at index " << new_versions[i] << std::endl;
#endif

            uint32_t current_index = new_versions[i] ;
            uint32_t source_index  = new_versions[i] ;
#ifdef DEBUG_CHAT
            RsGxsMessageId source_msg_id = posts[source_index].mMeta.mMsgId ;
#endif

            // What we do is everytime we find a replacement post, we climb up the replacement graph until we find the original post
            // (or the most recent version of it). When we reach this post, we replace it with the data of the source post.
            // In the mean time, all other posts have their MsgId cleared, so that the posts are removed from the list.

            //std::vector<uint32_t> versions ;
            std::map<RsGxsMessageId,uint32_t>::const_iterator vit ;

            while(search_map.end() != (vit=search_map.find(posts[current_index].mMeta.mOrigMsgId)))
            {
#ifdef DEBUG_CHAT
                std::cerr << "    post at index " << current_index << " replaces a post at position " << vit->second ;
#endif

                // Now replace the post only if the new versionis more recent. It may happen indeed that the same post has been corrected multiple
                // times. In this case, we only need to replace the post with the newest version

                //uint32_t prev_index = current_index ;
                current_index = vit->second ;

                if(posts[current_index].mMeta.mMsgId.isNull())	// This handles the branching situation where this post has been already erased. No need to go down further.
                {
#ifdef DEBUG_CHAT
                    std::cerr << "  already erased. Stopping." << std::endl;
#endif
                    break ;
                }

                if(posts[current_index].mMeta.mPublishTs < posts[source_index].mMeta.mPublishTs)
                {
#ifdef DEBUG_CHAT
                    std::cerr << " and is more recent => following" << std::endl;
#endif
                    for(std::set<RsGxsMessageId>::const_iterator itt(posts[current_index].mOlderVersions.begin());itt!=posts[current_index].mOlderVersions.end();++itt)
                        posts[source_index].mOlderVersions.insert(*itt);

                    posts[source_index].mOlderVersions.insert(posts[current_index].mMeta.mMsgId);
                    posts[current_index].mMeta.mMsgId.clear();	    // clear the msg Id so the post will be ignored
                }
#ifdef DEBUG_CHAT
                else
                    std::cerr << " but is older -> Stopping" << std::endl;
#endif
            }
        }
    }

#ifdef DEBUG_CHAT
    std::cerr << "Now adding posts..." << std::endl;
#endif

    for (std::vector<RsGxsChatMsg>::const_reverse_iterator it = posts.rbegin(); it != posts.rend(); ++it)
    {
#ifdef DEBUG_CHAT
        std::cerr << "  adding post: " << (*it).mMeta.mMsgId ;
#endif

        if(!(*it).mMeta.mMsgId.isNull())
        {
#ifdef DEBUG_CHAT
            std::cerr << " added" << std::endl;
#endif

            if (thread && thread->stopped())
                break;

            if (thread)
                thread->emitAddPost(qVariantFromValue(*it), related, ++pos, count);
            else
                createPostItem(*it, related);
        }
#ifdef DEBUG_CHAT
        else
            std::cerr << " skipped" << std::endl;
#endif
    }

    if (!thread) {
        //TODO: GUI
        //ui->feedWidget->setSortingEnabled(true);
    }
}

void UnseenGxsChatLobbyDialog::insertAllPosts(const uint32_t &token, GxsMessageFramePostThread2 *thread)
{
    std::vector<RsGxsChatMsg> posts;
    rsGxsChats->getPostData(token, posts);

    insertGxsChatPosts(posts, thread, false);
}


void UnseenGxsChatLobbyDialog::updateDisplay(bool complete)
{
    if (complete) {
        /* Fill complete */ // --> Fill complete here mean: load all messages!
        //fix the duplication of show all messages
        if (!showAllPostOnlyOnce)   //we get all msg only once, the next time will not show all again!
        {
            requestGroupData();
            requestAllPosts(); // get all messages here!!! --> Here we can get only 25 recent msgs
            showAllPostOnlyOnce = true;
        }

        return;
    }

    if (groupId().isNull()) {
        return;
    }

    bool updateGroup = false;
    const std::set<RsGxsGroupId> &grpIdsMeta = getGrpIdsMeta();

    if(grpIdsMeta.find(groupId())!=grpIdsMeta.end())
        updateGroup = true;

    const std::set<RsGxsGroupId> &grpIds = getGrpIds();
    if (!groupId().isNull() && grpIds.find(groupId())!=grpIds.end())
    {
        updateGroup = true;
        /* Do we need to fill all posts? */
        if (!showAllPostOnlyOnce)   //we get all msg only once, the next time will not show all again!
        {
            requestAllPosts();
            showAllPostOnlyOnce = true;
        }
    } else {
        std::map<RsGxsGroupId, std::set<RsGxsMessageId> > msgs;
        getAllMsgIds(msgs);
        if (!msgs.empty())
        {
            std::map<RsGxsGroupId, std::set<RsGxsMessageId> >::iterator mit2 = msgs.find(groupId());

            if (mit2 != msgs.end())
            {

                std::set<RsGxsMessageId> notReceiveSet;
                 notReceiveSet.clear();
                if(mit2->second.size() > 0)
                {
                    std::set<RsGxsMessageId>::iterator msgIdIt;
                    for (msgIdIt = mit2->second.begin(); msgIdIt != mit2->second.end(); ++msgIdIt)
                    {
                        if (allDownloadedMsgs.find(*msgIdIt) == allDownloadedMsgs.end())
                        {
                            notReceiveSet.insert(*msgIdIt);
                        }
                    }
                }
                if (notReceiveSet.size() == 0)
                {
                    return;
                }
                else
                {
                    requestPosts(notReceiveSet);
                    notReceiveSet.clear();
                }


            }
//            auto mit = msgs.find(groupId());
//            std::set<RsGxsMessageId> &msgIdV = mit->second;
//            if (mit != msgs.end())
//            {
//                //std::set<RsGxsMessageId> &msgIdV = mit->second;

//                if(msgIdV.size() > 0)
//                {
//                    std::set<RsGxsMessageId>::iterator msgIdIt;
//                    for (msgIdIt = msgIdV.begin(); msgIdIt != msgIdV.end(); ++msgIdIt)
//                    {
//                        if (allDownloadedMsgs.find(*msgIdIt) != allDownloadedMsgs.end())
//                        {
//                            mit->second.erase(*msgIdIt);
//                        }
//                    }
//                }
//                if(msgIdV.size() > 0) requestPosts(msgIdV);
//                    //requestPosts(mit->second);
//                else return;
//            }
        }
    }

    if (updateGroup) {
        requestGroupData();
    }
}



//COPY/MOVE from GxsMessageFramePostWidget (or GxsChannelPostsWidget that inherit from this class (GxsMessageFramePostWidget))
/**************************************************************/
/** Request / Response of Data ********************************/
/**************************************************************/

bool UnseenGxsChatLobbyDialog::navigate(const RsGxsMessageId &msgId)
{
    if (msgId.isNull()) {
        return false;
    }

//    if (mStateHelper->isLoading(mTokenTypeAllPosts) || mStateHelper->isLoading(mTokenTypePosts)) {
//        mNavigatePendingMsgId = msgId;

//        /* No information if group is available */
//        return true;
//    }

    return navigatePostItem(msgId);
}

void UnseenGxsChatLobbyDialog::requestGroupData()
{
#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::requestGroupData()";
    std::cerr << std::endl;
#endif

    mSubscribeFlags = 0;

    mTokenQueue->cancelActiveRequestTokens(mTokenTypeGroupData);

    if (groupId().isNull()) {
//        mStateHelper->setActive(mTokenTypeGroupData, false);
//        mStateHelper->setLoading(mTokenTypeGroupData, false);
//        mStateHelper->clear(mTokenTypeGroupData);

        mGroupName.clear();

        groupNameChanged(mGroupName);

        emit groupChanged(this);

        return;
    }

    //mStateHelper->setLoading(mTokenTypeGroupData, true);

    emit groupChanged(this);

    std::list<RsGxsGroupId> groupIds;
    groupIds.push_back(groupId());

    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_GROUP_DATA;

    uint32_t token;
    mTokenQueue->requestGroupInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, groupIds, mTokenTypeGroupData);
}

void UnseenGxsChatLobbyDialog::loadGroupData(const uint32_t &token)
{
#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::loadGroupData()";
    std::cerr << std::endl;
#endif

    RsGroupMetaData metaData;
    bool ok = insertGroupData(token, metaData);

    //mStateHelper->setLoading(mTokenTypeGroupData, false);

    if (ok) {
        mSubscribeFlags = metaData.mSubscribeFlags;

        mGroupName = QString::fromUtf8(metaData.mGroupName.c_str());
        groupNameChanged(mGroupName);
        //need to update groupname here:
        getChatWidget()->setTitle(mGroupName);
    } else {
        std::cerr << "UnseenGxsChatLobbyDialog::loadGroupData() ERROR Not just one Group";
        std::cerr << std::endl;

        //mStateHelper->clear(mTokenTypeGroupData);

        mGroupName.clear();
        groupNameChanged(mGroupName);
    }

    //mStateHelper->setActive(mTokenTypeGroupData, ok);
    emit groupChanged(this);
}

void UnseenGxsChatLobbyDialog::requestAllPosts()
{
#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::requestAllPosts()";
    std::cerr << std::endl;
#endif

    mNavigatePendingMsgId.clear();

    /* Request all posts */

    mTokenQueue->cancelActiveRequestTokens(mTokenTypeAllPosts);

    if (mFillThread) {
        /* Stop current fill thread */
        GxsMessageFramePostThread2 *thread = mFillThread;
        mFillThread = NULL;
        thread->stop(false);

        //mStateHelper->setLoading(mTokenTypeAllPosts, false);
    }

    //clearPosts();

    if (groupId().isNull()) {
//        mStateHelper->setActive(mTokenTypeAllPosts, false);
//        mStateHelper->setLoading(mTokenTypeAllPosts, false);
//        mStateHelper->clear(mTokenTypeAllPosts);

        emit groupChanged(this);
        return;
    }

    //mStateHelper->setLoading(mTokenTypeAllPosts, true);
    emit groupChanged(this);

    std::list<RsGxsGroupId> groupIds;
    groupIds.push_back(groupId());

    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_MSG_DATA;
    opts.page=1;
    uint32_t token;
    mTokenQueue->requestMsgInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, groupIds, mTokenTypeAllPosts);
}

void UnseenGxsChatLobbyDialog::loadAllPosts(const uint32_t &token)
{
#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::loadAllPosts()";
    std::cerr << std::endl;
#endif

    //mStateHelper->setActive(mTokenTypeAllPosts, true);

    if (useThread()) {
        /* Create fill thread */
        mFillThread = new GxsMessageFramePostThread2(token, this);

        // connect thread
        connect(mFillThread, SIGNAL(finished()), this, SLOT(fillThreadFinished()), Qt::BlockingQueuedConnection);
        connect(mFillThread, SIGNAL(addPost(QVariant,bool,int,int)), this, SLOT(fillThreadAddPost(QVariant,bool,int,int)), Qt::BlockingQueuedConnection);

#ifdef ENABLE_DEBUG
        std::cerr << "UnseenGxsChatLobbyDialog::loadAllPosts() Start fill thread" << std::endl;
#endif

        /* Start thread */
        mFillThread->start();
    } else {
        insertAllPosts(token, NULL);

        //mStateHelper->setLoading(mTokenTypeAllPosts, false);

        if (!mNavigatePendingMsgId.isNull()) {
            navigate(mNavigatePendingMsgId);

            mNavigatePendingMsgId.clear();
        }
    }

    emit groupChanged(this);
}

void UnseenGxsChatLobbyDialog::requestPosts(const std::set<RsGxsMessageId> &msgIds)
{
#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::requestPosts()";
    std::cerr << std::endl;
#endif

    mNavigatePendingMsgId.clear();

    mTokenQueue->cancelActiveRequestTokens(mTokenTypePosts);

    if (groupId().isNull()) {
//        mStateHelper->setActive(mTokenTypePosts, false);
//        mStateHelper->setLoading(mTokenTypePosts, false);
//        mStateHelper->clear(mTokenTypePosts);
        emit groupChanged(this);
        return;
    }

    if (msgIds.empty()) {
        return;
    }

   // mStateHelper->setLoading(mTokenTypePosts, true);
    emit groupChanged(this);

    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_MSG_DATA;

    uint32_t token;
    GxsMsgReq requestMsgIds;
    requestMsgIds[groupId()] = msgIds;
    mTokenQueue->requestMsgInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, requestMsgIds, mTokenTypePosts);
}

void UnseenGxsChatLobbyDialog::loadPosts(const uint32_t &token)
{
#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::loadPosts()";
    std::cerr << std::endl;
#endif

   // mStateHelper->setActive(mTokenTypePosts, true);

    insertPosts(token);

    //mStateHelper->setLoading(mTokenTypePosts, false);
    emit groupChanged(this);

    if (!mNavigatePendingMsgId.isNull()) {
        navigate(mNavigatePendingMsgId);

        mNavigatePendingMsgId.clear();
    }
}

void UnseenGxsChatLobbyDialog::loadRequest(const TokenQueue *queue, const TokenRequest &req)
{
#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::loadRequest() UserType: " << req.mUserType;
    std::cerr << std::endl;
#endif

    if (queue == mTokenQueue)
    {
        if (req.mUserType == mTokenTypeGroupData) {
            loadGroupData(req.mToken);
            return;
        }

        if (req.mUserType == mTokenTypeAllPosts) {
            //loadAllPosts(req.mToken);
            std::vector<RsGxsChatMsg> msgs;
            std::list<RsGxsGroupId> list;
            list.push_back(groupId());
            rsGxsChats->getChatsContent(list, msgs, 1);
            //Here we can set the latestHistoryMsgTimestamp for msgs[0]

            // show the msg info here for checking:
            std::cerr << "Here is 24 msgs we want to check: " << std::endl;

            for (uint32_t i=0;i<msgs.size();++i)
            {
                if(msgs[i].mMeta.mOrigMsgId == msgs[i].mMeta.mMsgId)
                    msgs[i].mMeta.mOrigMsgId.clear();

        #ifdef DEBUG_CHAT
                std::cerr << "  " << i << ": msg_id=" << msgs[i].mMeta.mMsgId <<" : msg timestamp= " << msgs[i].mMeta.mPublishTs << " : msg = " << msgs[i].mMsg << std::endl;
        #endif
            }

            //sortGxsMsgChat(msgs);
            if (msgs.size()> 0)
                mLatestHistoryMsgTimestamp = msgs[msgs.size() - 1].mMeta.mPublishTs;
            std::cerr <<  "mLatestHistoryMsgTimestamp: " << mLatestHistoryMsgTimestamp << std::endl;
            insertGxsChatPosts(msgs, NULL, true);

            return;
        }

        if (req.mUserType == mTokenTypePosts) {
            loadPosts(req.mToken);
            return;
        }
    }
    GxsMessageFrameWidgetloadRequest(queue, req);
}

void UnseenGxsChatLobbyDialog::groupIdChanged()
{
    mGroupName = groupId().isNull () ? "" : tr("Loading");
    groupNameChanged(mGroupName);

    emit groupChanged(this);

    fillComplete();
}


void UnseenGxsChatLobbyDialog::groupNameChanged(const QString &name)
{
    std::cerr << "groupNameChanged to " << name.toStdString() << std::endl;
//    if (groupId().isNull()) {
//        ui->nameLabel->setText(tr("No Channel Selected"));
//        ui->logoLabel->setPixmap(QPixmap(":/images/channels.png"));
//    } else {
//        ui->nameLabel->setText(name);
//    }
}

bool UnseenGxsChatLobbyDialog::navigatePostItem(const RsGxsMessageId &msgId)
{
    return true;
//    FeedItem *feedItem = ui->feedWidget->findGxsFeedItem(groupId(), msgId);
//    if (!feedItem) {
//        return false;
//    }

    //return ui->feedWidget->scrollTo(feedItem, true);
}

void UnseenGxsChatLobbyDialog::fillThreadAddPost(const QVariant &post, bool related, int current, int count)
{
    if (sender() == mFillThread) {
        fillThreadCreatePost(post, related, current, count);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////
///     ALL  from highest GxsChatPostsWidget
/////////////////////////////////////////////////////////////////////////////////////////////
//copy from highest: GxsChatPostsWidget

void UnseenGxsChatLobbyDialog::insertPosts(const uint32_t &token)
{
    std::vector<RsGxsChatMsg> posts;
    rsGxsChats->getPostData(token, posts);

    insertGxsChatPosts(posts, NULL, true);
}

void UnseenGxsChatLobbyDialog::setViewMode(int viewMode)
{
    return;
}
void UnseenGxsChatLobbyDialog::createPostItem(const RsGxsChatMsg& post, bool related)
{
    GxsChatPostItem *item = NULL;
    return;

}
void UnseenGxsChatLobbyDialog::filterChanged(int filter)
{
   return;
}

void UnseenGxsChatLobbyDialog::subscribeGroup(bool subscribe)
{
    if (groupId().isNull()) {
        return;
    }

    uint32_t token;
    rsGxsChats->subscribeToGroup(token, groupId(), subscribe);
//	mChannelQueue->queueRequest(token, 0, RS_TOKREQ_ANSTYPE_ACK, TOKEN_TYPE_SUBSCRIBE_CHANGE);
}

bool UnseenGxsChatLobbyDialog::insertGroupData(const uint32_t &token, RsGroupMetaData &metaData)
{
    std::vector<RsGxsChatGroup> groups;
    rsGxsChats->getGroupData(token, groups);

    if(groups.size() == 1)
    {
        insertChannelDetails(groups[0]);
        metaData = groups[0].mMeta;
        return true;
    }
    else
    {
        RsGxsChatGroup distant_group;
        if(rsGxsChats->retrieveDistantGroup(groupId(),distant_group))
        {
            insertChannelDetails(distant_group);
            metaData = distant_group.mMeta;
            return true ;
        }
    }

    return false;
}

void UnseenGxsChatLobbyDialog::insertChannelDetails(const RsGxsChatGroup &group)
{
    /* IMAGE */
    QPixmap chanImage;
    if (group.mImage.mData != NULL) {
        chanImage.loadFromData(group.mImage.mData, group.mImage.mSize, "PNG");
    } else {
//        chanImage = QPixmap(CHAN_DEFAULT_IMAGE);
    }
//    ui->logoLabel->setPixmap(chanImage);

//    ui->subscribersLabel->setText(QString::number(group.mMeta.mPop)) ;

//    if (group.mMeta.mSubscribeFlags & GXS_SERV::GROUP_SUBSCRIBE_PUBLISH)
//    {
//        mStateHelper->setWidgetEnabled(ui->postButton, true);
//    }
//    else
//    {
//        mStateHelper->setWidgetEnabled(ui->postButton, false);
//    }

//    ui->subscribeToolButton->setSubscribed(IS_GROUP_SUBSCRIBED(group.mMeta.mSubscribeFlags));

    bool autoDownload ;
            rsGxsChats->getChannelAutoDownload(group.mMeta.mGroupId,autoDownload);

    setAutoDownload(true);

//    if (IS_GROUP_SUBSCRIBED(group.mMeta.mSubscribeFlags)) {
//        ui->feedToolButton->setEnabled(true);

//        ui->fileToolButton->setEnabled(true);
//        ui->infoWidget->hide();
//        setViewMode(viewMode());

//        ui->infoPosts->clear();
//        ui->infoDescription->clear();
//    } else {
//        ui->infoPosts->setText(QString::number(group.mMeta.mVisibleMsgCount));
//        if(group.mMeta.mLastPost==0)
//            ui->infoLastPost->setText(tr("Never"));
//        else
//            ui->infoLastPost->setText(DateTime::formatLongDateTime(group.mMeta.mLastPost));
//        ui->infoDescription->setText(QString::fromUtf8(group.mDescription.c_str()));

//            ui->infoAdministrator->setId(group.mMeta.mAuthorId) ;

//            QString distrib_string ( "[unknown]" );

//            switch(group.mMeta.mCircleType)
//        {
//        case GXS_CIRCLE_TYPE_PUBLIC: distrib_string = tr("Public") ;
//            break ;
//        case GXS_CIRCLE_TYPE_EXTERNAL:
//        {
//            RsGxsCircleDetails det ;

//            // !! What we need here is some sort of CircleLabel, which loads the circle and updates the label when done.

//            if(rsGxsCircles->getCircleDetails(group.mMeta.mCircleId,det))
//                distrib_string = tr("Restricted to members of circle \"")+QString::fromUtf8(det.mCircleName.c_str()) +"\"";
//            else
//                distrib_string = tr("Restricted to members of circle ")+QString::fromStdString(group.mMeta.mCircleId.toStdString()) ;
//        }
//            break ;
//        case GXS_CIRCLE_TYPE_YOUR_EYES_ONLY: distrib_string = tr("Your eyes only");
//            break ;
//        case GXS_CIRCLE_TYPE_LOCAL: distrib_string = tr("You and your friend nodes");
//            break ;
//        default:
//            std::cerr << "(EE) badly initialised group distribution ID = " << group.mMeta.mCircleType << std::endl;
//        }

//        ui->infoDistribution->setText(distrib_string);

//        ui->infoWidget->show();
//        ui->feedWidget->hide();
//        ui->fileWidget->hide();

//        ui->feedToolButton->setEnabled(false);
//        ui->fileToolButton->setEnabled(false);
//    }
}

void UnseenGxsChatLobbyDialog::settingsChanged()
{
    mUseThread = Settings->getChatsLoadThread();

    //mStateHelper->setWidgetVisible(ui->progressBar, mUseThread);
}

void UnseenGxsChatLobbyDialog::fillThreadCreatePost(const QVariant &post, bool related, int current, int count)
{
    /* show fill progress */
    if (count) {
        //ui->progressBar->setValue(current * ui->progressBar->maximum() / count);
    }

    if (!post.canConvert<RsGxsChatMsg>()) {
        return;
    }

    createPostItem(post.value<RsGxsChatMsg>(), related);
}
void UnseenGxsChatLobbyDialog::fillThreadFinished()
{
#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::fillThreadFinished()" << std::endl;
#endif

    /* Thread has finished */
    GxsMessageFramePostThread2 *thread = dynamic_cast<GxsMessageFramePostThread2*>(sender());
    if (thread) {
        if (thread == mFillThread) {
            /* Current thread has finished */
            mFillThread = NULL;

            //mStateHelper->setLoading(mTokenTypeAllPosts, false);
            emit groupChanged(this);

            if (!mNavigatePendingMsgId.isNull()) {
                navigate(mNavigatePendingMsgId);

                mNavigatePendingMsgId.clear();
            }
        }

#ifdef ENABLE_DEBUG
        if (thread->stopped()) {
            // thread was stopped
            std::cerr << "UnseenGxsChatLobbyDialog::fillThreadFinished() Thread was stopped" << std::endl;
        }
#endif

#ifdef ENABLE_DEBUG
        std::cerr << "UnseenGxsChatLobbyDialog::fillThreadFinished() Delete thread" << std::endl;
#endif

        thread->deleteLater();
        thread = NULL;
    }

#ifdef ENABLE_DEBUG
    std::cerr << "UnseenGxsChatLobbyDialog::fillThreadFinished done()" << std::endl;
#endif
}

void UnseenGxsChatLobbyDialog::toggleAutoDownload()
{
    RsGxsGroupId grpId = groupId();
    if (grpId.isNull()) {
        return;
    }

    bool autoDownload ;
        if(!rsGxsChats->getChannelAutoDownload(grpId,autoDownload) || !rsGxsChats->setChannelAutoDownload(grpId, !autoDownload))
    {
        std::cerr << "GxsChatDialog::toggleAutoDownload() Auto Download failed to set";
        std::cerr << std::endl;
    }
}

class GxsChatPostsReadData
{
public:
    GxsChatPostsReadData(bool read)
    {
        mRead = read;
        mLastToken = 0;
    }

public:
    bool mRead;
    uint32_t mLastToken;
};

//static void setAllMessagesReadCallback(FeedItem *feedItem, void *data)
//{
//    GxsChatPostItem *chatPostItem = dynamic_cast<GxsChatPostItem*>(feedItem);
//    if (!chatPostItem) {
//        return;
//    }

//    GxsChatPostsReadData *readData = (GxsChatPostsReadData*) data;
//    bool is_not_new = !chatPostItem->isUnread() ;

//    if(is_not_new == readData->mRead)
//        return ;

//    RsGxsGrpMsgIdPair msgPair = std::make_pair(chatPostItem->groupId(), chatPostItem->messageId());
//    rsGxsChats->setMessageReadStatus(readData->mLastToken, msgPair, readData->mRead);
//}

void UnseenGxsChatLobbyDialog::setAllMessagesReadDo(bool read, uint32_t &token)
{
    if (groupId().isNull() || !IS_GROUP_SUBSCRIBED(subscribeFlags())) {
        return;
    }

    GxsChatPostsReadData data(read);
    //ui->feedWidget->withAll(setAllMessagesReadCallback, &data);

    token = data.mLastToken;

    //continue from the GxsMessageFrameWidget::setAllMessagesReadDo
    if (token) {
        /* Wait for acknowlegde of the token */
        mAcknowledgeReadStatusToken = token;
        mTokenQueue->queueRequest(mAcknowledgeReadStatusToken, 0, 0, mTokenTypeAcknowledgeReadStatus);
        //mStateHelper->setLoading(mTokenTypeAcknowledgeReadStatus, true);

        emit waitingChanged(this);
    }
}

void UnseenGxsChatLobbyDialog::createMsg()
{
    if (groupId().isNull()) {
        return;
    }

    if (!IS_GROUP_SUBSCRIBED(subscribeFlags())) {
        return;
    }

    CreateGxsChatMsg *msgDialog = new CreateGxsChatMsg(groupId());
    msgDialog->show();

    /* window will destroy itself! */
}
void UnseenGxsChatLobbyDialog::setAutoDownload(bool autoDl)
{
//    mAutoDownloadAction->setChecked(autoDl);
//    mAutoDownloadAction->setText(autoDl ? tr("Disable Auto-Download") : tr("Enable Auto-Download"));
}


////////////////////////////////////////////////////////////////////////////////////////////
///     ALL  from  GxsMessageFramePostThread
/////////////////////////////////////////////////////////////////////////////////////////////
//copy from GxsMessageFramePostThread
GxsMessageFramePostThread2::GxsMessageFramePostThread2(uint32_t token, UnseenGxsChatLobbyDialog *parent)
    : QThread(parent), mToken(token), mParent(parent)
{
    mStopped = false;
}

GxsMessageFramePostThread2::~GxsMessageFramePostThread2()
{
#ifdef ENABLE_DEBUG
    std::cerr << "GxsMessageFramePostThread::~GxsMessageFramePostThread" << std::endl;
#endif
}

void GxsMessageFramePostThread2::stop(bool waitForStop)
{
    if (waitForStop) {
        disconnect();
    }

    mStopped = true;
    QApplication::processEvents();

    if (waitForStop) {
        wait();
    }
}

void GxsMessageFramePostThread2::run()
{
#ifdef ENABLE_DEBUG
    std::cerr << "GxsMessageFramePostThread2::run()" << std::endl;
#endif

    mParent->insertAllPosts(mToken, this);

#ifdef ENABLE_DEBUG
    std::cerr << "GxsMessageFramePostThread2::run() stopped: " << (stopped() ? "yes" : "no") << std::endl;
#endif
}

void GxsMessageFramePostThread2::emitAddPost(const QVariant &post, bool related, int current, int count)
{
    emit addPost(post, related, current, count);
}
