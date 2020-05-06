
#include "ChatLobbyWidget.h"
#include "ui_ChatLobbyWidget.h"

#include "notifyqt.h"
#include "chat/ChatLobbyDialog.h"
#include "chat/ChatLobbyUserNotify.h"
#include "chat/ChatTabWidget.h"
#include "connect/ConnectFriendWizard.h"
#include "chat/CreateLobbyDialog.h"
#include "common/RSTreeWidgetItem.h"
#include "gui/RetroShareLink.h"
#include "gui/gxs/GxsIdDetails.h"
#include "gui/Identity/IdEditDialog.h"
#include "gui/settings/rsharesettings.h"
#include "util/HandleRichText.h"
#include "util/QtVersion.h"

//meiyousixin - add more PopupChatDialog into ChatLobbyWidget
#include "chat/PopupChatDialog.h"
#include "gui/common/AvatarDefs.h"
#include "gui/common/StatusDefs.h"

//unseenp2p - using SmartListView and SmartListModel
#include "gui/conversationitemdelegate.h"

#include "gui/MainWindow.h"


#include <QPainter>
#include "util/rsdir.h"
#include "retroshare/rsinit.h"
#include <stdio.h>
#include <retroshare/rshistory.h>


#include "retroshare/rsmsgs.h"
#include "retroshare/rspeers.h"
#include "retroshare/rsnotify.h"
#include "retroshare/rsidentity.h"

#include <QGridLayout>
#include <QMenu>
#include <QMessageBox>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QPainter>
#include <algorithm>
#include <time.h>

//#define CHAT_LOBBY_GUI_DEBUG 1

#define COLUMN_NAME       0
#define COLUMN_RECENT_TIME      1
#define COLUMN_DATA       0

#define ROLE_SORT          Qt::UserRole
#define ROLE_ID            Qt::UserRole + 1
#define ROLE_SUBSCRIBED    Qt::UserRole + 2
#define ROLE_PRIVACYLEVEL  Qt::UserRole + 3
#define ROLE_AUTOSUBSCRIBE Qt::UserRole + 4
#define ROLE_FLAGS         Qt::UserRole + 5
#define ROLE_ONE2ONE       Qt::UserRole + 6 //meiyousixin - add for one2one


#define TYPE_FOLDER       0
#define TYPE_LOBBY        1
#define TYPE_ONE2ONE      2   //meiyousixin - add for one2ones

#define GUI_DIR_NAME                  "gui"
#define RECENT_CHAT_FILENAME          "recent_chat.txt"
#define UNREAD_CHAT_FILENAME          "unread_chat.txt"

ChatLobbyWidget::ChatLobbyWidget(QWidget *parent, Qt::WindowFlags flags)
  : RsAutoUpdatePage(5000, parent, flags)
{

    ui = new Ui::ChatLobbyWidget();
    ui->setupUi(this);

    getHistoryForRecentList();

	m_bProcessSettings = false;
	myChatLobbyUserNotify = NULL;
	myInviteYesButton = NULL;
	myInviteIdChooser = NULL;

	QObject::connect( NotifyQt::getInstance(), SIGNAL(lobbyListChanged()), SLOT(lobbyChanged()));
	QObject::connect( NotifyQt::getInstance(), SIGNAL(chatLobbyEvent(qulonglong,int,const RsGxsId&,const QString&)), this, SLOT(displayChatLobbyEvent(qulonglong,int,const RsGxsId&,const QString&)));
	QObject::connect( NotifyQt::getInstance(), SIGNAL(chatLobbyInviteReceived()), this, SLOT(readChatLobbyInvites()));

    QObject::connect( NotifyQt::getInstance(), SIGNAL(alreadySendChat(const ChatId&, std::string, long long, std::string, bool)), this, SLOT(updateRecentTime(const ChatId&, std::string, long long, std::string, bool)));
    QObject::connect( NotifyQt::getInstance(), SIGNAL(newChatMessageReceive(const ChatId&, std::string, long long, std::string, bool)), this, SLOT(updateRecentTime(const ChatId&, std::string, long long, std::string, bool)));

    QObject::connect( ui->lobbyTreeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(lobbyTreeWidgetCustomPopupMenu(QPoint)));
    QObject::connect( ui->filterLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(filterItems(QString)));
    QObject::connect( ui->filterLineEdit, SIGNAL(filterChanged(int)), this, SLOT(filterColumnChanged(int)));
    QObject::connect( ui->createLobbyToolButton, SIGNAL(clicked()), this, SLOT(createChatLobby()));
    QObject::connect(NotifyQt::getInstance(), SIGNAL(peerStatusChanged(QString,int)), this, SLOT(ContactStatusChanged(QString, int)));

    //Oct 17 2019 unseenp2p - setup SmartListView info
    if (!ui->lobbyTreeWidget->model()) {
        smartListModel_ = new SmartListModel("testing", this);
        ui->lobbyTreeWidget->setModel(smartListModel_);
        ui->lobbyTreeWidget->setItemDelegate(new ConversationItemDelegate());
        ui->lobbyTreeWidget->show();
    }

    // smartlist selection
    QObject::connect(ui->lobbyTreeWidget->selectionModel(),
        SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
        this,
        SLOT(smartListSelectionChanged(QItemSelection, QItemSelection)));

    ui->lobbyTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu) ;
    ui->lobbyTreeWidget->header()->hide();

    float fact = QFontMetricsF(font()).height()/14.0f;

	/** Setup the actions for the header context menu */
//    showUserCountAct= new QAction(headerItem->text(COLUMN_USER_COUNT),this);
//    showUserCountAct->setCheckable(true); showUserCountAct->setToolTip(tr("Show")+" "+showUserCountAct->text()+" "+tr("column"));
//    connect(showUserCountAct,SIGNAL(triggered(bool)),this,SLOT(setShowUserCountColumn(bool))) ;
//    showTopicAct= new QAction(headerItem->text(COLUMN_TOPIC),this);
//    showTopicAct->setCheckable(true); showTopicAct->setToolTip(tr("Show")+" "+showTopicAct->text()+" "+tr("column"));
//    connect(showTopicAct,SIGNAL(triggered(bool)),this,SLOT(setShowTopicColumn(bool))) ;
//    showSubscribeAct= new QAction(headerItem->text(COLUMN_SUBSCRIBED),this);
//    showSubscribeAct->setCheckable(true); showSubscribeAct->setToolTip(tr("Show")+" "+showSubscribeAct->text()+" "+tr("column"));
//    connect(showSubscribeAct,SIGNAL(triggered(bool)),this,SLOT(setShowSubscribeColumn(bool))) ;

	// Set initial size of the splitter
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setSizes(QList<int>() << 280*fact << width());
    ui->splitter->setCollapsible(0, false);
    ui->splitter->setCollapsible(1, false);
    //sort the conversation list
    rsMsgs->setConversationListMode(CONVERSATION_MODE_WITHOUT_FILTER);
    rsMsgs->sortConversationItemListByRecentTime();

	/* add filter actions */
    ui->filterLineEdit->setPlaceholderText("Search ");
    ui->filterLineEdit->showFilterIcon();
//    ui->filterLineEdit->addFilter(QIcon(), tr("Name"), COLUMN_NAME, tr("Search Name"));
//    ui->filterLineEdit->setCurrentFilter(COLUMN_NAME);

	// load settings
    processSettings(true);
    alreadyOpenLastChatWindow = false;
    receiveGroupInvite = false;
    showBlankPage(0) ;

    int S = QFontMetricsF(font()).height();
    QString help_str = tr("\
                          <h1><img width=\"%1\" src=\":/home/img/question-64.png\">&nbsp;&nbsp;Chat Rooms</h1>                              \
            <p>Chat rooms work pretty much like IRC.                                      \
            They allow you to talk anonymously with tons of people without the need to make friends.</p>                    \
            <p>A chat room can be public (your friends see it) or private (your friends can't see it, unless you           \
                                                                           invite them with <img src=\":/images/add_24x24.png\" width=%2/>). \
            Once you have been invited to a private room, you will be able to see it when your friends   \
                                                                           are using it.</p>                                                                                               \
                                                                           <p>The list at left shows                                                                                     \
                                                                           chat lobbies your friends are participating in. You can either                                 \
                                                                           <ul>                                                                                                            \
                                                                           <li>Right click to create a new chat room</li>                                                              \
                                                                           <li>Double click a chat room to enter, chat, and show it to your friends</li>                      \
                                                                           </ul> \
                                                                           Note: For the chat rooms to work properly, your computer needs be on time.  So check your system clock!\
                                                                           </p>                                      \
                                                                           "
                                                                           ).arg(QString::number(2*S)).arg(QString::number(S)) ;

            registerHelpButton(ui->helpButton,help_str,"ChatLobbyDialog") ;
}

ChatLobbyWidget::~ChatLobbyWidget()
{
    // save settings
    processSettings(false);
}

void ChatLobbyWidget::smartListSelectionChanged(const QItemSelection  &selected, const QItemSelection  &deselected)
{
    Q_UNUSED(deselected);
    QModelIndexList indices = selected.indexes();

    if (indices.isEmpty()) {
        return;
    }

    auto selectedIndex = indices.at(0);

    if (not selectedIndex.isValid()) {
        return;
    }

    selectConversation(selectedIndex);
}

