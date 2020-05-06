#pragma once

#include "RsAutoUpdatePage.h"
#include "chat/ChatLobbyUserNotify.h"
#include "gui/gxs/GxsIdChooser.h"
#include "chat/PopupChatDialog.h"
#include "retroshare/rsstatus.h"

#include <retroshare/rsmsgs.h>
#include "chat/distributedchat.h"

//unseenp2p
#include "gui/smartlistmodel.h"
#include "gui/models/conversationmodel.h"
#include <vector>

#include <QAbstractButton>
#include <QTreeWidget>

#define IMAGE_CHATLOBBY			    ":/home/img/face_icon/un_chat_icon_128.png"

#define CHAT_LOBBY_PRIVACY_LEVEL_PUBLIC  1
#define CHAT_LOBBY_PRIVACY_LEVEL_PRIVATE 2
#define CHAT_LOBBY_ONE2ONE_LEVEL	 3

#define IMAGE_CREATE          ""
#define IMAGE_PUBLIC          ":/chat/img/groundchat.png"               //d: Update unseen icon
#define IMAGE_PRIVATE         ":/chat/img/groundchat_private.png"       //d: Update unseen icon
#define IMAGE_SUBSCRIBE       ":/images/edit_add24.png"
#define IMAGE_UNSUBSCRIBE     ":/images/cancel.png"
#define IMAGE_PEER_ENTERING   ":/chat/img/personal_add_64.png"          //d: Update unseen icon
#define IMAGE_PEER_LEAVING    ":/chat/img/personal_remove_64.png"       //d: Update unseen icon
#define IMAGE_TYPING		  ":/chat/img/typing.png"                   //d: Update unseen icon
#define IMAGE_MESSAGE	      ":/chat/img/chat_32.png"                  //d: Update unseen icon
#define IMAGE_MESSAGE_PRIVATE ":/chat/img/groundchat_private_unread.png"                //d: Notification icon for private group
#define IMAGE_AUTOSUBSCRIBE   ":/images/accepted16.png"
#define IMAGE_COPYRSLINK      ":/images/copyrslink.png"
#define IMAGE_UNSEEN          ":/app/images/unseen32.png"
#define IMAGE_UNREAD_ICON      ":/home/img/face_icon/un_chat_icon_d_128.png"

class RSTreeWidgetItemCompareRole;
class ChatTabWidget ;
class ChatLobbyDialog ;
class QTextBrowser ;

class PgpAuxUtils;

struct ChatLobbyInfoStruct
{
	QIcon default_icon ;
	ChatLobbyDialog *dialog ;
	time_t last_typing_event ;
};

struct ChatOne2OneInfoStruct
{
	QIcon default_icon ;
	PopupChatDialog *dialog;
	time_t last_typing_event;
};

namespace Ui {
class ChatLobbyWidget;
}
class ChatLobbyWidget : public RsAutoUpdatePage
{
	Q_OBJECT

public:
	/** Default constructor */
	ChatLobbyWidget(QWidget *parent = 0, Qt::WindowFlags flags = 0);

	/** Default destructor */
	~ChatLobbyWidget();

	virtual QIcon iconPixmap() const { return QIcon(IMAGE_CHATLOBBY) ; } //MainPage
    virtual QString pageName() const { return tr("Old Chats") ; } //MainPage
	virtual QString helpText() const { return ""; } //MainPage

	virtual UserNotify *getUserNotify(QObject *parent); //MainPage

	virtual void updateDisplay();

	void setCurrentChatPage(ChatLobbyDialog *) ;	// used by ChatLobbyDialog to raise.
	void addChatPage(ChatLobbyDialog *) ;
	bool showLobbyAnchor(ChatLobbyId id, QString anchor) ;

    unsigned int unreadCount();

	void openOne2OneChat(std::string rsId, std::string nickname);
	void addOne2OneChatPage(PopupChatDialog *d);
	void setCurrentOne2OneChatPage(PopupChatDialog *d);
    void updateContactItem(QTreeWidget *treeWidget, QTreeWidgetItem *item, const std::string &nickname, const ChatId& chatId, const std::string &rsId, unsigned int current_time, bool unread);
    void updateGroupChatItem(QTreeWidget *treeWidget, QTreeWidgetItem *item, const std::string &name, const ChatLobbyId& chatId,  unsigned int current_time, bool unread, ChatLobbyFlags lobby_flags);
	void fromGpgIdToChatId(const RsPgpId &gpgId,  ChatId &chatId);
    bool showContactAnchor(RsPeerId id, QString anchor);

    //unseen p2p - try to work with Model-view (smartlistview, smarlistmodel)
    std::map<ChatLobbyId,ChatLobbyInfoStruct> getGroupChatList();       // 17 Oct 2019 - meiyousixin
    std::map<std::string,ChatOne2OneInfoStruct> getOne2OneChatList();   // 17 Oct 2019 - meiyousixin

    unsigned int getUnreadMsgNumberForChat(ChatId chatId);  //01 Nov 2019, Unseenp2p - for new MVC GUI
    void openLastChatWindow();

