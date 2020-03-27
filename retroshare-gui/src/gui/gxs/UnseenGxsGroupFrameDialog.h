/****************************************************************
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2008 Robert Fernie
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

#ifndef _UNSEENGXSGROUPFRAMEDIALOG_H
#define _UNSEENGXSGROUPFRAMEDIALOG_H

#include "gui/gxs/RsGxsUpdateBroadcastPage.h"
#include "RsAutoUpdatePage.h"
#include "gui/RetroShareLink.h"
#include "gui/settings/rsharesettings.h"
#include "util/RsUserdata.h"

#include <inttypes.h>

#include "util/TokenQueue.h"
#include "GxsIdTreeWidgetItem.h"
#include "UnseenGxsGroupDialog.h"
#include "gui/gxschats/UnseenGxsSmartlistmodel.h"

#include "gui/gxschats/GxsChatUserNotify.h"

#define GXSCONVERSATION_MODE_WITHOUT_FILTER                    0x0000
#define GXSCONVERSATION_MODE_WITH_SEARCH_FILTER                0x0001

namespace Ui {
class UnseenGxsGroupFrameDialog;
}

class UnseenGxsGroupDialog;
class UnseenGxsChatLobbyDialog;
//class UnseenGroupTreeWidget;
class UnseenGxsSmartListView;
class GroupTreeWidget;
class GroupItemInfo;
class GxsMessageFrameWidget;
class UIStateHelper;
class RsGxsCommentService;
class GxsCommentDialog;

class UnseenGroupItemInfo;


struct UnseenGxsChatLobbyInfoStruct
{
    UnseenGxsChatLobbyDialog *dialog ;
    time_t last_typing_event ;
};

class UnseenGxsGroupFrameDialog : public RsGxsUpdateBroadcastPage, public TokenResponse
{
	Q_OBJECT

public:
	enum TextType {
		TEXT_NAME,
		TEXT_NEW,
		TEXT_TODO,
		TEXT_YOUR_GROUP,
		TEXT_SUBSCRIBED_GROUP,
		TEXT_POPULAR_GROUP,
		TEXT_OTHER_GROUP
	};

	enum IconType {
		ICON_NAME,
		ICON_NEW,
		ICON_YOUR_GROUP,
		ICON_SUBSCRIBED_GROUP,
		ICON_POPULAR_GROUP,
		ICON_OTHER_GROUP,
		ICON_SEARCH,
		ICON_DEFAULT
	};

public:
    UnseenGxsGroupFrameDialog(RsGxsIfaceHelper *ifaceImpl, QWidget *parent = 0,bool allow_dist_sync=false);
    virtual ~UnseenGxsGroupFrameDialog();

	bool navigate(const RsGxsGroupId &groupId, const RsGxsMessageId& msgId);

	// Callback for all Loads.
	virtual void loadRequest(const TokenQueue *queue, const TokenRequest &req);

	virtual QString getHelpString() const =0;

	virtual void getGroupList(std::map<RsGxsGroupId,RsGroupMetaData> &groups) ;

    void groupInfoToUnseenGroupItemInfo(const RsGroupMetaData &groupInfo, UnseenGroupItemInfo &groupItemInfo, const RsUserdata */*userdata*/);
    void addChatPage(UnseenGxsChatLobbyDialog *);

    void setCurrentChatPage(UnseenGxsChatLobbyDialog *d);
    QString itemIdAt(QPoint &point);
    UnseenGroupItemInfo groupItemIdAt(QPoint &point);
    UnseenGroupItemInfo groupItemIdAt(QString groupId);
    void sortGxsConversationListByRecentTime();

    //unseenp2p - overload with the parent
    virtual UserNotify *getUserNotify(QObject *parent);

protected:
	virtual void showEvent(QShowEvent *event);
	virtual void updateDisplay(bool complete);

	const RsGxsGroupId &groupId() { return mGroupId; }
	void setSingleTab(bool singleTab);
	void setHideTabBarWithOneTab(bool hideTabBarWithOneTab);
	bool getCurrentGroupName(QString& name);
	virtual RetroShareLink::enumType getLinkType() = 0;
	virtual GroupFrameSettings::Type groupFrameSettingsType() { return GroupFrameSettings::Nothing; }
	virtual void groupInfoToGroupItemInfo(const RsGroupMetaData &groupInfo, GroupItemInfo &groupItemInfo, const RsUserdata *userdata);
    void groupInfoToUnseenGroupItemInfo2(const RsGxsChatGroup &groupInfo, UnseenGroupItemInfo &groupItemInfo, const RsUserdata */*userdata*/);
    virtual void checkRequestGroup(const RsGxsGroupId& /* grpId */) {}	// overload this one in order to retrieve full group data when the group is browsed