void ChatLobbyWidget::selectConversation(const QModelIndex& index)
{

    //How to get the ConversationModel and get the index of it
    if (!index.isValid()) return;

    //std::vector<conversationInfo> list = rsMsgs->getConversationItemList();
    uint32_t conversationMode = rsMsgs->getConversationListMode();
    std::vector<conversationInfo> list; // = rsMsgs->getConversationItemList();
    if (conversationMode == CONVERSATION_MODE_WITHOUT_FILTER)
    {
        list = rsMsgs->getConversationItemList();
    }
    else if (conversationMode == CONVERSATION_MODE_WITH_SEARCH_FILTER)
    {
        list = rsMsgs->getSearchFilteredConversationItemList();
    }

    if (index.row() >= static_cast<int>(list.size())) return;
    conversationInfo chatItem = list.at(index.row());
    std::string uId = chatItem.uId;
    ChatId chatId;
    if (chatItem.contactType == 0)      //groupchat, show groupchat chat view
    {
        showGroupChatMVC(chatItem.chatLobbyId);
        chatId = ChatId(chatItem.chatLobbyId);
    }
    else                                //contact, show contact chat view
    {
        RsPeerId rsPeerId(chatItem.rsPeerIdStr);
        showContactChatMVC(chatItem.rsPeerIdStr);
        chatId = ChatId(rsPeerId);
    }

    lastChatId = chatId;

    //Need to check the unread msg, if this item has unread number, so call these functions
    if (chatItem.UnreadMessagesCount > 0)
    {
        //reset the unread number as 0 when user click on the conversation item on the left side
        rsMsgs->updateUnreadNumberOfItemInConversationList(chatItem.uId, 1, true);
        //when user click on the conversation item, just update the msg as read
        rsHistory->updateMessageAsRead(chatId);

        emit ui->lobbyTreeWidget->model()->layoutChanged();
    }

}

UserNotify *ChatLobbyWidget::getUserNotify(QObject *parent)
{
	if (!myChatLobbyUserNotify){
		myChatLobbyUserNotify = new ChatLobbyUserNotify(parent);
		connect(myChatLobbyUserNotify, SIGNAL(countChanged(ChatLobbyId, unsigned int)), this, SLOT(updateNotify(ChatLobbyId, unsigned int)));
        connect(myChatLobbyUserNotify, SIGNAL(countChangedFromP2P(ChatId, unsigned int)), this, SLOT(updateNotifyFromP2P(ChatId, unsigned int)));
	}
	return myChatLobbyUserNotify;
}

void ChatLobbyWidget::updateNotify(ChatLobbyId id, unsigned int count)
{
	ChatLobbyDialog *dialog=NULL;
	dialog=_lobby_infos[id].dialog;
	if(!dialog) return;

	QToolButton* notifyButton=dialog->getChatWidget()->getNotifyButton();
	if (!notifyButton) return;
	dialog->getChatWidget()->setNotify(myChatLobbyUserNotify);
	if (count>0){
		notifyButton->setVisible(true);
		//notifyButton->setIcon(_lobby_infos[id].default_icon);
		notifyButton->setToolTip(QString("(%1)").arg(count));
	} else {
		notifyButton->setVisible(false);
        ChatId chatId(id);
        rsHistory->updateMessageAsRead(chatId);
	}
}


void ChatLobbyWidget::updateNotifyFromP2P(ChatId id, unsigned int count)
{
    PopupChatDialog *dialog=NULL;
    dialog=_chatOne2One_infos[id.toPeerId().toStdString()].dialog;
    if(!dialog) return;

    QToolButton* notifyButton=dialog->getChatWidget()->getNotifyButton();
    if (!notifyButton) return;
    dialog->getChatWidget()->setNotify(myChatLobbyUserNotify);
    if (count>0){
        notifyButton->setVisible(true);
        //notifyButton->setIcon(_lobby_infos[id].default_icon);
        notifyButton->setToolTip(QString("(%1)").arg(count));
    } else {
        notifyButton->setVisible(false);

        //Need to update msg as read by updating the history
        rsHistory->updateMessageAsRead(id);
        //And need to update for all items on the left, return to their avatars
        std::set<ChatId>::iterator i;

        if (recentUnreadListOfChatId.size() > 0)
        {
            if (recentUnreadListOfChatId.count(id) != 0) recentUnreadListOfChatId.erase(id);
        }
    }
}

static bool trimAnonIds(std::list<RsGxsId>& lst)
{
    // trim down identities that are unsigned, because the lobby requires it.
    bool removed = false ;

    RsIdentityDetails idd ;

    for(std::list<RsGxsId>::iterator it = lst.begin();it!=lst.end();)
	    if(!rsIdentity->getIdDetails(*it,idd) || !(idd.mFlags & RS_IDENTITY_FLAGS_PGP_LINKED))
	    {
		    it = lst.erase(it) ;
		    removed= true ;
	    }
	    else
		    ++it ;

    return removed ;
}

void ChatLobbyWidget::lobbyTreeWidgetCustomPopupMenu(QPoint)
{
    return;
//	std::cerr << "Creating customPopupMennu" << std::endl;
//    QTreeWidgetItem *item = ui->lobbyTreeWidget->currentItem();
//    //QModelIndex itemIndex = ui->lobbyTreeWidget->currentIndex();

//	QMenu contextMnu(this);

//	if (item && item->type() == TYPE_FOLDER) {
//		QAction *action = contextMnu.addAction(QIcon(IMAGE_CREATE), tr("Create chat room"), this, SLOT(createChatLobby()));
//		action->setData(item->data(COLUMN_DATA, ROLE_PRIVACYLEVEL).toInt());
//	}

//        if (item && item->type() == TYPE_LOBBY)
//        {
//            std::list<RsGxsId> own_identities ;
//            rsIdentity->getOwnIds(own_identities) ;

//            if (item->data(COLUMN_DATA, ROLE_SUBSCRIBED).toBool())
//                contextMnu.addAction(QIcon(IMAGE_UNSUBSCRIBE), tr("Leave this room"), this, SLOT(unsubscribeItem()));
//            else
//            {
//                QTreeWidgetItem *item = ui->lobbyTreeWidget->currentItem();

//                //ChatLobbyId id = item->data(COLUMN_DATA, ROLE_ID).toULongLong();
//            ChatLobbyFlags flags(item->data(COLUMN_DATA, ROLE_FLAGS).toUInt());

//                bool removed = false ;
//                if(flags & RS_CHAT_LOBBY_FLAGS_PGP_SIGNED)
//                    removed = trimAnonIds(own_identities) ;

//                if(own_identities.empty())
//                {
//                    if(removed)
//                    contextMnu.addAction(QIcon(IMAGE_SUBSCRIBE), tr("Create a non anonymous identity and enter this room"), this, SLOT(createIdentityAndSubscribe()));
//                        else
//                    contextMnu.addAction(QIcon(IMAGE_SUBSCRIBE), tr("Create an identity and enter this chat room"), this, SLOT(createIdentityAndSubscribe()));
//                }
//                else if(own_identities.size() == 1)
//                {
//                    QAction *action = contextMnu.addAction(QIcon(IMAGE_SUBSCRIBE), tr("Enter this chat room"), this, SLOT(subscribeChatLobbyAs()));
//                    action->setData(QString::fromStdString((own_identities.front()).toStdString())) ;
//                }
//                else
//                {
//                    QMenu *mnu = contextMnu.addMenu(QIcon(IMAGE_SUBSCRIBE),tr("Enter this chat room as...")) ;

//                    for(std::list<RsGxsId>::const_iterator it=own_identities.begin();it!=own_identities.end();++it)
//                    {
//                        RsIdentityDetails idd ;
//                        rsIdentity->getIdDetails(*it,idd) ;

//                        QPixmap pixmap ;

//                        if(idd.mAvatar.mSize == 0 || !pixmap.loadFromData(idd.mAvatar.mData, idd.mAvatar.mSize, "PNG"))
//                            pixmap = QPixmap::fromImage(GxsIdDetails::makeDefaultIcon(*it)) ;

//                        QAction *action = mnu->addAction(QIcon(pixmap), QString("%1 (%2)").arg(QString::fromUtf8(idd.mNickname.c_str()), QString::fromStdString((*it).toStdString())), this, SLOT(subscribeChatLobbyAs()));
//                        action->setData(QString::fromStdString((*it).toStdString())) ;
//                    }
//                }
//            }

//            if (item->data(COLUMN_DATA, ROLE_AUTOSUBSCRIBE).toBool())
//                contextMnu.addAction(QIcon(IMAGE_AUTOSUBSCRIBE), tr("Remove Auto Subscribe"), this, SLOT(autoSubscribeItem()));
//            else if(!own_identities.empty())
//                contextMnu.addAction(QIcon(IMAGE_SUBSCRIBE), tr("Add Auto Subscribe"), this, SLOT(autoSubscribeItem()));
//            contextMnu.addAction(QIcon(IMAGE_COPYRSLINK), tr("Copy UnseenP2P Link"), this, SLOT(copyItemLink()));
//        }

//        contextMnu.addSeparator();//-------------------------------------------------------------------

////        showUserCountAct->setChecked(!ui->lobbyTreeWidget->isColumnHidden(COLUMN_USER_COUNT));
////        showTopicAct->setChecked(!ui->lobbyTreeWidget->isColumnHidden(COLUMN_TOPIC));
////        showSubscribeAct->setChecked(!ui->lobbyTreeWidget->isColumnHidden(COLUMN_SUBSCRIBED));

//        QMenu *menu = contextMnu.addMenu(tr("Columns"));
//        menu->addAction(showUserCountAct);
//        menu->addAction(showTopicAct);
//        menu->addAction(showSubscribeAct);

//        contextMnu.exec(QCursor::pos());
}

