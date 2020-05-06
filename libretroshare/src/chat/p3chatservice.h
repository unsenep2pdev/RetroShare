/*******************************************************************************
 * libretroshare/src/chat: chatservice.h                                       *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright 2004-2008 by Robert Fernie.                                       *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Lesser General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Lesser General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Lesser General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#ifndef SERVICE_CHAT_HEADER
#define SERVICE_CHAT_HEADER

#include <list>
#include <string>
#include <vector>

#include "rsitems/rsmsgitems.h"
#include "services/p3service.h"
#include "pqi/pqiservicemonitor.h"
#include "chat/distantchat.h"
#include "chat/distributedchat.h"
#include "retroshare/rsmsgs.h"
#include "gxstrans/p3gxstrans.h"
#include "util/rsdeprecate.h"

//gxs observer (intercommuncation between two services (chat and gxschat)
#include "gxs/rsnxsobserver.h"

class p3ServiceControl;
class p3LinkMgr;
class p3HistoryMgr;

typedef RsPeerId ChatLobbyVirtualPeerId ;

//!The basic Chat service.
 /**
  *
  * Can be used to send and receive chats, immediate status (using notify), avatars, and custom status
  * This service uses rsnotify (callbacks librs clients (e.g. rs-gui))
  * @see NotifyBase
  */