private slots:
	void todo();

	/** Create the context popup menu and it's submenus */
	void groupTreeCustomPopupMenu(QPoint point);
	void settingsChanged();
    void setSyncPostsDelay();
    void setStorePostsDelay();

	void restoreGroupKeys();
	void newGroup();
    void distantRequestGroupData();

	void changedCurrentGroup(const QString &groupId);
	void groupTreeMiddleButtonClicked(QTreeWidgetItem *item);
	void openInNewTab();
	void messageTabCloseRequested(int index);
	void messageTabChanged(int index);
	void messageTabInfoChanged(QWidget *widget);
	void messageTabWaitingChanged(QWidget *widget);

	void copyGroupLink();

	void subscribeGroup();
	void unsubscribeGroup();

	void showGroupDetails();
	void editGroupDetails();

	void markMsgAsRead();
	void markMsgAsUnread();

	void sharePublishKey();

	void loadComment(const RsGxsGroupId &grpId, const QVector<RsGxsMessageId>& msg_versions,const RsGxsMessageId &most_recent_msgId, const QString &title);

    void searchNetwork(const QString &search_string) ;
	void removeAllSearches();
	void removeCurrentSearch();

    void smartListSelectionChanged(const QItemSelection  &selected, const QItemSelection  &deselected);
    void updateRecentTimeOrNewMsg(const gxsChatId&, std::string, long long, std::string, bool);

    void unsubscribeGxsGroupChat(RsGxsGroupId id);

    void updateMessageChanged(RsGxsChatMsg gxsChatMsg, bool incoming, RsGxsGroupId id, QDateTime time, QString senderName, QString msg);
    void updateGxsMsgNotify(RsGxsChatMsg gxsChatMsg, gxsChatId id, unsigned int count);

    void filterGxsItems(const QString &text);
    void filterColumnChanged(int);
private:
	virtual QString text(TextType type) = 0;
	virtual QString icon(IconType type) = 0;
	virtual QString settingsGroupName() = 0;
    //virtual TurtleRequestId distantSearch(const QString& search_string) ;

    virtual UnseenGxsGroupDialog *createNewGroupDialog(TokenQueue *tokenQueue) = 0;
    virtual UnseenGxsGroupDialog *createGroupDialog(TokenQueue *tokenQueue, RsTokenService *tokenService, UnseenGxsGroupDialog::Mode mode, RsGxsGroupId groupId) = 0;
	virtual int shareKeyType() = 0;
	virtual GxsMessageFrameWidget *createMessageFrameWidget(const RsGxsGroupId &groupId) = 0;
	virtual void groupTreeCustomActions(RsGxsGroupId /*grpId*/, int /*subscribeFlags*/, QList<QAction*> &/*actions*/) {}
	virtual RsGxsCommentService *getCommentService() { return NULL; }
	virtual QWidget *createCommentHeaderWidget(const RsGxsGroupId &/*grpId*/, const RsGxsMessageId &/*msgId*/) { return NULL; }
    virtual bool getDistantSearchResults(TurtleRequestId /* id */, std::map<RsGxsGroupId,RsGxsGroupSummary>& /* group_infos */){ return false ;}

	void initUi();

	void updateMessageSummaryList(RsGxsGroupId groupId);
	void updateSearchResults();

	void openGroupInNewTab(const RsGxsGroupId &groupId);
	void groupSubscribe(bool subscribe);

	void processSettings(bool load);

	// New Request/Response Loading Functions.
	void insertGroupsData(const std::map<RsGxsGroupId, RsGroupMetaData> &groupList, const RsUserdata *userdata);
    void insertGroupsData2(const std::map<RsGxsGroupId,RsGxsChatGroup> &groupList, const RsUserdata *userdata);

    void requestGroupSummary();
	void loadGroupSummary(const uint32_t &token);
	virtual uint32_t requestGroupSummaryType() { return GXS_REQUEST_TYPE_GROUP_META; } // request only meta data
	virtual void loadGroupSummaryToken(const uint32_t &token, std::list<RsGroupMetaData> &groupInfo, RsUserdata* &userdata); // use with requestGroupSummaryType
    virtual void loadGroupSummaryToken2(const uint32_t &token, std::list<RsGxsChatGroup> &groupInfo, RsUserdata* &userdata); // use with requestGroupSummaryType

	void requestGroupStatistics(const RsGxsGroupId &groupId);
	void loadGroupStatistics(const uint32_t &token);

	// subscribe/unsubscribe ack.