void ChatLobbyWidget::lobbyChanged()
{
	updateDisplay();
}

static void updateItem(QTreeWidget *treeWidget, QTreeWidgetItem *item, ChatLobbyId id, const std::string &name, const std::string &topic, int count, bool subscribed, bool autoSubscribe,ChatLobbyFlags lobby_flags)
{
	item->setText(COLUMN_NAME, QString::fromUtf8(name.c_str()));
	item->setData(COLUMN_NAME, ROLE_SORT, QString::fromUtf8(name.c_str()));

	item->setData(COLUMN_DATA, ROLE_ID, (qulonglong)id);
	item->setData(COLUMN_DATA, ROLE_FLAGS, lobby_flags.toUInt32());

	QColor color = treeWidget->palette().color(QPalette::Active, QPalette::Text);
    
//	for (int column = 0; column < COLUMN_COUNT; ++column) {
//		item->setTextColor(column, color);
//	}
    QString tooltipstr = QObject::tr("Group name:")+" "+item->text(COLUMN_NAME)+"\n"
                     +QObject::tr("Group type:")+" "+ (lobby_flags & RS_CHAT_LOBBY_FLAGS_PUBLIC? QObject::tr("Public group (Community group) "): QObject::tr("Private group (members only)"))+"\n"
                     //+QObject::tr("Participants:")+" "+QString::number(count)+"\n"
                     +QObject::tr("Id:")+" "+QString::number(id,16) ;
    
    if(lobby_flags & RS_CHAT_LOBBY_FLAGS_PGP_SIGNED)
    {
        tooltipstr += QObject::tr("\nSecurity: no anonymous IDs") ;
        QColor foreground = QColor(0, 128, 0); // green
//        for (int column = 0; column < COLUMN_COUNT; ++column)
//            item->setTextColor(column, foreground);
    }
    item->setToolTip(0,tooltipstr) ;
}

void ChatLobbyWidget::addChatPage(ChatLobbyDialog *d)
{
	// check that the page does not already exist. 

	if(_lobby_infos.find(d->id()) == _lobby_infos.end())
	{
        ChatLobbyId id = d->id();
        ChatLobbyInfo linfo;
        ui->stackedWidget->addWidget(d) ;

        connect(d,SIGNAL(lobbyLeave(ChatLobbyId)),this,SLOT(unsubscribeChatLobby(ChatLobbyId))) ;
        connect(d,SIGNAL(typingEventReceived(ChatLobbyId)),this,SLOT(updateTypingStatus(ChatLobbyId))) ;
        connect(d,SIGNAL(messageReceived(bool,ChatLobbyId,QDateTime,QString,QString)),this,SLOT(updateMessageChanged(bool,ChatLobbyId,QDateTime,QString,QString))) ;
        connect(d,SIGNAL(peerJoined(ChatLobbyId)),this,SLOT(updatePeerEntering(ChatLobbyId))) ;
        connect(d,SIGNAL(peerLeft(ChatLobbyId)),this,SLOT(updatePeerLeaving(ChatLobbyId))) ;


        _lobby_infos[id].dialog = d ;
        _lobby_infos[id].default_icon = QIcon() ;
        _lobby_infos[id].last_typing_event = time(NULL) ;

        //get the selected groupchat or contact
        QModelIndexList list = ui->lobbyTreeWidget->selectionModel()->selectedIndexes();
        std::string selectedUId;
        foreach (QModelIndex index, list)
        {
            if (index.row()!=-1)
            {
                selectedUId = rsMsgs->getSeletedUIdBeforeSorting(index.row());
                break;
            }
        }
        //get the groupchat info
        std::string uId = std::to_string(id);
        if (!rsMsgs->isChatIdInConversationList(uId))
        {
            uint current_time = QDateTime::currentDateTime().toTime_t();
            std::string groupname = "Unknown name";
            if(rsMsgs->getChatLobbyInfo(id,linfo))
            {
                groupname = linfo.lobby_name.c_str();
                bool is_private = !(linfo.lobby_flags & RS_CHAT_LOBBY_FLAGS_PUBLIC);
                int groupChatType = (is_private? 1:0);
                rsMsgs->saveContactOrGroupChatToModelData(groupname, "", 0, current_time, "", true, 0, groupChatType,"", id, uId );

                //after open new window and add the new conversation item, need to sort and update the layout
                rsMsgs->sortConversationItemListByRecentTime();
                emit ui->lobbyTreeWidget->model()->layoutChanged();
            }
        }
        //Here need to check whether this groupchat this member create or receive auto-accept group invite
        // If this is this member create so no need to clearSelection and re-select, if not, no need to do these following
        if (!receiveGroupInvite)
        {
            ui->lobbyTreeWidget->selectionModel()->clearSelection();
            // need to re-select the conversation item when we have new chat only
            int seletedrow = rsMsgs->getIndexFromUId(uId);
            QModelIndex idx = ui->lobbyTreeWidget->model()->index(seletedrow, 0);
            ui->lobbyTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
            emit ui->lobbyTreeWidget->model()->layoutChanged();
        }
        else
        {
            //if receive the auto-accept group-invite, so after sort conversation items, the selection will go wrong,
            // need to save the selected item and re-select again
            ui->lobbyTreeWidget->selectionModel()->clearSelection();
            int seletedrow = rsMsgs->getIndexFromUId(selectedUId);
            QModelIndex idx = ui->lobbyTreeWidget->model()->index(seletedrow, 0);
            ui->lobbyTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
            emit ui->lobbyTreeWidget->model()->layoutChanged();
        }
        if (receiveGroupInvite) receiveGroupInvite = false;
	}
}

void ChatLobbyWidget::addOne2OneChatPage(PopupChatDialog *d)
{
	// check that the page does not already exist.
    std::string uId = d->chatId().toPeerId().toStdString();
    if (_chatOne2One_infos.count(uId) < 1)
	{
        ui->stackedWidget->addWidget(d) ;
        connect(d,SIGNAL(messageP2PReceived(ChatMessage)),this,SLOT(updateP2PMessageChanged(ChatMessage))) ;
        _chatOne2One_infos[d->chatId().toPeerId().toStdString()].dialog = d ;

        //Need to check and add conversation item into the vector of Model-View data
        if (!rsMsgs->isChatIdInConversationList(uId))
        {
            uint current_time = QDateTime::currentDateTime().toTime_t();
            RsPgpId pgpId = rsPeers->getGPGId(d->chatId().toPeerId());
            std::string nickname = rsPeers->getGPGName(pgpId);
            rsMsgs->saveContactOrGroupChatToModelData(nickname, "", 0, current_time, "", true, 1, -1, uId, d->chatId().toLobbyId(), uId);
            _chatOne2One_infos[d->chatId().toPeerId().toStdString()].last_typing_event = current_time;

            //after open new window and add the new conversation item, need to sort and update the layout
            rsMsgs->sortConversationItemListByRecentTime();
        }
   }
    //for both case, if the item already exist in the conversation list, need to remove the current selection
    int seletedrow = rsMsgs->getIndexFromUId(uId);
    ui->lobbyTreeWidget->selectionModel()->clearSelection();
    emit ui->lobbyTreeWidget->model()->layoutChanged();

    // for both case, need to re-select the conversation item
    QModelIndex idx = ui->lobbyTreeWidget->model()->index(seletedrow, 0);
    ui->lobbyTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
    emit ui->lobbyTreeWidget->model()->layoutChanged();
    ui->lobbyTreeWidget->show();

}

void ChatLobbyWidget::setCurrentChatPage(ChatLobbyDialog *d)
{
    ui->stackedWidget->setCurrentWidget(d) ;

    if (d) {
        //re-select the left conversation item
        std::string uId = std::to_string(d->chatId().toLobbyId());
        int seletedrow = rsMsgs->getIndexFromUId(uId);
        if (seletedrow >= 0)
        {
            ui->lobbyTreeWidget->selectionModel()->clearSelection();
            emit ui->lobbyTreeWidget->model()->layoutChanged();

            //update again after clear seletion
            QModelIndex idx = ui->lobbyTreeWidget->model()->index(seletedrow, 0);
            ui->lobbyTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
            emit ui->lobbyTreeWidget->model()->layoutChanged();
        }
    }
}