class   p3ChatService :
        public p3Service, public DistantChatService, public DistributedChatService, virtual public p3Config,
        public pqiServiceMonitor /*, GxsTransClient  */
{
public:
    p3ChatService(p3ServiceControl *cs, p3IdService *pids, p3LinkMgr *cm, p3HistoryMgr *historyMgr, RsNxsChatObserver *gxsChatObs);

	virtual RsServiceInfo getServiceInfo();

	/***** overloaded from p3Service *****/
	/*!
		 * This retrieves all chat msg items and also (important!)
		 * processes chat-status items that are in service item queue. chat msg item requests are also processed and not returned
		 * (important! also) notifications sent to notify base  on receipt avatar, immediate status and custom status
		 * : notifyCustomState, notifyChatStatus, notifyPeerHasNewAvatar
		 * @see NotifyBase
		 */
	virtual int tick();
    /*************** GXS Chat Service and Callback  ************/
    static const uint32_t FRAGMENT_SIZE;

    void handleRecvGxsChatMessage(GxsNxsChatMsgItem *item);
    void handleRecvGxsChatGroup(GxsNxsChatGroupItem *item);
    void handleRecvGxsChatPublishKey(GxsNxsGroupPublishKeyItem *item);

    void sendGxsChat(GxsNxsChatMsgItem *si, std::list<RsPeerId>& ids);
    void sendGxsPubChat(GxsNxsChatMsgItem *si);

	/*************** pqiMonitor callback ***********************/
	virtual void statusChange(const std::list<pqiServicePeer> &plist);

	/*!
		 * public chat sent to all peers
		 */
	void sendPublicChat(const std::string &msg);

	/********* RsMsgs ***********/
	/*!
	 * Send a chat message.
	 * @param destination where to send the chat message
	 * @param msg the message
	 * @see ChatId
	 */
	bool sendChat(ChatId destination, std::string msg);

	/*!
		 * chat is sent to specifc peer
		 * @param id peer to send chat msg to
		 */
	bool	sendPrivateChat(const RsPeerId &id, const std::string &msg);

	/**
	 * can be used to send 'immediate' status msgs, these status updates are
	 * meant for immediate use by peer (not saved by rs) e.g currently used to
	 * update user when a peer 'is typing' during a chat */
	void sendStatusString( const ChatId& id, const std::string& status_str );

	/**
	 * @brief clearChatLobby: Signal chat was cleared by GUI.
	 * @param id: Chat id cleared.
	 */
	virtual void clearChatLobby(const ChatId& id);

    //unseenp2p - for MVC
    virtual void saveContactOrGroupChatToModelData(std::string displayName, std::string nickInGroupChat,
                                                   unsigned int UnreadMessagesCount, unsigned int lastMsgDatetime, std::string lastMessage,bool isOtherLastMsg,
                                                   int contactType, int groupChatType, std::string rsPeerIdStr, ChatLobbyId chatLobbyId, std::string uId);
    virtual void removeContactOrGroupChatFromModelData(std::string uId);
    virtual std::vector<conversationInfo> getConversationItemList();

    virtual void updateRecentTimeOfItemInConversationList(std::string uId, std::string nickInGroupChat, long long lastMsgDatetime, std::string textmsg, bool isOtherMsg );
    virtual void sortConversationItemListByRecentTime();
    virtual void updateUnreadNumberOfItemInConversationList(std::string uId, unsigned int unreadNumber, bool isReset);
    virtual std::string getSeletedUIdBeforeSorting(int row);
    virtual int getIndexFromUId(std::string uId);
    virtual bool isChatIdInConversationList(std::string uId);
    virtual void setConversationListMode(uint32_t mode);
    virtual uint32_t getConversationListMode();
    virtual void setSearchFilter(const std::string &filtertext);
    virtual std::vector<conversationInfo> getSearchFilteredConversationItemList();


	/*!
		 * send to all peers online
		 *@see sendStatusString()
		 */
	void  sendGroupChatStatusString(const std::string& status_str) ;

	/*!
		 * this retrieves custom status for a peers, generate a requests to the peer
		 * @param peer_id the id of the peer you want status string for
		 */
	std::string getCustomStateString(const RsPeerId& peer_id) ;

	/*!
		 * sets the client's custom status, generates 'status available' item sent to all online peers
		 */
	void  setOwnCustomStateString(const std::string&) ;

	/*!
		 * @return client's custom string
		 */
	std::string getOwnCustomStateString() ;

	/*! gets the peer's avatar in jpeg format, if available. Null otherwise. Also asks the peer to send
		* its avatar, if not already available. Creates a new unsigned char array. It's the caller's
		* responsibility to delete this ones used.
		*/
	void getAvatarJpegData(const RsPeerId& peer_id,unsigned char *& data,int& size) ;

	/*!
		 * Sets the avatar data and size for client's account
		 * @param data is copied, so should be destroyed by the caller
		 */
	void setOwnAvatarJpegData(const unsigned char *data,int size) ;

	/*!
		 * Gets the avatar data for clients account
		 * data is in jpeg format
		 */
	void getOwnAvatarJpegData(unsigned char *& data,int& size) ;

	/*!
		 * Return the max message size for security forwarding
		 * @param type RS_CHAT_TYPE_...
		 * return 0 unlimited
		 */
	static uint32_t getMaxMessageSecuritySize(int type);

	/*!
		 * Checks message security, especially remove billion laughs attacks
		 */

	static bool checkForMessageSecurity(RsChatMsgItem *) ;

	/*!
		 * @param clear private chat queue
		 */
	bool clearPrivateChatQueue(bool incoming, const RsPeerId &id);

	virtual bool initiateDistantChatConnexion( const RsGxsId& to_gxs_id,
	                                           const RsGxsId& from_gxs_id,
	                                           DistantChatPeerId &pid,
	                                           uint32_t& error_code,
	                                           bool notify = true );

	/// @see GxsTransClient::receiveGxsTransMail(...)
	virtual bool receiveGxsTransMail( const RsGxsId& authorId,
	                                  const RsGxsId& recipientId,
	                                  const uint8_t* data, uint32_t dataSize );

	/// @see GxsTransClient::notifySendMailStatus(...)
	virtual bool notifyGxsTransSendStatus( RsGxsTransId mailId,
	                                       GxsTransSendStatus status );


protected:
	/************* from p3Config *******************/
	virtual RsSerialiser *setupSerialiser() ;

	/*!
		 * chat msg items and custom status are saved
		 */
	virtual bool saveList(bool& cleanup, std::list<RsItem*>&) ;
	virtual void saveDone();
	virtual bool loadList(std::list<RsItem*>& load) ;

	// accepts virtual peer id
	bool isOnline(const RsPeerId &pid) ;

    /// This is to be used by subclasses/parents to call IndicateConfigChanged()
    virtual void triggerConfigSave()  { IndicateConfigChanged() ; }

	/// Same, for storing messages in incoming list
	RS_DEPRECATED virtual void locked_storeIncomingMsg(RsChatMsgItem *) ;

private:

    typedef std::vector<RsNxsGrp*> GrpFragments;
    typedef std::vector<RsNxsMsg*> MsgFragments;

    /*!
     * Fragment a message into individual fragments which are at most 150kb
     * @param msg message to fragment
     * @param msgFragments fragmented message
     * @return false if fragmentation fails true otherwise
     */
    bool fragmentMsg(RsNxsMsg& msg, MsgFragments& msgFragments) const;

    /*!
     * Fragment a group into individual fragments which are at most 150kb
     * @param grp group to fragment
     * @param grpFragments fragmented group
     * @return false if fragmentation fails true other wise
     */
    bool fragmentGrp(RsNxsGrp& grp, GrpFragments& grpFragments) const;

    /*!
     * Fragment a message into individual fragments which are at most 150kb
     * @param msg message to fragment
     * @param msgFragments fragmented message
     * @return NULL if not possible to reconstruct message from fragment,
     *              pointer to defragments nxs message is possible
     */
    RsNxsMsg* deFragmentMsg(MsgFragments& msgFragments) const;

    /*!
     * Fragment a group into individual fragments which are at most 150kb
     * @param grp group to fragment
     * @param grpFragments fragmented group
     * @return NULL if not possible to reconstruct group from fragment,
     *              pointer to defragments nxs group is possible
     */
    RsNxsGrp* deFragmentGrp(GrpFragments& grpFragments) const;


    /*!
     * Note that if all fragments for a message are not found then its fragments are dropped
     * @param fragments message fragments which are not necessarily from the same message
     * @param partFragments the partitioned fragments (into message ids)
     */
    void collateMsgFragments(MsgFragments &fragments, std::map<RsGxsMessageId, MsgFragments>& partFragments) const;

    /*!
     * Note that if all fragments for a group are not found then its fragments are dropped
     * @param fragments group fragments which are not necessarily from the same group
     * @param partFragments the partitioned fragments (into message ids)
     */
    void collateGrpFragments(GrpFragments fragments, std::map<RsGxsGroupId, GrpFragments>& partFragments) const;



	RsMutex mChatMtx;

	class AvatarInfo ;
	class StateStringInfo ;

	// Receive chat queue
	void receiveChatQueue();
	void handleIncomingItem(RsItem *);	// called by the former, and turtle handler for incoming encrypted items

	virtual void sendChatItem(RsChatItem *) ;

	void initChatMessage(RsChatMsgItem *c, ChatMessage& msg);

	/// Send avatar info to peer in jpeg format.
	void sendAvatarJpegData(const RsPeerId& peer_id) ;

	/// Send custom state info to peer
	void sendCustomState(const RsPeerId& peer_id);

	/// Receive the avatar in a chat item, with RS_CHAT_RECEIVE_AVATAR flag.
	void receiveAvatarJpegData(RsChatAvatarItem *ci) ;	// new method
	void receiveStateString(const RsPeerId& id,const std::string& s) ;

	/// methods for handling various Chat items.
	virtual bool handleRecvChatMsgItem(RsChatMsgItem *&item) ;			// NULL-ifies the item if memory ownership is taken
    
	void handleRecvChatStatusItem(RsChatStatusItem *item) ;
	void handleRecvChatAvatarItem(RsChatAvatarItem *item) ;

	/// Sends a request for an avatar to the peer of given id
	void sendAvatarRequest(const RsPeerId& peer_id) ;

	/// Send a request for custom status string
	void sendCustomStateRequest(const RsPeerId& peer_id);

	/// called as a proxy to sendItem(). Possibly splits item into multiple items of size lower than the maximum item size.
	//void checkSizeAndSendMessage(RsChatLobbyMsgItem *item) ;
	void checkSizeAndSendMessage(RsChatMsgItem *item) ;	// keep for compatibility for a few weeks.

	/// Called when a RsChatMsgItem is received. The item may be collapsed with any waiting partial chat item from the same peer.
        /// if so, the chat item will be turned to NULL
	bool locked_checkAndRebuildPartialMessage(RsChatMsgItem *&) ;

	RsChatAvatarItem *makeOwnAvatarItem() ;
	RsChatStatusItem *makeOwnCustomStateStringItem() ;

	p3ServiceControl *mServiceCtrl;
	p3LinkMgr *mLinkMgr;
	p3HistoryMgr *mHistoryMgr;

	/// messages waiting to be send when peer comes online
	typedef std::map<uint64_t, RsChatMsgItem *> outMP;
	outMP privateOutgoingMap;

	AvatarInfo *_own_avatar ;
	std::map<RsPeerId,AvatarInfo *> _avatars ;
	std::map<RsPeerId,RsChatMsgItem *> _pendingPartialMessages;

	std::string _custom_status_string ;
	std::map<RsPeerId,StateStringInfo> _state_strings ;

	RsChatSerialiser *_serializer;

	struct DistantEndpoints { RsGxsId from; RsGxsId to; };
	typedef std::map<DistantChatPeerId, DistantEndpoints> DIDEMap;
	DIDEMap mDistantGxsMap;
	RsMutex mDGMutex;

    //p3GxsTrans& mGxsTransport;
    RsNxsChatObserver *gxsChatSync;
    uint16_t mServType;


    //unseenp2p - for MVC
    std::vector<conversationInfo> conversationItemList;
    std::vector<conversationInfo> filtererConversationItemList;
    uint32_t conversationListMode ; // CONVERSATION_MODE_WITHOUT_FILTER  or
                                    // CONVERSATION_MODE_WITH_SEARCH_FILTER
    std::string filter_text;
};

class p3ChatService::StateStringInfo
{
   public:
	  StateStringInfo()
	  {
		  _custom_status_string = "" ;	// the custom status string of the peer
		  _peer_is_new = false ;			// true when the peer has a new avatar
		  _own_is_new = false ;				// true when I myself a new avatar to send to this peer.
	  }

	  std::string _custom_status_string ;
	  int _peer_is_new ;			// true when the peer has a new avatar
	  int _own_is_new ;			// true when I myself a new avatar to send to this peer.
};

#endif // SERVICE_CHAT_HEADER

