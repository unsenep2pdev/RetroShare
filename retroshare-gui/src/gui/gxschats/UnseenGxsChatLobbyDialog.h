/****************************************************************
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2006, crypton
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
 ****************************************************************/


#ifndef _UNSEENGXSCHATLOBBYDIALOG_H
#define _UNSEENGXSCHATLOBBYDIALOG_H

#include "ui_UnseenGxsChatLobbyDialog.h"
//#include "ui_ChatLobbyDialog.h"
#include "gui/common/RSTreeWidgetItem.h"
#include "gui/chat/ChatDialog.h"

#include <retroshare/rsgxsifacetypes.h>
#include "util/TokenQueue.h"

#include "gui/gxs/GxsMessageFramePostWidget.h"
#include "gui/feeds/FeedHolder.h"
#include "retroshare/rsgxschats.h"

class GxsMessageFramePostThread2;
class UIStateHelper;

Q_DECLARE_METATYPE(gxsChatId)
Q_DECLARE_METATYPE(RsGxsChatMsg)


class GxsIdChooser ;
class QToolButton;
class QWidgetAction;
class ChatId;
class gxsChatId;

//for gxs things
class GxsChatPostItem;
class RsGxsIfaceHelper;
class RsGxsUpdateBroadcastBase;
typedef uint32_t TurtleRequestId;

//customize this class so it can inherit from the TokenResponse
class UnseenGxsChatLobbyDialog: public ChatDialog, public TokenResponse
{
	Q_OBJECT 

	friend class ChatDialog;
    friend class GxsMessageFramePostThread2;

public:
    void displayLobbyEvent(int event_type, const RsGxsId &gxs_id, const QString& str);

	virtual void showDialog(uint chatflags);
	virtual ChatWidget *getChatWidget();
	virtual bool hasPeerStatus() { return false; }
	virtual bool notifyBlink();
    void setIdentity(const RsGxsId& gxs_id);
    bool isParticipantMuted(const RsGxsId &participant);
	ChatLobbyId id() const { return lobbyId ;}
	ChatId	chatId() const {return cId;}
	void sortParcipants();

    //unseenp2p  - move from private to public
    void updateParticipantsList();
    //unseenp2p - move from protected to public
    void processSettings(bool load);
    //unseenp2p - add for gxs groupchat
    RsGxsGroupId groupId() const {return mGroupId; }

    void updateTitle(QString title);

private slots:
	void participantsTreeWidgetCustomPopupMenu( QPoint point );
	void textBrowserAskContextMenu(QMenu* contextMnu, QString anchorForPosition, const QPoint point);
	void inviteFriends() ;
    void leaveGxsGroupChat() ;
	void filterChanged(const QString &text);
    void showInPeopleTab();
signals:
    void gxsGroupLeave(RsGxsGroupId) ;
	void typingEventReceived(ChatLobbyId) ;
    //void messageReceived(bool incoming, ChatLobbyId lobby_id, QDateTime time, QString senderName, QString msg) ;
    //unseenp2p: for gxs groupchat
    void gxsMessageReceived(RsGxsChatMsg gxsChatMsg, bool incoming, gxsChatId groupChatId, QDateTime time, QString senderName, QString msg) ;

	void peerJoined(ChatLobbyId) ;
	void peerLeft(ChatLobbyId) ;

protected:
	/** Default constructor */
    UnseenGxsChatLobbyDialog(const ChatLobbyId& lid, QWidget *parent = 0, Qt::WindowFlags flags = 0);

    //unseenp2p - for gxs groupchat
     UnseenGxsChatLobbyDialog(const RsGxsGroupId& groupId, QWidget *parent = 0, Qt::WindowFlags flags = 0);

	/** Default destructor */
    virtual ~UnseenGxsChatLobbyDialog();

    //virtual void init(const ChatId &id, const QString &title);
     //unseenp2p
     virtual void init(const gxsChatId &id, const QString &title);

	virtual bool canClose();
    virtual void addChatMsg(const ChatMessage &msg);

protected slots:
    void changeNickname();
	void changeParticipationState();
    void distantChatParticipant();
    void participantsTreeWidgetDoubleClicked(QTreeWidgetItem *item, int column);
    void sendMessage();
	void voteParticipant();

private:

	void initParticipantsContextMenu(QMenu* contextMnu, QList<RsGxsId> idList);
	
	void filterIds();

    QString getParticipantName(const RsGxsId& id) const;
    void muteParticipant(const RsGxsId& id);
    void unMuteParticipant(const RsGxsId& id);
    bool isNicknameInLobby(const RsGxsId& id);
	