void ChatLobbyWidget::setCurrentOne2OneChatPage(PopupChatDialog *d)
{
    ui->stackedWidget->setCurrentWidget(d) ;

    if (d) {
        //re-select the right conversation item
        std::string uId = d->chatId().toPeerId().toStdString();
        int seletedrow = rsMsgs->getIndexFromUId(uId);
        if (seletedrow >= 0)
        {
            ui->lobbyTreeWidget->selectionModel()->clearSelection();
            emit ui->lobbyTreeWidget->model()->layoutChanged();

            //update again after clear seletion
            QModelIndex idx = ui->lobbyTreeWidget->model()->index(seletedrow, 0);
            ui->lobbyTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
            emit ui->lobbyTreeWidget->model()->layoutChanged();
        }
    }
}

void ChatLobbyWidget::updateDisplay()
{
    //openLastChatWindow();

#ifdef CHAT_LOBBY_GUI_DEBUG
    std::cerr << "Show all group chats !" << std::endl;
    for(std::map<ChatLobbyId,ChatLobbyInfo>::const_iterator it(_groupchat_infos.begin());it!=_groupchat_infos.end();++it)
    {
        std::cerr << "Group Chat: " << it->second.lobby_name << std::endl;
    }
#endif

}

void ChatLobbyWidget::createChatLobby()
{
    int privacyLevel = 2; //meiyousixin - change to 2 - private as default , 1 - public
	QAction *action = qobject_cast<QAction*>(sender());
	if (action) {
		privacyLevel = action->data().toInt();
	}

	std::set<RsPeerId> friends;
	CreateLobbyDialog(friends, privacyLevel).exec();
}

void ChatLobbyWidget::showLobby(QTreeWidgetItem *item)
{
	if (item == NULL || item->type() != TYPE_LOBBY) {
		showBlankPage(0) ;
		return;
	}

	ChatLobbyId id = item->data(COLUMN_DATA, ROLE_ID).toULongLong();

	if(_lobby_infos.find(id) == _lobby_infos.end())
    {
        ChatDialog::chatFriend(ChatId(id),true) ;
        //showBlankPage(id) ;
    }
	else
        ui->stackedWidget->setCurrentWidget(_lobby_infos[id].dialog) ;

    //unseenp2p
    if (ChatLobbyDialog *cld = dynamic_cast<ChatLobbyDialog*>(ChatDialog::getExistingChat(ChatId(id)))) {
        cld->updateParticipantsList();
    }
}

void ChatLobbyWidget::showGroupChatMVC(ChatLobbyId lobbyId)
{

    //ChatLobbyId lobbyId = QString::fromStdString(chatIdStr).toULongLong();
    if(_lobby_infos.find(lobbyId) == _lobby_infos.end())
    {
        ChatDialog::chatFriend(ChatId(lobbyId),true) ;
        //showBlankPage(id) ;
    }
    else
        ui->stackedWidget->setCurrentWidget(_lobby_infos[lobbyId].dialog) ;

    //unseenp2p
    if (ChatLobbyDialog *cld = dynamic_cast<ChatLobbyDialog*>(ChatDialog::getExistingChat(ChatId(lobbyId)))) {
        cld->updateParticipantsList();
    }


}
// convert from RsPgpId to chatId
void ChatLobbyWidget::fromGpgIdToChatId(const RsPgpId &gpgId, ChatId &chatId)
{
  RsPeerDetails detail;
  if (rsPeers->getGPGDetails(gpgId, detail))
    {
          //let's get the ssl child details
          std::list<RsPeerId> sslIds;
          rsPeers->getAssociatedSSLIds(detail.gpg_id, sslIds);

          if (sslIds.size() == 1) {
                  // chat with the one ssl id (online or offline)
                  chatId = ChatId(sslIds.front());
                  return ;
          }

          // more than one ssl ids available, check for online
          std::list<RsPeerId> onlineIds;
          for (std::list<RsPeerId>::iterator it = sslIds.begin(); it != sslIds.end(); ++it) {
                  if (rsPeers->isOnline(*it)) {
                          onlineIds.push_back(*it);
                  }
          }

          if (onlineIds.size() == 1) {
                  // chat with the online ssl id
                  chatId = ChatId(onlineIds.front());
                  return ;
          }

  }
}

//22 Sep 2018 - meiyousixin - show contact chat like showLobby
void ChatLobbyWidget::showContactChat(QTreeWidgetItem *item)
{
	if (item == NULL || item->type() != TYPE_ONE2ONE) {
		return;
	}

	QString chatIdStr = item->data(COLUMN_DATA, ROLE_ID).toString();
    if(_chatOne2One_infos.count(chatIdStr.toStdString()) > 0)
    {
        ui->stackedWidget->setCurrentWidget(_chatOne2One_infos[chatIdStr.toStdString()].dialog) ;
    }
    else
    {
        //create chat dialog and add into this widget
        RsPeerId peerId(chatIdStr.toStdString());
        ChatId chatId(peerId);
        ChatDialog::chatFriend(chatId);
    }
}

//25 Oct 2019 - meiyousixin - using MVC for the chat item list
void ChatLobbyWidget::showContactChatMVC(std::string chatIdStr)
{

    if(_chatOne2One_infos.count(chatIdStr) > 0)
    {
        ui->stackedWidget->setCurrentWidget(_chatOne2One_infos[chatIdStr].dialog) ;
    }
    else
    {
        //create chat dialog and add into this widget
        RsPeerId peerId(chatIdStr);
        ChatId chatId(peerId);
        ChatDialog::chatFriend(chatId);
    }
}

unsigned int ChatLobbyWidget::getUnreadMsgNumberForChat(ChatId chatId)
{
    int messageCount = 21; // We only check 21 msg because we will show 20+ incase the unread number is big
    std::list<HistoryMsg> historyMsgs;
    rsHistory->getMessages(chatId, historyMsgs, messageCount);
    std::list<HistoryMsg>::reverse_iterator historyIt;

    unsigned int count = 0;
    for (historyIt = historyMsgs.rbegin(); historyIt != historyMsgs.rend(); ++historyIt)
    {
        if (historyIt->unread) count +=1;
        else
        {
            break;
        }
    }
    return  count;
}

// Sort recent list by history messages of all conversations
void ChatLobbyWidget::getHistoryForRecentList()
{
    int messageCount = 1;
    std::list<HistoryMsg> historyMsgs;
    if (messageCount > 0)
    {
        //get one history msg for all contacts and sort by recent time
        std::list<RsPeerId> friendList;
        rsPeers->getFriendList(friendList);
        std::list<RsPeerId>::const_iterator peerIt;
        for (peerIt = friendList.begin(); peerIt != friendList.end(); ++peerIt)
        {
            ChatId chatId(*peerIt);
            rsHistory->getMessages(chatId, historyMsgs, messageCount);
            std::list<HistoryMsg>::iterator historyIt;
            for (historyIt = historyMsgs.begin(); historyIt != historyMsgs.end(); ++historyIt)
            {
                RsPgpId pgpId = rsPeers->getGPGId(chatId.toPeerId());
                std::string nickname = rsPeers->getGPGName(pgpId);
                QDateTime datetimeForMsg = (historyIt->incoming ? QDateTime::fromTime_t(historyIt->recvTime) :QDateTime::fromTime_t(historyIt->sendTime)) ;
                uint lastMsgDatetime = (historyIt->incoming ? historyIt->recvTime :historyIt->sendTime) ;
                std::string LastInteractionDateStr = datetimeForMsg.toString().toStdString();
                int contactType = 1;
                int groupChatType = -1;
                std::string rsPeerIdStr = chatId.toPeerId().toStdString();
                std::string chatLobbyIdStr = "";

                unsigned int unreadMsgNumber = getUnreadMsgNumberForChat(chatId);
                rsMsgs->saveContactOrGroupChatToModelData(nickname, "",unreadMsgNumber, lastMsgDatetime, historyIt->message, (historyIt->incoming?true:false),
                                                                                                                         contactType, groupChatType, rsPeerIdStr, chatId.toLobbyId(), rsPeerIdStr );
            }
        }

        //get one history msg  for all group chats and sort by recent time
        std::list<HistoryMsg> historyGroupChatMsgs;
        rsMsgs->getGroupChatInfoList(_groupchat_infos);

        for(std::map<ChatLobbyId,ChatLobbyInfo>::const_iterator it(_groupchat_infos.begin());it!=_groupchat_infos.end();++it)
        {
            ChatId chatId(it->second.lobby_id);
            rsHistory->getMessages(chatId, historyGroupChatMsgs, messageCount);

            std::list<HistoryMsg>::iterator historyIt;
            for (historyIt = historyGroupChatMsgs.begin(); historyIt != historyGroupChatMsgs.end(); ++historyIt)
            {

#ifdef CHAT_LOBBY_GUI_DEBUG
                std::cerr << "History for group chat " <<  (*it).second.lobby_name <<"last msg is unread: " << historyIt->unread << std::endl;
#endif
                QDateTime datetimeForMsg = (historyIt->incoming ? QDateTime::fromTime_t(historyIt->recvTime) :QDateTime::fromTime_t(historyIt->sendTime)) ;
                std::string LastInteractionDateStr = datetimeForMsg.toString().toStdString();
                uint lastMsgDatetime = (historyIt->incoming ? historyIt->recvTime :historyIt->sendTime) ;
                int contactType = 0;
                bool is_private = !(it->second.lobby_flags & RS_CHAT_LOBBY_FLAGS_PUBLIC);
                int groupChatType = (is_private? 1:0);
                std::string chatLobbyIdStr = std::to_string(chatId.toLobbyId());
                std::string rsPeerIdStr ="";
                unsigned int unreadMsgNumber = getUnreadMsgNumberForChat(chatId);
                rsMsgs->saveContactOrGroupChatToModelData(it->second.lobby_name, historyIt->nickInGroupchat,
                                                           unreadMsgNumber, lastMsgDatetime, historyIt->message,(historyIt->incoming?true:false),
                                                           contactType, groupChatType, rsPeerIdStr, chatId.toLobbyId(), chatLobbyIdStr);
            }
        }


    }
}

