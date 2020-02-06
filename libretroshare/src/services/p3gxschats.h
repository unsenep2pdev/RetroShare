#ifndef P3_GXSCHATS_SERVICE_HEADER
#define P3_GXSCHATS_SERVICE_HEADER

#include "retroshare/rsgxschats.h"
#include "services/p3gxscommon.h"
#include "gxs/rsgenexchange.h"
#include "gxs/gxstokenqueue.h"
#include "util/rstickevent.h"

#include <map>
#include <string>

//chatservice
#include "chat/p3chatservice.h"
#include "rsitems/rsmsgitems.h"
#include "pqi/pqiservicemonitor.h"
#include "chat/distantchat.h"
#include "chat/distributedchat.h"
#include "retroshare/rsmsgs.h"
#include "gxstrans/p3gxstrans.h"
#include "util/rsdeprecate.h"

class p3ServiceControl;
class p3LinkMgr;
class p3HistoryMgr;



class SSGxsChatGroup
{
    public:
    SSGxsChatGroup(): mAutoDownload(false), mDownloadDirectory("") {}
    bool load(const std::string &input);
    std::string save() const;

    bool mAutoDownload;
    std::string mDownloadDirectory;
};

class p3GxsChats: public RsGenExchange,  public RsGxsChats,
    public GxsTokenQueue, virtual public p3Config, public RsNxsChatObserver,
    public RsTickEvent	/* only needed for testing - remove after */
{
public:
    p3GxsChats( RsGeneralDataService* gds, RsNetworkExchangeService* nes,
                   RsGixs* gixs);
    virtual RsServiceInfo getServiceInfo();

    virtual void service_tick();

    virtual void setp3ChatService(p3ChatService *chatsrv)
    {
        if(mChatSrv != NULL)
            std::cerr << "(EE) Cannot override existing p3chatservice. Make sure it has been deleted otherwise." << std::endl;
        else
            mChatSrv = chatsrv;
    }

protected:


    virtual RsSerialiser* setupSerialiser();                            // @see p3Config::setupSerialiser()
    virtual bool saveList(bool &cleanup, std::list<RsItem *>&saveList); // @see p3Config::saveList(bool &cleanup, std::list<RsItem *>&)
    virtual bool loadList(std::list<RsItem *>& loadList);               // @see p3Config::loadList(std::list<RsItem *>&)

    virtual TurtleRequestId turtleGroupRequest(const RsGxsGroupId& group_id);
    virtual TurtleRequestId turtleSearchRequest(const std::string& match_string);
    virtual bool retrieveDistantSearchResults(TurtleRequestId req, std::map<RsGxsGroupId, RsGxsGroupSummary> &results) ;
    virtual bool clearDistantSearchResults(TurtleRequestId req);
    virtual bool retrieveDistantGroup(const RsGxsGroupId& group_id,RsGxsChatGroup& distant_group);

    // Overloaded to cache new groups.
    // no tokens... should be cached.
    virtual bool setChannelAutoDownload(const RsGxsGroupId &groupId, bool enabled);
    virtual	bool getChannelAutoDownload(const RsGxsGroupId &groupid, bool& enabled);
    virtual bool setChannelDownloadDirectory(const RsGxsGroupId &groupId, const std::string& directory);
    virtual bool getChannelDownloadDirectory(const RsGxsGroupId &groupId, std::string& directory);


    //p3chatservice callback to process gxs messages/groups

    virtual void receiveNewChatMesesage(std::vector<GxsNxsChatMsgItem*>& messages);
    virtual void receiveNewChatGroup(std::vector<GxsNxsChatGroupItem*>& groups);
    virtual void receiveNotifyMessages(std::vector<RsNxsNotifyChat*>& notifyMessages);

    virtual void notifyReceiveChatInvite(const RsGxsGroupId &grpId) {}
    virtual void notifyReceiveChatPublishKey(const RsGxsGroupId &grpId) {}
    virtual void notifyChangedChatGroupStats(const RsGxsGroupId &grpId) {}


    virtual RsGenExchange::ServiceCreate_Return service_CreateGroup(RsGxsGrpItem* grpItem, RsTlvSecurityKeySet& keySet);
    virtual RsGenExchange::ServiceCreate_Return service_PublishGroup(RsNxsGrp *grp);
    virtual RsGenExchange::ServiceCreate_Return service_CreateMessage(RsNxsMsg* msg);

    virtual ServiceCreate_Return service_RecvBounceGroup(RsNxsGrp *grp, bool isNew);
    virtual ServiceCreate_Return service_RecvBounceMessage(RsNxsMsg* msg, bool isNew);


    virtual void notifyReceivePublishKey(const RsGxsGroupId &grpId, const RsPeerId &peerid);
    virtual void handleBounceShareKey();
    virtual void processRecvBounceGroup();
    virtual void processRecvBounceMessage();
    virtual void processRecvBounceNotify();
    virtual void processRecvBounceNotifyClear();
    virtual void publishBounceNotifyMessage(RsNxsNotifyChat * notifyMsg);
    virtual void publishNotifyMessage(const RsGxsGroupId &grpId,std::pair<std::string,std::string> &command);

virtual void notifyChanges(std::vector<RsGxsNotify*>& changes);

        // Overloaded from RsTickEvent.
virtual void handle_event(uint32_t event_type, const std::string &elabel);

public:

virtual bool getGroupData(const uint32_t &token, std::vector<RsGxsChatGroup> &groups);
virtual bool getPostData(const uint32_t &token, std::vector<RsGxsChatMsg> &posts, std::vector<RsGxsComment> &cmts);
virtual bool getPostData(const uint32_t &token, std::vector<RsGxsChatMsg> &posts) {	std::vector<RsGxsComment> cmts; return getPostData( token, posts, cmts);}
//Not currently used
//virtual bool getRelatedPosts(const uint32_t &token, std::vector<RsGxsChannelPost> &posts);

        //////////////////////////////////////////////////////////////////////////////

//virtual bool setMessageStatus(const std::string &msgId, const uint32_t status, const uint32_t statusMask);
//virtual bool setGroupSubscribeFlags(const std::string &groupId, uint32_t subscribeFlags, uint32_t subscribeMask);

//virtual bool groupRestoreKeys(const std::string &groupId);
    virtual bool groupShareKeys(
            const RsGxsGroupId &groupId, const std::set<RsPeerId>& peers);

virtual bool createGroup(uint32_t &token, RsGxsChatGroup &group);
virtual bool createPost(uint32_t &token, RsGxsChatMsg &post);

virtual bool updateGroup(uint32_t &token, RsGxsChatGroup &group);

virtual bool getOwnMember(GxsChatMember &member);

    /*!
     * \brief acceptNewMessage
     * 		Early checks if the message can be accepted. This is mainly used to check wether the group is for instance overloaded and the service wants
     * 		to put limitations to it.
     * 		Returns true unless derived in GXS services.
     *
     * \param grpMeta Group metadata to check
     * \return
     */
    virtual bool acceptNewMessage(const RsGxsMsgMetaData* grpMeta, uint32_t size );


    /// @see RsGxsChannels::turtleSearchRequest
    virtual bool turtleSearchRequest(const std::string& matchString,
            const std::function<void (const RsGxsGroupSummary&)>& multiCallback,
            rstime_t maxWait = 300 );

    /**
     * Receive results from turtle search @see RsGenExchange @see RsNxsObserver
     * @see p3turtle::handleSearchResult
     */
    void receiveDistantSearchResults( TurtleRequestId id,
                                      const RsGxsGroupId& grpId );
    /* Comment service - Provide RsGxsCommentService - redirect to p3GxsCommentService */
    virtual bool getCommentData(uint32_t token, std::vector<RsGxsComment> &msgs)
    { return mCommentService->getGxsCommentData(token, msgs); }

    virtual bool getRelatedComments( uint32_t token,  std::vector<RsGxsComment> &msgs )
    { return mCommentService->getGxsRelatedComments(token, msgs); }

virtual bool createComment(uint32_t &token, RsGxsComment &msg)
    {
        return mCommentService->createGxsComment(token, msg);
    }

virtual bool createVote(uint32_t &token, RsGxsVote &msg)
    {
        return mCommentService->createGxsVote(token, msg);
    }

virtual bool acknowledgeComment(uint32_t token, std::pair<RsGxsGroupId, RsGxsMessageId>& msgId)
    {
        return acknowledgeMsg(token, msgId);
    }


virtual bool acknowledgeVote(uint32_t token, std::pair<RsGxsGroupId, RsGxsMessageId>& msgId)
    {
        if (mCommentService->acknowledgeVote(token, msgId))
        {
            return true;
        }
        return acknowledgeMsg(token, msgId);
    }

    // Overloaded from RsGxsIface.
virtual bool subscribeToGroup(uint32_t &token, const RsGxsGroupId &groupId, bool subscribe);

    // Set Statuses.
virtual void setMessageProcessedStatus(uint32_t& token, const RsGxsGrpMsgIdPair& msgId, bool processed);
virtual void setMessageReadStatus(uint32_t& token, const RsGxsGrpMsgIdPair& msgId, bool read);

    // File Interface
    virtual bool ExtraFileHash(const std::string& path);
virtual bool ExtraFileRemove(const RsFileHash &hash);


    /// Implementation of @see RsGxsChannels::getChatsSummaries
    virtual bool getChatsSummaries(std::list<RsGroupMetaData>& channels);

    /// Implementation of @see RsGxsChannels::getChannelsInfo
    virtual bool getChatsInfo(
            const std::list<RsGxsGroupId>& chanIds,
            std::vector<RsGxsChatGroup>& channelsInfo );

    /// Implementation of @see RsGxsChannels::getChannelContent
    virtual bool getChatsContent(
            const std::list<RsGxsGroupId>& chanIds,
            std::vector<RsGxsChatMsg>& posts );

protected:
    // Overloaded from GxsTokenQueue for Request callbacks.
    virtual void handleResponse(uint32_t token, uint32_t req_type);


private:

static uint32_t chatsAuthenPolicy();

    // Handle Processing.
    void request_AllSubscribedGroups();
    void request_SpecificSubscribedGroups(const std::list<RsGxsGroupId> &groups);
    void load_SubscribedGroups(const uint32_t &token);

    void request_SpecificUnprocessedPosts(std::list<std::pair<RsGxsGroupId, RsGxsMessageId> > &ids);
    void load_SpecificUnprocessedPosts(const uint32_t &token);

    void request_GroupUnprocessedPosts(const std::list<RsGxsGroupId> &grouplist);
    void load_GroupUnprocessedPosts(const uint32_t &token);

    void handleUnprocessedPost(const RsGxsChatMsg &msg);

    // Local Cache of Subscribed Groups. and AutoDownload Flag.
    void updateSubscribedGroup(const RsGroupMetaData &group);
    void clearUnsubscribedGroup(const RsGxsGroupId &id);

    bool setAutoDownload(const RsGxsGroupId &groupId, bool enabled);
    bool autoDownloadEnabled(const RsGxsGroupId &groupId, bool &enabled);



    std::map<RsGxsGroupId, RsGroupMetaData> mSubscribedGroups;


// DUMMY DATA,
virtual bool generateDummyData();

std::string genRandomId();

void 	dummy_tick();

bool generatePost(uint32_t &token, const RsGxsGroupId &grpId);

bool generateGroup(uint32_t &token, std::string groupName);

    class ChatDummyRef
    {
        public:
        ChatDummyRef() { return; }
        ChatDummyRef(const RsGxsGroupId &grpId, const RsGxsMessageId &threadId, const RsGxsMessageId &msgId)
        :mGroupId(grpId), mThreadId(threadId), mMsgId(msgId) { return; }

        RsGxsGroupId mGroupId;
        RsGxsMessageId mThreadId;
        RsGxsMessageId mMsgId;
    };

    uint32_t mGenToken;
    bool mGenActive;
    int mGenCount;
    std::vector<ChatDummyRef> mGenRefs;
    RsGxsMessageId mGenThreadId;

    p3GxsCommentService *mCommentService;
    std::map<RsGxsGroupId,rstime_t> mKnownChats;

    /** Store search callbacks with timeout*/
    std::map<
        TurtleRequestId,
        std::pair<
            std::function<void (const RsGxsGroupSummary&)>,
            std::chrono::system_clock::time_point >
     > mSearchCallbacksMap;
    RsMutex mSearchCallbacksMapMutex;

    /// Cleanup mSearchCallbacksMap
    void cleanTimedOutSearches();
    p3ChatService *mChatSrv;
    RsSerialType *mSerialiser;
    RsMutex mChatMtx;

    GxsChatMember *ownChatId; //computing your RsPeerId and GxsId then chat service start. We keep unique pair <RsPeerId,GxsId>.
    void initChatId();

    typedef std::pair<RsGxsChatGroup::ChatType, std::list<GxsChatMember>> ChatInfo; //one2one-list, group-list, channel-list
    std::map<RsGxsGroupId,ChatInfo> grpMembers;  //conversationId, {chattype, memberlist}.
    void loadChatsMembers(RsGxsChatGroup &grp);
    std::vector<std::pair<RsNxsGrp*,bool>> groupBouncePending;
    std::vector<std::pair<RsNxsMsg*, bool>> messageBouncePending;
    std::vector<std::pair<RsGxsGroupId,RsPeerId>> shareKeyBouncePending;
    RsGeneralDataService* mDataStore;
    std::map<RsGxsGrpMsgIdPair, uint32_t> messageCache;
    std::map<RsNxsNotifyChat*, rstime_t> notifyMsgCache;
    std::map<uint32_t, rstime_t> already_notifyMsg;

};

#endif // P3GXSCHATS_H