	ChatLobbyId lobbyId;
	//meiyousixin - add this one to identify the chat id
	ChatId cId;
	QString _lobby_name ;
	time_t lastUpdateListTime;

        RSTreeWidgetItemCompareRole *mParticipantCompareRole ;

    QToolButton *inviteFriendsButton ;
    QToolButton *membersListButton ;
    QToolButton *membersListButton2 ;
    QToolButton *unsubscribeButton ;

	/** Qt Designer generated object */
    Ui::UnseenGxsChatLobbyDialog ui;
	
	/** Ignored Users in Chatlobby by nickname until we had implemented Peer Ids in ver 0.6 */
    std::set<RsGxsId> mutedParticipants;

    QAction *muteAct;
    QAction *votePositiveAct;
    QAction *voteNeutralAct;
    QAction *voteNegativeAct;
    QAction *distantChatAct;
    QAction *actionSortByName;
    QAction *actionSortByActivity;
    QWidgetAction *checkableAction;
    QAction *sendMessageAct;
    QAction *showInPeopleAct;

    GxsIdChooser *ownIdChooser ;
    //icons cache
    QIcon bullet_red_128, bullet_grey_128, bullet_green_128, bullet_yellow_128;
    QIcon bullet_unknown_128;

    //unseenp2p - add for gxs groupchat
    //RsGxsGroupId  mGXSGroupId;
    std::set<GxsChatMember> old_participating_friends;

    ////////////////////////////////////////////////////////////////////////////////////////////
    ///             THESE ARE FOR 4 CLASS THAT THIS CLASS NEED TO DO                ////////////
    ////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////
    ///     ALL  from RsGxsUpdateBroadcastWidget
    /////////////////////////////////////////////////////////////////////////////////////////////
//copy from RsGxsUpdateBroadcastWidget - to get all the RsGxsGroupId, RsGxsMessageId, loadRequest
public:
    void fillComplete();
    void setUpdateWhenInvisible(bool update);
    const std::set<RsGxsGroupId> &getGrpIds();
    const std::set<RsGxsGroupId> &getGrpIdsMeta();
    void getAllGrpIds(std::set<RsGxsGroupId> &grpIds);
    const std::map<RsGxsGroupId, std::set<RsGxsMessageId> > &getMsgIds();
    const std::map<RsGxsGroupId, std::set<RsGxsMessageId> > &getMsgIdsMeta();
    void getAllMsgIds(std::map<RsGxsGroupId, std::set<RsGxsMessageId> > &msgIds);
    const std::set<TurtleRequestId>& getSearchResults() ;

    RsGxsIfaceHelper *interfaceHelper() { return mInterfaceHelper; }
    virtual void updateDisplay(bool complete);
protected:
    virtual void showEvent(QShowEvent *event);
    // This is overloaded in subclasses. -> now it will in one class only
    //virtual void updateDisplay(bool complete); move to public to call from the UnseenGxsGroupFrameDialog

private slots:
    void fillDisplay(bool complete);

private:
    RsGxsUpdateBroadcastBase *mBase;
    RsGxsIfaceHelper *mInterfaceHelper;
    bool showAllPostOnlyOnce;
    std::set<RsGxsMessageId> allDownloadedMsgs;
    rstime_t      mLatestHistoryMsgTimestamp;
//END of copy from RsGxsUpdateBroadcastWidget


    ////////////////////////////////////////////////////////////////////////////////////////////
    ///     ALL  from GxsMessageFrameWidget
    /////////////////////////////////////////////////////////////////////////////////////////////

public:

    const RsGxsGroupId &groupId();  //need to consider with the
    void setGroupId(const RsGxsGroupId &groupId);
    void setAllMessagesRead(bool read);
    void groupIdChanged();
    bool isLoading();
    bool isWaiting();

    /* GXS functions */
    uint32_t nextTokenType() { return ++mNextTokenType; }
    void GxsMessageFrameWidgetloadRequest(const TokenQueue *queue, const TokenRequest &req);

signals:
    void groupChanged(QWidget *widget);
    void waitingChanged(QWidget *widget);
    void loadComment(const RsGxsGroupId &groupId, const QVector<RsGxsMessageId>& msg_versions,const RsGxsMessageId &msgId, const QString &title);

protected:
    void setAllMessagesReadDo(bool read, uint32_t &token);
protected:
    TokenQueue *mTokenQueue;
    UIStateHelper *mStateHelper;

    /* Set read status */
    uint32_t mTokenTypeAcknowledgeReadStatus;
    uint32_t mAcknowledgeReadStatusToken;

private:
    RsGxsGroupId mGroupId; /* current group */
    uint32_t mNextTokenType;
//END of copy from GxsMessageFrameWidget