// 22 Sep 2018 - meiyousixin - this function is for the case where we don't have any identity yet
void ChatLobbyWidget::createIdentityAndSubscribe()
{
//    QTreeWidgetItem *item = ui->lobbyTreeWidget->currentItem();

//    if(!item)
//        return ;

//    ChatLobbyId id = item->data(COLUMN_DATA, ROLE_ID).toULongLong();
//    ChatLobbyFlags flags(item->data(COLUMN_DATA, ROLE_FLAGS).toUInt());

//    IdEditDialog dlg(this);
//    dlg.setupNewId(false);
    
//    if(flags & RS_CHAT_LOBBY_FLAGS_PGP_SIGNED) //
//	    dlg.enforceNoAnonIds() ;
    
//    dlg.exec();
//    // fetch new id
//    std::list<RsGxsId> own_ids;
//    if(!rsIdentity->getOwnIds(own_ids) || own_ids.empty())
//        return;

//    if(rsMsgs->joinVisibleChatLobby(id,own_ids.front()))
//        ChatDialog::chatFriend(ChatId(id),true) ;
}

void ChatLobbyWidget::subscribeChatLobbyAs()
{
//    QTreeWidgetItem *item = ui->lobbyTreeWidget->currentItem();

//    if(!item)
//        return ;

//    ChatLobbyId id = item->data(COLUMN_DATA, ROLE_ID).toULongLong();

//    QAction *action = qobject_cast<QAction *>(QObject::sender());
//    if (!action)
//        return ;

//    RsGxsId gxs_id(action->data().toString().toStdString());
//        //uint32_t error_code ;

//    if(rsMsgs->joinVisibleChatLobby(id,gxs_id))
//        ChatDialog::chatFriend(ChatId(id),true) ;
}

bool ChatLobbyWidget::showLobbyAnchor(ChatLobbyId id, QString anchor)
{
//	QTreeWidgetItem *item = getTreeWidgetItem(id) ;

//	if(item != NULL) {
//        if(item->type() == TYPE_LOBBY) {

//            if(_lobby_infos.find(id) == _lobby_infos.end()) {
//				showBlankPage(id) ;
//			} else {
//				//ChatLobbyDialog cldChatLobby =_lobby_infos[id].dialog;
//				ui->stackedWidget->setCurrentWidget(_lobby_infos[id].dialog) ;
//				ChatLobbyDialog *cldCW=NULL ;
//				if (NULL != (cldCW = dynamic_cast<ChatLobbyDialog *>(ui->stackedWidget->currentWidget())))
//					cldCW->getChatWidget()->scrollToAnchor(anchor);
//			}

//			ui->lobbyTreeWidget->setCurrentItem(item);
//			return true;
//		}
//	}

	return false;
}

bool ChatLobbyWidget::showContactAnchor(RsPeerId id, QString anchor)
{
//    ChatId chatId(id);
//    //Need to get the current index, but not current item
//    QTreeWidgetItem *item = getTreeWidgetItemForChatId(chatId) ;

//    if(item != NULL) {
//        if(item->type() == TYPE_ONE2ONE) {

//            if(_chatOne2One_infos.find(id.toStdString()) == _chatOne2One_infos.end()) {
//                //showBlankPage(id) ;
//            } else {
//                //ChatLobbyDialog cldChatLobby =_lobby_infos[id].dialog;
//                ui->stackedWidget->setCurrentWidget(_chatOne2One_infos[id.toStdString()].dialog) ;
//                PopupChatDialog *cldCW=NULL ;
//                if (NULL != (cldCW = dynamic_cast<PopupChatDialog *>(ui->stackedWidget->currentWidget())))
//                    cldCW->getChatWidget()->scrollToAnchor(anchor);
//            }

//            //ui->lobbyTreeWidget->setCurrentItem(item);
//            //ui->lobbyTreeWidget->setCurrentIndex()
//            return true;
//        }
//    }

    return false;
}

void ChatLobbyWidget::subscribeChatLobbyAtItem(QTreeWidgetItem *item)
{
    if (item == NULL || item->type() != TYPE_LOBBY) {
        return;
    }

    ChatLobbyId id = item->data(COLUMN_DATA, ROLE_ID).toULongLong();
    ChatLobbyFlags flags ( item->data(COLUMN_DATA, ROLE_FLAGS).toUInt());
    RsGxsId gxs_id ;

    std::list<RsGxsId> own_ids;
    // not using rsMsgs->getDefaultIdentityForChatLobby(), to check if we have an identity
    // to work around the case when the identity was deleted and is invalid
    // (the chatservice does not know if a default identity was deleted)
    // only rsIdentity knows the truth at the moment!
    if(!rsIdentity->getOwnIds(own_ids))
        return;

    // if there is no identity yet, show the dialog to create a new identity
    if(own_ids.empty())
    {
        IdEditDialog dlg(this);
        dlg.setupNewId(false);
        
        if(flags & RS_CHAT_LOBBY_FLAGS_PGP_SIGNED) //
		dlg.enforceNoAnonIds() ;
        dlg.exec();
        // fetch new id
        if(!rsIdentity->getOwnIds(own_ids) || own_ids.empty())
            return;
        gxs_id = own_ids.front();
    }
    else
    {
        rsMsgs->getDefaultIdentityForChatLobby(gxs_id);
        
        RsIdentityDetails idd ;
        if(!rsIdentity->getIdDetails(gxs_id,idd))
            return ;
        
        if( (flags & RS_CHAT_LOBBY_FLAGS_PGP_SIGNED) && !(idd.mFlags & RS_IDENTITY_FLAGS_PGP_LINKED))
        {
            QMessageBox::warning(NULL,tr("Default identity is anonymous"),tr("You cannot join this chat room with your default identity, since it is anonymous and the chat room forbids it.")) ;
            return ;
        }
    }

    if(rsMsgs->joinVisibleChatLobby(id,gxs_id))
        ChatDialog::chatFriend(ChatId(id),true) ;
}

void ChatLobbyWidget::joinGroupChatInBackground(ChatLobbyInfo lobbyInfo)
{
    ChatLobbyFlags flags = lobbyInfo.lobby_flags;
    RsGxsId gxs_id ;

    std::list<RsGxsId> own_ids;
    // not using rsMsgs->getDefaultIdentityForChatLobby(), to check if we have an identity
    // to work around the case when the identity was deleted and is invalid
    // (the chatservice does not know if a default identity was deleted)
    // only rsIdentity knows the truth at the moment!
    if(!rsIdentity->getOwnIds(own_ids))
    {

        rsMsgs->getDefaultIdentityForChatLobby(gxs_id);
        if(rsMsgs->joinVisibleChatLobby(lobbyInfo.lobby_id,gxs_id))
        {
#ifdef CHAT_LOBBY_GUI_DEBUG
            std::cerr << " Joining groupchat: " << lobbyInfo.lobby_name << ", topic " << lobbyInfo.lobby_topic << " #" << std::hex << lobbyInfo.lobby_id << std::dec << " public. Lobby flags: " << lobbyInfo.lobby_flags << std::endl;
#endif

        }
        RsIdentityDetails idd ;
        if(!rsIdentity->getIdDetails(gxs_id,idd))
            return ;

        if( (flags & RS_CHAT_LOBBY_FLAGS_PGP_SIGNED) && !(idd.mFlags & RS_IDENTITY_FLAGS_PGP_LINKED))
        {
            QMessageBox::warning(NULL,tr("Default identity is anonymous"),tr("You cannot join this chat room with your default identity, since it is anonymous and the chat room forbids it.")) ;
            return ;
        }
    }

}

void ChatLobbyWidget::autoSubscribeLobby(QTreeWidgetItem *item)
{
    if (item == NULL || item->type() != TYPE_LOBBY) {
        return;
    }

    ChatLobbyId id = item->data(COLUMN_DATA, ROLE_ID).toULongLong();
    bool isAutoSubscribe = rsMsgs->getLobbyAutoSubscribe(id);
    rsMsgs->setLobbyAutoSubscribe(id, !isAutoSubscribe);
    if (!isAutoSubscribe && !item->data(COLUMN_DATA, ROLE_SUBSCRIBED).toBool())
        subscribeChatLobbyAtItem(item);
}