//	void acknowledgeSubscribeChange(const uint32_t &token);

	GxsMessageFrameWidget *messageWidget(const RsGxsGroupId &groupId, bool ownTab);
	GxsMessageFrameWidget *createMessageWidget(const RsGxsGroupId &groupId);

	GxsCommentDialog *commentWidget(const RsGxsMessageId &msgId);

//	void requestGroupSummary_CurrentGroup(const  RsGxsGroupId &groupId);
//	void loadGroupSummary_CurrentGroup(const uint32_t &token);

    //unseenp2p
    void selectConversation(const QModelIndex& index);
    void showGxsGroupChatMVC(gxsChatId chatId);
    void updateRecentTimeOfItemInGxsConversationList(std::string uId, std::string nickInGroupChat, long long lastMsgDatetime, std::string textmsg, bool isOtherMsg );
    bool isGroupIdInGxsConversationList(std::string uId);
    void saveGxsGroupChatInfoToModelData(const RsGxsChatGroup gxsGroupInfo, std::string nickInGroupChat, unsigned int UnreadMessagesCount, unsigned int lastMsgDatetime, std::string lastMessage, bool isOtherLastMsg);
    int getIndexFromUId(std::string uId);
    void updateUnreadNumberOfItemInGxsConversationList(std::string uId, unsigned int unreadNumber, bool isReset);

    void setConversationListMode(uint32_t mode);
    uint32_t getConversationListMode();
    void setSearchFilter(const std::string &filtertext);
    std::vector<UnseenGroupItemInfo> getSearchFilteredGxsGroupList();

protected:
	bool mCountChildMsgs; // Count unread child messages?

private:
	bool mInitialized;
	bool mInFill;
    bool mDistSyncAllowed;
	QString mSettingsName;
	RsGxsGroupId mGroupId;
	RsGxsIfaceHelper *mInterface;
	RsTokenService *mTokenService;
	TokenQueue *mTokenQueue;
	GxsMessageFrameWidget *mMessageWidget;

	QTreeWidgetItem *mYourGroups;
	QTreeWidgetItem *mSubscribedGroups;
	QTreeWidgetItem *mPopularGroups;
	QTreeWidgetItem *mOtherGroups;

	RsGxsGroupId mNavigatePendingGroupId;
	RsGxsMessageId mNavigatePendingMsgId;

	UIStateHelper *mStateHelper;

	/** Qt Designer generated object */
    Ui::UnseenGxsGroupFrameDialog *ui;

	std::map<RsGxsGroupId,RsGroupMetaData> mCachedGroupMetas;
    std::map<RsGxsGroupId,RsGxsChatGroup> mCachedChatGroupData;

    std::map<uint32_t,QTreeWidgetItem*> mSearchGroupsItems ;
    std::map<uint32_t,std::set<RsGxsGroupId> > mKnownGroups;

    std::map<RsGxsGroupId,UnseenGxsChatLobbyInfoStruct> _unseenGxsGroup_infos ;
    UnseenGxsSmartListModel* smartListModel_; //unseenp2p - use the MVC GUI for the gxs chat

    //unseenp2p GUI
    GxsChatUserNotify* myGxsChatUserNotify;

    std::vector<UnseenGroupItemInfo> allGxsGroupList; //allGxsGroupList = adminList + subList

    std::vector<UnseenGroupItemInfo> filteredGxsGroupList;
    uint32_t conversationListMode ; // CONVERSATION_MODE_WITHOUT_FILTER  or
                                    // CONVERSATION_MODE_WITH_SEARCH_FILTER
    std::string filter_text;

};

#endif