    ////////////////////////////////////////////////////////////////////////////////////////////
    ///     ALL  from GxsMessageFramePostWidget
    /////////////////////////////////////////////////////////////////////////////////////////////
//copy from GxsMessageFramePostWidget
public:


    /* GxsMessageFrameWidget */
    //need to remove because of the same
    /* GxsMessageFrameWidget */

    QString groupName(bool withUnreadCount);
    bool navigate(const RsGxsMessageId& msgId);

    /* GXS functions */
    virtual void loadRequest(const TokenQueue *queue, const TokenRequest &req);

    int subscribeFlags() { return mSubscribeFlags; }

protected:
    /* RsGxsUpdateBroadcastWidget */
    //virtual void updateDisplay(bool complete);

     void groupNameChanged(const QString &/*name*/);


     void clearPosts();
     bool navigatePostItem(const RsGxsMessageId& msgId);

    /* Thread functions */
     bool useThread() { return false; }
     void fillThreadCreatePost(const QVariant &/*post*/, bool /*related*/, int /*current*/, int /*count*/);

    /* GXS functions */
    void requestGroupData();
    void loadGroupData(const uint32_t &token);
    bool insertGroupData(const uint32_t &token, RsGroupMetaData &metaData);

    void requestAllPosts();
    void loadAllPosts(const uint32_t &token);
    void insertAllPosts(const uint32_t &token, GxsMessageFramePostThread2 *thread);

    void requestPosts(const std::set<RsGxsMessageId> &msgIds);
    void loadPosts(const uint32_t &token);
    void insertPosts(const uint32_t &token);

private slots:
    void fillThreadFinished();
    void fillThreadAddPost(const QVariant &post, bool related, int current, int count);
protected:
    uint32_t mTokenTypeGroupData;
    uint32_t mTokenTypeAllPosts;
    uint32_t mTokenTypePosts;
    RsGxsMessageId mNavigatePendingMsgId;

private:
    QString mGroupName;
    int mSubscribeFlags;
    GxsMessageFramePostThread2 *mFillThread;

    ////////////////////////////////////////////////////////////////////////////////////////////
    ///     ALL  from GxsChatPostsWidget - highest class
    /////////////////////////////////////////////////////////////////////////////////////////////
//copy from GxsChatPostsWidget - highest class
public:

    /* GxsMessageFrameWidget */
   // virtual QIcon groupIcon();

    /* FeedHolder */
    QScrollArea *getScrollArea();
    void deleteFeedItem(QWidget *item, uint32_t type);
    void openChat(const RsPeerId& peerId);
    void openComments(uint32_t type, const RsGxsGroupId &groupId, const QVector<RsGxsMessageId> &msg_versions, const RsGxsMessageId &msgId, const QString &title);


private slots:
    void createMsg();
    void toggleAutoDownload();
    void subscribeGroup(bool subscribe);
    void filterChanged(int filter);
    void setViewMode(int viewMode);
    void settingsChanged();

private:


    void setAutoDownload(bool autoDl);

    int viewMode();

    void insertChannelDetails(const RsGxsChatGroup &group);
    void insertGxsChatPosts(std::vector<RsGxsChatMsg> &posts, GxsMessageFramePostThread2 *thread, bool related);

    void createPostItem(const RsGxsChatMsg &post, bool related);

private:
    QAction *mAutoDownloadAction;

    bool mUseThread;

//END of copy from GxsChatPostsWidget

//RS GXS workflow

// GxsChannelPostsWidget -> GxsMessageFramePostWidget -> GxsMessageFrameWidget -> RsGxsUpdateBroadcastWidget (->QWidget) + TokenResponse
//                                     |
//                          GxsMessageFramePostThread

//Because UnseenGxsChatLobbyDialog get all these 4 classes, so it will take all variables and functions from these 4 classes

// GxsGroupFrameDialog -> RsGxsUpdateBroadcastPage (-> MainPage) + TokenResponse


};

class GxsMessageFramePostThread2 : public QThread
{
    Q_OBJECT

public:
    GxsMessageFramePostThread2(uint32_t token, UnseenGxsChatLobbyDialog  *parent);
    ~GxsMessageFramePostThread2();

    void run();
    void stop(bool waitForStop);
    bool stopped() { return mStopped; }

    void emitAddPost(const QVariant &post, bool related, int current, int count);

signals:
    void addPost(const QVariant &post, bool related, int current, int count);

private:
    uint32_t mToken;
    UnseenGxsChatLobbyDialog *mParent;
    volatile bool mStopped;
};

#endif