void ChatLobbyWidget::showBlankPage(ChatLobbyId id)
{
	// show the default blank page.
    ui->stackedWidget->setCurrentWidget(ui->_lobby_blank_page) ;

    QString text = tr("UnseenP2P is a decentralized, private and secure commmunication and sharing platform. \nUnseenP2P provides filesharing, chat, messages, forums and channels. ") ;
    ui->lobbyInfoLabel->setText(text) ;
}

void ChatLobbyWidget::subscribeItem()
{
    //subscribeChatLobbyAtItem(ui->lobbyTreeWidget->currentItem());
}

void ChatLobbyWidget::autoSubscribeItem()
{
   // autoSubscribeLobby(ui->lobbyTreeWidget->currentItem());
}

void ChatLobbyWidget::copyItemLink()
{
//	QTreeWidgetItem *item = ui->lobbyTreeWidget->currentItem();
//	if (item == NULL || item->type() != TYPE_LOBBY) {
//		return;
//	}

//	ChatLobbyId id = item->data(COLUMN_DATA, ROLE_ID).toULongLong();
//	QString name = item->text(COLUMN_NAME);

//	RetroShareLink link = RetroShareLink::createChatRoom(ChatId(id),name);
//	if (link.valid()) {
//		QList<RetroShareLink> urls;
//		urls.push_back(link);
//		RSLinkClipboard::copyLinks(urls);
//    }
}

//update recent time for every chat item and sort by recent time
void ChatLobbyWidget::updateRecentTime(const ChatId & chatId, std::string nickInGroupChat, long long current_time, std::string textmsg, bool isSend)
{

        //Need to update the Conversation list in rsMsg, then update the GUI of chat item list in ChatLobbyWIdget
       std::string chatIdStr;
       if (chatId.isLobbyId())          //for groupchat
       {
           chatIdStr = std::to_string(chatId.toLobbyId());
       }
       else if (chatId.isPeerId())      //for one2one chat
       {
           chatIdStr = chatId.toPeerId().toStdString();
       }

       //update both new last msg and last msg datetime to the conversation list
       rsMsgs->updateRecentTimeOfItemInConversationList(chatIdStr, nickInGroupChat, current_time, textmsg, !isSend);

       //Need to get the selected item with saving uId before sorting and updating the layout
       //so that we use uId to re-select the item with saved uId
        // Even when user in search mode, we still can get the right selectedUId
       QModelIndexList list = ui->lobbyTreeWidget->selectionModel()->selectedIndexes();
       std::string selectedUId;
       foreach (QModelIndex index, list)
       {
           if (index.row()!=-1)
           {
               selectedUId = rsMsgs->getSeletedUIdBeforeSorting(index.row());
               break;
           }
       }

        if (!isSend)
        {
            //receive new msg, need to update unread msg to model data, increase 1
            rsMsgs->updateUnreadNumberOfItemInConversationList(chatIdStr, 1, false);
            //check if this is a current chat window, so update new msg as read
            if (chatIdStr == selectedUId)
            {
                rsHistory->updateMessageAsRead(chatId);
            }
        }
        else
        {
            //check if this is the filtered search mode, just return to normal mode
            if (rsMsgs->getConversationListMode() == CONVERSATION_MODE_WITH_SEARCH_FILTER)
            {
                if (!ui->filterLineEdit->text().isEmpty())
                {
                    ui->filterLineEdit->setText("");
                }
            }
        }

        rsMsgs->sortConversationItemListByRecentTime();
        emit ui->lobbyTreeWidget->model()->layoutChanged();

        // For both case of sending or receiving, just clear the selection and re-select chat item
        ui->lobbyTreeWidget->selectionModel()->clearSelection();

        //re-select the chat item again, the old selected one was saved to selectedIndex
        // at first find the index of the uId, then re-select

        int seletedrow = rsMsgs->getIndexFromUId(selectedUId);
        QModelIndex idx = ui->lobbyTreeWidget->model()->index(seletedrow, 0);
        ui->lobbyTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
        emit ui->lobbyTreeWidget->model()->layoutChanged();

}

QTreeWidgetItem *ChatLobbyWidget::getTreeWidgetItemForChatId(ChatId id)
{
     QTreeWidgetItem *lobby_item = commonItem;
     int childCnt = lobby_item->childCount();
     int childIndex = 0;

     while (childIndex < childCnt) {
             QTreeWidgetItem *itemLoop = lobby_item->child(childIndex);

	     if (itemLoop->type() == TYPE_ONE2ONE && itemLoop->data(COLUMN_DATA, ROLE_ID) == QString::fromUtf8(id.toPeerId().toStdString().c_str()))
		     return itemLoop ;

             ++childIndex ;
     }
     return NULL ;

}
QTreeWidgetItem *ChatLobbyWidget::getTreeWidgetItem(ChatLobbyId id)
{

    QTreeWidgetItem *lobby_item = commonItem;
    int childCnt = lobby_item->childCount();
    int childIndex = 0;

    while (childIndex < childCnt) {
        QTreeWidgetItem *itemLoop = lobby_item->child(childIndex);

        if (itemLoop->type() == TYPE_LOBBY && itemLoop->data(COLUMN_DATA, ROLE_ID).toULongLong() == id)
            return itemLoop ;

        ++childIndex ;
    }

	return NULL ;
}
void ChatLobbyWidget::updateTypingStatus(ChatLobbyId id)
{
    //unseenp2p - meiyousixin - no need to show typing icon on group item on the left list
//	QTreeWidgetItem *item = getTreeWidgetItem(id) ;
	
//	if(item != NULL)
//	{
//		item->setIcon(COLUMN_NAME,QIcon(IMAGE_TYPING)) ;
//		_lobby_infos[id].last_typing_event = time(NULL) ;

//		QTimer::singleShot(5000,this,SLOT(resetLobbyTreeIcons())) ;
//	}
}
void ChatLobbyWidget::updatePeerLeaving(ChatLobbyId id)
{
     //unseenp2p - meiyousixin - no need to show leaving and joining icons on the icon of left list
//	QTreeWidgetItem *item = getTreeWidgetItem(id) ;
	
//	if(item != NULL)
//	{
//		item->setIcon(COLUMN_NAME,QIcon(IMAGE_PEER_LEAVING)) ;
//		_lobby_infos[id].last_typing_event = time(NULL) ;

//		QTimer::singleShot(5000,this,SLOT(resetLobbyTreeIcons())) ;
//	}
}
void ChatLobbyWidget::updatePeerEntering(ChatLobbyId id)
{
    //unseenp2p - meiyousixin - no need to show leaving and joining icons on the icon of left list
//	QTreeWidgetItem *item = getTreeWidgetItem(id) ;
	
//	if(item != NULL)
//	{
//		item->setIcon(COLUMN_NAME,QIcon(IMAGE_PEER_ENTERING)) ;
//		_lobby_infos[id].last_typing_event = time(NULL) ;

//		QTimer::singleShot(5000,this,SLOT(resetLobbyTreeIcons())) ;
//	}
}
void ChatLobbyWidget::resetLobbyTreeIcons()
{
	time_t now = time(NULL) ;

	for(std::map<ChatLobbyId,ChatLobbyInfoStruct>::iterator it(_lobby_infos.begin());it!=_lobby_infos.end();++it)
		if(it->second.last_typing_event + 5 <= now)
		{
			getTreeWidgetItem(it->first)->setIcon(COLUMN_NAME,it->second.default_icon) ;
			//std::cerr << "Reseted 1 lobby icon." << std::endl;
		}
}

void ChatLobbyWidget::unsubscribeItem()
{
}