    QIcon lastIconForPeerId(RsPeerId peerId, bool unread);
    QImage avatarImageForPeerId(RsPeerId peerId);

    void processSettings(bool bLoad);

signals:
    void unreadCountChanged(unsigned int unreadCount);

protected slots:
	void lobbyChanged();
	void lobbyTreeWidgetCustomPopupMenu(QPoint);
	void createChatLobby();
	void subscribeItem();
	void unsubscribeItem();
	void itemDoubleClicked(QTreeWidgetItem *item, int column);
	void updateCurrentLobby() ;
	void displayChatLobbyEvent(qulonglong lobby_id, int event_type, const RsGxsId& gxs_id, const QString& str);
	void readChatLobbyInvites();
	void showLobby(QTreeWidgetItem *lobby_item) ;
	void showBlankPage(ChatLobbyId id) ;
	void unsubscribeChatLobby(ChatLobbyId id) ;
	void createIdentityAndSubscribe();
	void subscribeChatLobbyAs() ;
	void updateTypingStatus(ChatLobbyId id) ;
	void resetLobbyTreeIcons() ;
	void updateMessageChanged(bool incoming, ChatLobbyId, QDateTime time, QString senderName, QString msg);
	void updatePeerEntering(ChatLobbyId);
	void updatePeerLeaving(ChatLobbyId);
	void autoSubscribeItem();
	void copyItemLink();
    void updateRecentTime(const ChatId&, std::string, long long, std::string, bool);
    //void updateP2PMessageChanged(bool incoming, const ChatId& chatId, QDateTime time, QString senderName, QString msg);
    void updateP2PMessageChanged(ChatMessage);
    void on_addContactButton_clicked();
private slots:
	void filterColumnChanged(int);
	void filterItems(const QString &text);
	
	void setShowUserCountColumn(bool show);
	void setShowTopicColumn(bool show);
	void setShowSubscribeColumn(bool show);

	void updateNotify(ChatLobbyId id, unsigned int count) ;
    void updateNotifyFromP2P(ChatId id, unsigned int count);
	void idChooserCurrentIndexChanged(int index);

    void UpdateStatusForAllContacts();
    void UpdateStatusForContact(QTreeWidgetItem* gpgItem, const RsPeerId peerId);
    void ContactStatusChanged(QString, int);
    //unseenp2p - using for new chat list
    void smartListSelectionChanged(const QItemSelection  &selected, const QItemSelection  &deselected);

private:
	void autoSubscribeLobby(QTreeWidgetItem *item);
	void subscribeChatLobby(ChatLobbyId id) ;
	void subscribeChatLobbyAtItem(QTreeWidgetItem *item) ;
    void joinGroupChatInBackground(ChatLobbyInfo lobbyInfo);
	bool filterItem(QTreeWidgetItem *item, const QString &text, int filterColumn);

    //unseenp2p
    void selectConversation(const QModelIndex& index);

    RSTreeWidgetItemCompareRole *compareRole;
    //QTreeWidgetItem *chatContactItem; //21 Sep 2018 - meiyousixin - add this 'contact' tree for one2one chat
    QTreeWidgetItem *commonItem; //27 Nov 2018 - meiyousixin  - using for all conversations
	QTreeWidgetItem *getTreeWidgetItem(ChatLobbyId);
	QTreeWidgetItem *getTreeWidgetItemForChatId(ChatId);

	ChatTabWidget *tabWidget ;

	std::map<ChatLobbyId,ChatLobbyInfoStruct> _lobby_infos ;
	std::map<std::string,ChatOne2OneInfoStruct> _chatOne2One_infos ; // 22 Sep 2018 - meiyousixin - add this for containing all one2one chat widget

	std::map<QTreeWidgetItem*,time_t> _icon_changed_map ;

    bool m_bProcessSettings;


    /** Defines the actions for the header context menu */
    QAction* showUserCountAct;
	QAction* showTopicAct;
	QAction* showSubscribeAct;
	int getNumColVisible();

	ChatLobbyUserNotify* myChatLobbyUserNotify;

	QAbstractButton* myInviteYesButton;
	GxsIdChooser* myInviteIdChooser;

	/* UI - from Designer */
    Ui::ChatLobbyWidget* ui;

	void showContactChat(QTreeWidgetItem *item);
    void showContactChatMVC(std::string chatIdStr);     //for MVC GUI
    void showGroupChatMVC(ChatLobbyId lobbyId);       //for MVC GUI
    void getHistoryForRecentList();
    void resetAvatarForContactItem(const ChatId &chatId);

    std::set<ChatId> recentUnreadListOfChatId;
    QPixmap currentStatusIcon(RsPeerId peerId, QFont& gpgFontOut);


    std::map<ChatLobbyId,ChatLobbyInfo> _groupchat_infos;

    SmartListModel* smartListModel_;

    ChatId lastChatId;      //unseenp2p - for saving the current/last chat when user close the app
    bool alreadyOpenLastChatWindow;
    bool receiveGroupInvite;
};