void ChatLobbyWidget::unsubscribeChatLobby(ChatLobbyId id)
{

    // close the tab.
    std::map<ChatLobbyId,ChatLobbyInfoStruct>::iterator it = _lobby_infos.find(id) ;

    if(it != _lobby_infos.end())
    {
        if (myChatLobbyUserNotify){
            myChatLobbyUserNotify->chatLobbyCleared(id, "");
        }

        ui->stackedWidget->removeWidget(it->second.dialog) ;
        _lobby_infos.erase(it) ;
    }
    //remove item from conversations list, using the MVC now
    std::string uId = std::to_string(id);
    if (rsMsgs->isChatIdInConversationList(uId))
    {
        rsMsgs->removeContactOrGroupChatFromModelData(uId);
        emit ui->lobbyTreeWidget->model()->layoutChanged();
    }

    // Unsubscribe the chat lobby
    ChatDialog::closeChat(ChatId(id));
    rsMsgs->unsubscribeChatLobby(id);
    rsMsgs->setLobbyAutoSubscribe(id, false );


    //Re-select the chat item on the left, depend on the history of stackedWidget
    //need to get the next dialog in the ui->stackedWidget
    PopupChatDialog *cldCW=NULL ;
    ChatLobbyDialog *groupChatCW = NULL;
    std::string chatIdStr;

    if (NULL != (cldCW = dynamic_cast<PopupChatDialog *>(ui->stackedWidget->currentWidget())))
    {
            //cldCW->getChatWidget()->scrollToAnchor(anchor);
        ChatId chatId = cldCW->chatId();
        chatIdStr= chatId.toPeerId().toStdString();

    }
    else
    {
        if (NULL != (groupChatCW = dynamic_cast<ChatLobbyDialog *>(ui->stackedWidget->currentWidget())))
        {
            //ChatId chatId = groupChatCW->id();
            chatIdStr = std::to_string(groupChatCW->id());
        }

    }

    if (chatIdStr.length() > 0)
    {
        //check if this is the filtered search mode, just return to normal mode and select on the normal mode of conversation list
        if (rsMsgs->getConversationListMode() == CONVERSATION_MODE_WITH_SEARCH_FILTER)
        {
            if (!ui->filterLineEdit->text().isEmpty())
            {
                ui->filterLineEdit->setText("");
            }
        }

         //re-select the chat item again, at first find the index of the uId, then re-select
         int seletedrow = rsMsgs->getIndexFromUId(chatIdStr);
         if (seletedrow >= 0)
         {
             ui->lobbyTreeWidget->selectionModel()->clearSelection();
             QModelIndex idx = ui->lobbyTreeWidget->model()->index(seletedrow, 0);
             ui->lobbyTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
             //emit ui->lobbyTreeWidget->model()->layoutChanged();
         }

    }


}
// Try to add selection of contact chat in the same tree widget
void ChatLobbyWidget::updateCurrentLobby()
{
}
void ChatLobbyWidget::updateMessageChanged(bool incoming, ChatLobbyId id, QDateTime time, QString senderName, QString msg)
{
//	QTreeWidgetItem *current_item = ui->lobbyTreeWidget->currentItem();
//	bool bIsCurrentItem = (current_item != NULL && current_item->data(COLUMN_DATA, ROLE_ID).toULongLong() == id);

    if (myChatLobbyUserNotify){
        if (incoming) myChatLobbyUserNotify->chatLobbyNewMessage(id, time, senderName, msg);
    }

}

//void ChatLobbyWidget::updateP2PMessageChanged(bool incoming, const ChatId& chatId, QDateTime time, QString senderName, QString msg)
void ChatLobbyWidget::updateP2PMessageChanged(ChatMessage msg)
{
//    QTreeWidgetItem *item = getTreeWidgetItemForChatId(msg.chat_id);
//    QTreeWidgetItem *current_item = ui->lobbyTreeWidget->currentItem();

    if (myChatLobbyUserNotify)
    {
        QDateTime sendTime = QDateTime::fromTime_t(msg.sendTime);
        QString message = QString::fromUtf8(msg.msg.c_str());
        QString ownName = QString::fromUtf8(rsPeers->getPeerName(rsPeers->getOwnId()).c_str());
        QString name = msg.incoming? QString::fromStdString(rsPeers->getGPGName(rsPeers->getGPGId(msg.chat_id.toPeerId()))): ownName;
        if (msg.incoming) myChatLobbyUserNotify->chatP2PNewMessage(msg.chat_id, sendTime, name, message);
    }

}

void ChatLobbyWidget::itemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    //std::cerr << " Add group chat item to the common item when get history, group name: "  << std::endl;
    //subscribeChatLobbyAtItem(item);
}

void ChatLobbyWidget::displayChatLobbyEvent(qulonglong lobby_id, int event_type, const RsGxsId &gxs_id, const QString& str)
{
    if (ChatLobbyDialog *cld = dynamic_cast<ChatLobbyDialog*>(ChatDialog::getExistingChat(ChatId(lobby_id)))) {
        cld->displayLobbyEvent(event_type, gxs_id, str);
    }
}

void ChatLobbyWidget::readChatLobbyInvites()
{
    std::list<ChatLobbyInvite> invites;
    rsMsgs->getPendingChatLobbyInvites(invites);

    RsGxsId default_id ;
    rsMsgs->getDefaultIdentityForChatLobby(default_id) ;

	std::list<ChatLobbyId> subscribed_lobbies ;
	rsMsgs->getChatLobbyList(subscribed_lobbies) ;

	for(std::list<ChatLobbyInvite>::const_iterator it(invites.begin());it!=invites.end();++it)
	{
		// first check if the lobby is already subscribed. If so, just ignore the request.

		bool found = false ;
		for(auto it2(subscribed_lobbies.begin());it2!=subscribed_lobbies.end() && !found;++it2)
			found = found || (*it2 == (*it).lobby_id) ;

		if(found)
			continue ;

        // auto accept groupchat invite using default gxsId
        //chosen_id = default_id;
        if(rsMsgs->acceptLobbyInvite((*it).lobby_id,default_id)) // before: chosen_id
        {
            receiveGroupInvite = true;
            ChatDialog::chatFriend(ChatId((*it).lobby_id),false); //before is true
            rsMsgs->setLobbyAutoSubscribe((*it).lobby_id, true);
            rsMsgs->joinVisibleChatLobby((*it).lobby_id, default_id); // before: chosen_id
        }
        else
            std::cerr << "Can't join chat room with id 0x" << std::hex << (*it).lobby_id << std::dec << std::endl;

    }

	myInviteYesButton = NULL;
	myInviteIdChooser = NULL;
}

void ChatLobbyWidget::idChooserCurrentIndexChanged(int /*index*/)
{
	if (myInviteYesButton && myInviteIdChooser)
	{
		RsGxsId chosen_id;
		switch (myInviteIdChooser->getChosenId(chosen_id))
		{
			case GxsIdChooser::KnowId:
			case GxsIdChooser::UnKnowId:
				myInviteYesButton->setEnabled(true);
			break;
			case GxsIdChooser::NoId:
			case GxsIdChooser::None:
			default: ;
				myInviteYesButton->setEnabled(false);
		}
	}
}

void ChatLobbyWidget::filterColumnChanged(int)
{
    filterItems(ui->filterLineEdit->text());
}

void ChatLobbyWidget::filterItems(const QString &text)
{

    if (text.isEmpty())
    {
        rsMsgs->setConversationListMode(CONVERSATION_MODE_WITHOUT_FILTER);
    }
    else
    {
        rsMsgs->setConversationListMode(CONVERSATION_MODE_WITH_SEARCH_FILTER);
        std::string filter = text.toStdString();
        rsMsgs->setSearchFilter(filter);
    }
    ui->lobbyTreeWidget->selectionModel()->clearSelection();
    emit ui->lobbyTreeWidget->model()->layoutChanged();
    ui->lobbyTreeWidget->show();
}

bool ChatLobbyWidget::filterItem(QTreeWidgetItem *item, const QString &text, int filterColumn)
{
    bool visible = true;

    if (text.isEmpty() == false) {
        if (item->text(filterColumn).contains(text, Qt::CaseInsensitive) == false) {
            visible = false;
        }
    }

    int visibleChildCount = 0;
    int count = item->childCount();
    for (int index = 0; index < count; ++index) {
        if (filterItem(item->child(index), text, filterColumn)) {
            ++visibleChildCount;
        }
    }

    if (visible || visibleChildCount) {
        item->setHidden(false);
    } else {
        item->setHidden(true);
    }

    return (visible || visibleChildCount);
}

void ChatLobbyWidget::openLastChatWindow()
{
    //run only the first time and once only!!!
    if (alreadyOpenLastChatWindow) return;
    std::string selectedUId;
    if (lastChatId.isNotSet()) return;
    ChatId chatId = lastChatId;
    std::cerr << "try to open the last chat in the previous time" << std::endl;
    if (chatId.isPeerId())
    {
        std::string chatIdStr = chatId.toStdString();
        showContactChatMVC(chatIdStr);
        selectedUId = chatIdStr;
    }
    else if (chatId.isLobbyId())
    {
        showGroupChatMVC(chatId.toLobbyId());
        selectedUId = std::to_string(chatId.toLobbyId());
    }

    //clear selection first
    ui->lobbyTreeWidget->selectionModel()->clearSelection();
    emit ui->lobbyTreeWidget->model()->layoutChanged();
    ui->lobbyTreeWidget->show();

    //open the last chat window in previous time
    int seletedrow = rsMsgs->getIndexFromUId(selectedUId);
    QModelIndex idx = ui->lobbyTreeWidget->model()->index(seletedrow, 0);
    if (idx.row() > 0)
    {
        ui->lobbyTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
        emit ui->lobbyTreeWidget->model()->layoutChanged();
    }

    alreadyOpenLastChatWindow = true;

}

void ChatLobbyWidget::processSettings(bool bLoad)
{
	m_bProcessSettings = true;
    Settings->beginGroup(QString("ChatLobbyWidget"));

    if (bLoad) {
        // load settings
        ui->splitter->restoreState(Settings->value("splitter").toByteArray());
        ui->lobbyTreeWidget->restoreGeometry(Settings->value("geometry").toByteArray());
        QString lastChatIdStr;
        QString chatIdStr = Settings->value("lastChatId", lastChatIdStr).toString();
        lastChatId = ChatId(chatIdStr.toStdString());
    } else {
        // save settings
        Settings->setValue("splitter", ui->splitter->saveState());
        Settings->setValue("geometry",ui->lobbyTreeWidget->saveGeometry());
        Settings->setValue("lastChatId", QString::fromStdString(lastChatId.toStdString()));
    }

	Settings->endGroup();
    m_bProcessSettings = false;
}

void ChatLobbyWidget::setShowUserCountColumn(bool show)
{
}

void ChatLobbyWidget::setShowTopicColumn(bool show)
{
}

void ChatLobbyWidget::setShowSubscribeColumn(bool show)
{
}

int ChatLobbyWidget::getNumColVisible()
{
	int iNumColVis=0;
//	for (int iColumn = 0; iColumn < COLUMN_COUNT; ++iColumn) {
//		if (!ui->lobbyTreeWidget->isColumnHidden(iColumn)) {
//			++iNumColVis;
//		}
//	}
    return iNumColVis;
}

void ChatLobbyWidget::openOne2OneChat(std::string rsId, std::string nickname)
{
  RsPgpId pgpId = RsPgpId(rsId);
  RsPeerDetails detail;
  if (rsPeers->getGPGDetails(pgpId, detail))
    {
        // just open a window as normal (using RS calling), add the one2one chat page will be processed in addOne2OneChatPage.
        ChatDialog::chatFriend(pgpId);
    }
}



void ChatLobbyWidget::resetAvatarForContactItem( const ChatId& chatId)
{
//    QTreeWidgetItem *item = getTreeWidgetItemForChatId(chatId);
//    QPixmap avatar;
//    AvatarDefs::getAvatarFromSslId(chatId.toPeerId(), avatar);
//    if (!avatar.isNull() && item)
//          item->setIcon(COLUMN_NAME,QIcon(avatar)) ;
}

//QIcon avatar
static QIcon createAvatar(const QPixmap &avatar, const QPixmap &overlay, bool unread)
{
    int avatarWidth = avatar.width();
    int avatarHeight = avatar.height();

    QPixmap pixmap(avatar);

    int overlaySize = (avatarWidth > avatarHeight) ? (avatarWidth/2.5) :  (avatarHeight/2.5);
    int overlayX = avatarWidth - overlaySize;
    int overlayY = avatarHeight - overlaySize;

    QPainter painter(&pixmap);
    painter.drawPixmap(overlayX, overlayY, overlaySize, overlaySize, overlay);
    if (unread)
    {
        QIcon unreadIcon = QIcon(IMAGE_UNREAD_ICON);
        int overlayUnreadY = avatarHeight - 2.5*overlaySize;
        int overlayUnreadX = avatarHeight - overlaySize;
        QPixmap unreadOverlay = unreadIcon.pixmap(unreadIcon.actualSize(QSize(25, 15)));
        painter.drawPixmap(overlayUnreadX, overlayUnreadY, overlaySize, overlaySize*1.5, unreadOverlay);    //d: increase size unreadIcon
    }

    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

//QImage avatar
static QImage createImageAvatar(const QPixmap &avatar, const QPixmap &overlay, bool unread)
{
    int avatarWidth = avatar.width();
    int avatarHeight = avatar.height();

    QPixmap pixmap(avatar);

    int overlaySize = (avatarWidth > avatarHeight) ? (avatarWidth/2.5) :  (avatarHeight/2.5);
    int overlayX = avatarWidth - overlaySize;
    int overlayY = avatarHeight - overlaySize;

    QPainter painter(&pixmap);
    painter.drawPixmap(overlayX, overlayY, overlaySize, overlaySize, overlay);
    if (unread)
    {
        QIcon unreadIcon = QIcon(IMAGE_UNREAD_ICON);
        int overlayUnreadY = avatarHeight - 2.5*overlaySize;
        int overlayUnreadX = avatarHeight - overlaySize;
        QPixmap unreadOverlay = unreadIcon.pixmap(unreadIcon.actualSize(QSize(25, 15)));
        painter.drawPixmap(overlayUnreadX, overlayUnreadY, overlaySize, overlaySize*1.5, unreadOverlay);    //d: increase size unreadIcon
    }

    QImage image = pixmap.toImage();
    return image;
}

QPixmap ChatLobbyWidget::currentStatusIcon(RsPeerId peerId, QFont& gpgFontOut)
{
    StatusInfo statusContactInfo;
    rsStatus->getStatus(peerId,statusContactInfo);
    QFont gpgFont;
    QPixmap gpgOverlayIcon;
    gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_OFFLINE));
    switch (statusContactInfo.status)
    {
        case RS_STATUS_INACTIVE:
            gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_AWAY));
            gpgFont.setBold(false);
            break;

        case RS_STATUS_ONLINE:
            gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_ONLINE));
            gpgFont.setBold(true);
            break;

        case RS_STATUS_AWAY:
            gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_AWAY));
            gpgFont.setBold(true);
            break;

        case RS_STATUS_BUSY:
            gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_BUSY));
            gpgFont.setBold(true);
            break;
    }
    gpgFontOut = gpgFont;
    return gpgOverlayIcon;
}

void ChatLobbyWidget::updateContactItem(QTreeWidget *treeWidget, QTreeWidgetItem *item, const std::string &nickname, const ChatId& chatId, const std::string &rsId, unsigned int current_time, bool unread)
{
}

void ChatLobbyWidget::updateGroupChatItem(QTreeWidget *treeWidget, QTreeWidgetItem *item, const std::string &name, const ChatLobbyId& id, unsigned int current_time, bool unread, ChatLobbyFlags lobby_flags)
{
}

QIcon ChatLobbyWidget::lastIconForPeerId(RsPeerId peerId, bool unread)
{
    QPixmap bestAvatar;
    AvatarDefs::getAvatarFromSslId(peerId, bestAvatar);
    QFont gpgFont;
    QPixmap gpgOverlayIcon = currentStatusIcon(peerId, gpgFont);
    return createAvatar(bestAvatar.isNull() ? QPixmap(AVATAR_DEFAULT_IMAGE) : bestAvatar, gpgOverlayIcon, unread);

}

QImage ChatLobbyWidget::avatarImageForPeerId(RsPeerId peerId)
{
    QPixmap bestAvatar;
    AvatarDefs::getAvatarFromSslId(peerId, bestAvatar);

    return bestAvatar.toImage();

}

void ChatLobbyWidget::UpdateStatusForContact(QTreeWidgetItem* gpgItem , const RsPeerId peerId)
{
    StatusInfo statusContactInfo;
    rsStatus->getStatus(peerId,statusContactInfo);
    QFont gpgFont;
    QPixmap gpgOverlayIcon;
    gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_OFFLINE));
    switch (statusContactInfo.status)
    {
        case RS_STATUS_INACTIVE:
            gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_AWAY));
            gpgFont.setBold(false);
            break;

        case RS_STATUS_ONLINE:
            gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_ONLINE));
            gpgFont.setBold(true);
            break;

        case RS_STATUS_AWAY:
            gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_AWAY));
            gpgFont.setBold(true);
            break;

        case RS_STATUS_BUSY:
            gpgOverlayIcon = QPixmap(StatusDefs::imageStatus(RS_STATUS_BUSY));
            gpgFont.setBold(true);
            break;
    }

    //Need to set status Icon to avatar
    QPixmap bestAvatar;
    AvatarDefs::getAvatarFromSslId(peerId, bestAvatar);
    ChatId chatId(peerId);
    bool unread = (recentUnreadListOfChatId.count(chatId) != 0? true: false);
    gpgItem->setIcon(COLUMN_NAME, createAvatar(bestAvatar.isNull() ? QPixmap(AVATAR_DEFAULT_IMAGE) : bestAvatar, gpgOverlayIcon, unread));
    gpgItem->setFont(COLUMN_NAME, gpgFont);

}

void ChatLobbyWidget::ContactStatusChanged(QString peerIdStr, int status)
{
    //update in the model, in the model will take the status and update there
    emit ui->lobbyTreeWidget->model()->layoutChanged();
    ui->lobbyTreeWidget->show();
}

void ChatLobbyWidget::UpdateStatusForAllContacts()
{
    std::list<RsPeerId> peerIds;
    if (rsPeers->getFriendList(peerIds))
    {
        std::list<RsPeerId>::iterator peerId;
        for (peerId = peerIds.begin(); peerId != peerIds.end(); ++peerId)
        {
            ChatId chatId(*peerId);
            QTreeWidgetItem* curItem = getTreeWidgetItemForChatId(chatId);
            if (curItem)
                UpdateStatusForContact(curItem, *peerId);
        }
    }
}
void ChatLobbyWidget::on_addContactButton_clicked()
{
        ConnectFriendWizard connwiz (this);
        connwiz.setStartId(ConnectFriendWizard::Page_Text);
        connwiz.exec ();
}

std::map<ChatLobbyId,ChatLobbyInfoStruct> ChatLobbyWidget::getGroupChatList()
{
    return _lobby_infos;
}
std::map<std::string,ChatOne2OneInfoStruct> ChatLobbyWidget::getOne2OneChatList()
{
    return _chatOne2One_infos;
}





