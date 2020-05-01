/*******************************************************************************
 * libretroshare/src/services: p3GxsChats.cc                                *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright 2012-2012 Robert Fernie <retroshare@lunamutt.com>                 *
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
#include "services/p3gxschats.h"
#include "rsitems/rsgxschatitems.h"
#include "util/radix64.h"
#include "util/rsmemory.h"

#include <retroshare/rsidentity.h>
#include <retroshare/rsfiles.h>


#include "retroshare/rsgxsflags.h"
#include "retroshare/rsfiles.h"

#include "rsserver/p3face.h"
#include "retroshare/rsnotify.h"

#include <stdio.h>

// For Dummy Msgs.
#include "util/rsrandom.h"
#include "util/rsstring.h"

#include "chat/rschatitems.h"
#include "retroshare/rspeers.h"
#include "rsitems/rsnxsitems.h"

#define GXSCHATS_DEBUG 1



RsGxsChats *rsGxsChats = NULL;

//cache delay
#define DELAY_BETWEEN_CONFIG_UPDATES   30

#define GXSCHAT_STOREPERIOD	(3600 * 24 * 30)  // 30 days store

#define	 GXSCHATS_SUBSCRIBED_META		11
#define  GXSCHATS_UNPROCESSED_SPECIFIC	12
#define  GXSCHATS_UNPROCESSED_GENERIC	13

#define CHAT_PROCESS	 		0x0011
#define CHAT_TESTEVENT_DUMMYDATA	0x0012
#define DUMMYDATA_PERIOD		60	// long enough for some RsIdentities to be generated.

#define CHAT_DOWNLOAD_PERIOD 	(3600 * 24 * 7)
#define CHAT_MAX_AUTO_DL		(8 * 1024 * 1024 * 1024ull)	// 8 GB. Just a security ;-)

/********************************************************************************/
/******************* Startup / Tick    ******************************************/
/********************************************************************************/

p3GxsChats::p3GxsChats(RsGeneralDataService *gds, RsNetworkExchangeService *nes, RsGixs* gixs):
    RsGenExchange( gds, nes, new RsGxsChatSerialiser(), RS_SERVICE_GXS_TYPE_CHATS, gixs, chatsAuthenPolicy() ),
    RsGxsChats(static_cast<RsGxsIface&>(*this)), GxsTokenQueue(this),
    mSearchCallbacksMapMutex("GXS chats search"),
    mChatSrv(NULL),
    mSerialiser(new RsGxsChatSerialiser()),
    mDataStore(gds),
    ownChatId(NULL),
    mChatMtx("GxsChat Mutex")
{
    // For Dummy Msgs.
    mGenActive = false;
    mCommentService = new p3GxsCommentService(this,  RS_SERVICE_GXS_TYPE_CHATS);

    RsTickEvent::schedule_in(CHAT_PROCESS, 0);

    // Test Data disabled in repo.
    //RsTickEvent::schedule_in(CHAT_TESTEVENT_DUMMYDATA, DUMMYDATA_PERIOD);
    mGenToken = 0;
    mGenCount = 0;

    mLastConfigUpdate=0;

    //initalized chatInfo
    initChatId();
}

p3GxsChats::~p3GxsChats(){
    std::cerr <<"p3GxsChats::~p3GxsChats()"<<std::endl;
   IndicateConfigChanged() ;   //sync before shutdown
   std::this_thread::sleep_for(std::chrono::milliseconds(1000*15)); //waiting 15s before shutdown.
}

const std::string GXS_CHATS_APP_NAME = "gxschats";
const uint16_t GXS_CHATS_APP_MAJOR_VERSION  =       1;
const uint16_t GXS_CHATS_APP_MINOR_VERSION  =       0;
const uint16_t GXS_CHATS_MIN_MAJOR_VERSION  =       1;
const uint16_t GXS_CHATS_MIN_MINOR_VERSION  =       0;

RsServiceInfo p3GxsChats::getServiceInfo()
{
        return RsServiceInfo(RS_SERVICE_GXS_TYPE_CHATS,
                GXS_CHATS_APP_NAME,
                GXS_CHATS_APP_MAJOR_VERSION,
                GXS_CHATS_APP_MINOR_VERSION,
                GXS_CHATS_MIN_MAJOR_VERSION,
                GXS_CHATS_MIN_MINOR_VERSION);
}

uint32_t p3GxsChats::chatsAuthenPolicy()
{
    uint32_t policy = 0;
    uint32_t flag = 0;

    flag =  GXS_SERV::MSG_AUTHEN_ROOT_AUTHOR_SIGN | GXS_SERV::MSG_AUTHEN_CHILD_AUTHOR_SIGN;
    RsGenExchange::setAuthenPolicyFlag(flag, policy, RsGenExchange::PUBLIC_GRP_BITS);

    flag = GXS_SERV::MSG_AUTHEN_ROOT_PUBLISH_SIGN | GXS_SERV::MSG_AUTHEN_CHILD_PUBLISH_SIGN;
    RsGenExchange::setAuthenPolicyFlag(flag, policy, RsGenExchange::RESTRICTED_GRP_BITS);

    return policy;
}

static const uint32_t GXS_CHATS_CONFIG_MAX_TIME_NOTIFY_STORAGE = 86400*30*2 ; // ignore notifications for 2 months
static const uint8_t  GXS_CHATS_CONFIG_SUBTYPE_NOTIFY_RECORD   = 0x02 ;

struct RsGxsChatNotifyRecordsItem: public RsItem
{

    RsGxsChatNotifyRecordsItem()
        : RsItem(RS_PKT_VERSION_SERVICE,RS_SERVICE_GXS_TYPE_CHATS_CONFIG,GXS_CHATS_CONFIG_SUBTYPE_NOTIFY_RECORD)
    {}

    virtual ~RsGxsChatNotifyRecordsItem() {}

    void serial_process( RsGenericSerializer::SerializeJob j,
                         RsGenericSerializer::SerializeContext& ctx )
    { RS_SERIAL_PROCESS(records); }

    void clear() {
        records.clear();
    }

    std::map<RsGxsGroupId,LocalGroupInfo> records;
};


class GxsChatsConfigSerializer : public RsServiceSerializer
{
public:
    GxsChatsConfigSerializer() : RsServiceSerializer(RS_SERVICE_GXS_TYPE_CHATS_CONFIG) {}
    virtual ~GxsChatsConfigSerializer() {}

    RsItem* create_item(uint16_t service_id, uint8_t item_sub_id) const
    {
        if(service_id != RS_SERVICE_GXS_TYPE_CHATS_CONFIG)
            return NULL;

        switch(item_sub_id)
        {
        case GXS_CHATS_CONFIG_SUBTYPE_NOTIFY_RECORD: return new RsGxsChatNotifyRecordsItem();
        default:
            return NULL;
        }
    }
};

bool p3GxsChats::saveList(bool &cleanup, std::list<RsItem *>&saveList)
{
#ifdef GXSCHATS_DEBUG
    std::cerr <<" p3GxsChats::saveList()"<<std::endl;
#endif

    cleanup = true ;

    RsGxsChatNotifyRecordsItem *item = new RsGxsChatNotifyRecordsItem ;

    item->records = mKnownChats ;

    saveList.push_back(item) ;

    return true;
}

bool p3GxsChats::loadList(std::list<RsItem *>& loadList)
{
#ifdef GXSCHATS_DEBUG
    std::cerr <<" p3GxsChats::loadList()"<<std::endl;
#endif

    while(!loadList.empty())
    {
        RsItem *item = loadList.front();
        loadList.pop_front();

        rstime_t now = time(NULL);

        RsGxsChatNotifyRecordsItem *fnr = dynamic_cast<RsGxsChatNotifyRecordsItem*>(item) ;

        if(fnr != NULL)
        {
            RS_STACK_MUTEX(mChatMtx);
            mKnownChats.clear();


            for(auto it(fnr->records.begin());it!=fnr->records.end();++it){
                LocalGroupInfo localInfo = it->second;
                rstime_t lasttime = localInfo.update_ts;
                if( lasttime + GXS_CHATS_CONFIG_MAX_TIME_NOTIFY_STORAGE > now){
                    mKnownChats.insert(std::make_pair(it->first, it->second)) ;
                }

            }

        }

        delete item ;
    }
    return true;
}
void p3GxsChats::initChatId(){

    if(ownChatId == NULL && rsIdentity!=NULL && rsPeers !=NULL ){

        ownChatId = new GxsChatMember();
        //getting default RsPeerId

        RsPeerId peerid = rsPeers->getOwnId();
        std::string peername = rsPeers->getPeerName(peerid);
        std::string shareInviteUrl= rsPeers->GetRetroshareInvite();

        RsGxsId defaultId;
        //getting default RsGxsId by using the first id on the list.
        std::list<RsGxsId> own_ids ;
        rsIdentity->getOwnIds(own_ids) ;

        if(!own_ids.empty())
        {
            defaultId = own_ids.front() ;
        }

        ownChatId->nickname = peername;
        ownChatId->chatPeerId=peerid;
        ownChatId->chatGxsId=defaultId;
        ownChatId->chatinfo["shareInviteUrl"]=shareInviteUrl;

    }

}

RsSerialiser* p3GxsChats::setupSerialiser()
{
    RsSerialiser* rss = new RsSerialiser;
    rss->addSerialType(new GxsChatsConfigSerializer());

    return rss;
}

/** Overloaded to cache new groups **/
RsGenExchange::ServiceCreate_Return p3GxsChats::service_CreateGroup(RsGxsGrpItem* grpItem, RsTlvSecurityKeySet& keySet)
{
    updateSubscribedGroup(grpItem->meta);
    //send Group to the network
    RsGxsChatGroupItem *item = dynamic_cast<RsGxsChatGroupItem*>(grpItem);
    if (item){
        //groupId should exist by now.
        grpMembers[item->meta.mGroupId]= make_pair(item->type,item->members);

#ifdef GXSCHATS_DEBUG
        std::cerr <<"*****************p3GxsChats::service_CreateGroup(**********"<<std::endl;
        std::cerr <<"*****************GroupId:"<<item->meta.mGroupId<< "  and   group.type:"<<item->type<<std::endl;
        std::cerr <<"*****************Group mInternalCircleID: "<<item->meta.mInternalCircle <<std::endl;
        std::cerr <<"*****************GroupMembers: *************"<<std::endl;
        for(auto it=item->members.begin(); it !=item->members.end(); it++){
            std::cerr << "chatPeerId:"<< it->chatPeerId<< "Username: "<<it->nickname<< std::endl;
        }
#endif
    }
    else {
        std::cerr <<"Failed to cast object: dynamic_cast<RsGxsChatGroupItem*>(grpItem); "<<std::endl;
        return SERVICE_CREATE_FAIL;
    }


    return SERVICE_CREATE_SUCCESS;
}

RsGenExchange::ServiceCreate_Return p3GxsChats::service_UpateGroup(RsGxsGrpItem* grpItem, const RsTlvSecurityKeySet& keySet){
#ifdef GXSCHATS_DEBUG
    std::cerr <<"*****************p3GxsChats::service_UpateGroup()**********"<<std::endl;
#endif

    typedef std::map<RsGxsId, RsTlvPrivateRSAKey> keyMap;
    const keyMap& allKeys = keySet.private_keys;
    keyMap::const_iterator cit = allKeys.begin();

    bool adminFound = false, publishFound = false;
    for(; cit != allKeys.end(); ++cit)
    {
        const RsTlvPrivateRSAKey& key = cit->second;
        if(key.keyFlags & RSTLV_KEY_TYPE_FULL)		// this one is not useful. Just a security.
        {
            if(key.keyFlags & RSTLV_KEY_DISTRIB_ADMIN)
                adminFound = true;

            if(key.keyFlags & RSTLV_KEY_DISTRIB_PUBLISH)
                publishFound = true;

        }
        else if(key.keyFlags & RSTLV_KEY_TYPE_PUBLIC_ONLY)		// this one is not useful. Just a security.
        {
            std::cerr << "(EE) found a public only key in the private key list" << std::endl;
            return SERVICE_GXSCHATS_DEFAULT ;
        }
    }


    RsGxsChatGroupItem* groupItem = dynamic_cast<RsGxsChatGroupItem*>(grpItem);
    if(groupItem){
        RsGroupInfo groupInfo;
        if(rsPeers->getGroupInfo(RsNodeGroupId(groupItem->meta.mInternalCircle), groupInfo)){
            std::cerr <<"******p3GxsChats::service_UpateGroup(): Update GroupNodeId MemberLists()*******"<<std::endl;
            for(auto it2 = groupItem->members.begin(); it2 != groupItem->members.end(); ++it2)
            {
                std::cerr<<"Member: "<<it2->nickname <<" and RsPeerId: "<<it2->chatPeerId <<std::endl;
                RsPeerDetails detail;
                if (rsPeers->getPeerDetails((*it2).chatPeerId, detail))
                    rsPeers->assignPeerToGroup(groupInfo.id, detail.gpg_id, true);
            }

        }
    }


    // user must have both private and public parts of publish and admin keys
    if(adminFound || publishFound)  //owner | admin of the gxsgroup
        return SERVICE_GXSCHATS_UPDATE_SUCCESS;

    return SERVICE_GXSCHATS_DEFAULT ;

}

RsGenExchange::ServiceCreate_Return p3GxsChats::service_PublishGroup(RsNxsGrp *grp, bool update){
    if (ownChatId==NULL){ //initalized chatInfo
        initChatId();
    }

    std::map<RsGxsGroupId, RsGroupMetaData>::iterator it;
    it = mSubscribedGroups.find(grp->grpId);
    if (it == mSubscribedGroups.end() )
        return SERVICE_CREATE_FAIL;  //message doesn't belong to any group.


    auto mit = grpMembers.find(grp->grpId);
    if(mit == grpMembers.end())
        return SERVICE_CREATE_FAIL;  //message doesn't belong to any group.

    ChatInfo cinfo = mit->second;

    RsTlvBinaryData& data = grp->grp;
    RsItem* item = NULL;

    if(data.bin_len != 0)
        item = mSerialiser->deserialise(data.bin_data, &data.bin_len);

    if(item)
    {
        RsGxsGrpItem* gItem = dynamic_cast<RsGxsGrpItem*>(item);
        if (gItem)
        {
            gItem->meta = *(grp->metaData);
            RsGxsChatGroupItem* groupItem = dynamic_cast<RsGxsChatGroupItem*>(gItem);
            if(groupItem){
                RsGroupMetaData chatGrpMeta = groupItem->meta;
                if(chatGrpMeta.mCircleType !=GXS_CIRCLE_TYPE_PUBLIC){
                    RS_STACK_MUTEX(mChatMtx);
                    cinfo = std::make_pair(groupItem->type,groupItem->members);
                    grpMembers.insert(std::make_pair(grp->grpId,cinfo));
                }
            }
        }
    }

    std::list<RsPeerId> ids;
    rsPeers->getOnlineList(ids);
    std::set<RsPeerId> tempSendList;
    RsNetworkExchangeService *netService = RsGenExchange::getNetworkExchangeService();
    RsGroupMetaData grpMeta = it->second;

    if(grpMeta.mCircleType==GXS_CIRCLE_TYPE_PUBLIC && !ids.empty()){
            std::set<RsPeerId> peers(ids.begin(), ids.end());
            netService->PublishChatGroup(grp,ids);
            //if(cinfo.first !=RsGxsChatGroup::CHANNEL)
            //    groupShareKeys(grp->grpId,peers);
            //sharing publish key to all invite members.
    }else{
        //check groupNodeId
        RsGroupInfo groupInfo2;
        if(!update){
            if( !rsPeers->getGroupInfoByName(grpMeta.mGroupName, groupInfo2))
            {
                RsGroupInfo groupInfo;
                //groupInfo.id = RsNodeGroupId(it->second.mMeta.mGroupId);
                groupInfo.id = RsNodeGroupId(RsGxsCircleId(grp->grpId)); //.clear(); // RS will generate an ID
                groupInfo.name = grpMeta.mGroupId.toStdString();

                switch(cinfo.first){
                case RsGxsChatGroup::ONE2ONE:   groupInfo.type =RsGroupInfo::ONE2ONE;   break;
                case RsGxsChatGroup::GROUPCHAT: groupInfo.type =RsGroupInfo::GROUPCHAT; break;
                case RsGxsChatGroup::CHANNEL:   groupInfo.type =RsGroupInfo::CHANNEL;   break;
                }
                //create local groupnode and add this group to the groupnode list in the local here:
                if(rsPeers->addGroupWithId(groupInfo, true))
                {
                    grp->metaData->mInternalCircle = RsGxsCircleId(grp->grpId);
                }
                std::set<RsPgpId> pgpIds;
                //get all PgpId from the member list of this group
                for(std::set<GxsChatMember>::iterator it2 = cinfo.second.begin(); it2 != cinfo.second.end(); ++it2)
                {
                    RsPeerDetails detail;
                    if (rsPeers->getPeerDetails((*it2).chatPeerId, detail))
                    {
                        pgpIds.insert(detail.gpg_id);
                    }
                }
                std::set<RsPgpId>::iterator it;
                for (it = pgpIds.begin(); it != pgpIds.end(); ++it) {
                    rsPeers->assignPeerToGroup(groupInfo.id, *it, true);
                }
            }
            else{
                //already create nodegroupd, just need to upgrade groupId.
                if(rsPeers->removeGroup(groupInfo2.id)){
                    groupInfo2.id=RsNodeGroupId(RsGxsCircleId(grp->grpId));
                    groupInfo2.name = grpMeta.mGroupId.toStdString();
                    //create local groupnode and add this group to the groupnode list in the local here:
                    if(rsPeers->addGroupWithId(groupInfo2, true))
                    {
                        grp->metaData->mInternalCircle = RsGxsCircleId(grp->grpId);
                    }
                }

            }
        }else{
            RsGroupInfo groupInfo;
            RsNodeGroupId nodeId( RsGxsCircleId(grp->grpId));
            if(rsPeers->getGroupInfo(nodeId, groupInfo) ){
                groupInfo.peerIds.clear();
                rsPeers->addGroupWithId(groupInfo, true);

                for(std::set<GxsChatMember>::iterator it2 = cinfo.second.begin(); it2 != cinfo.second.end(); ++it2)
                {
                    RsPeerDetails detail;
                    if (rsPeers->getPeerDetails((*it2).chatPeerId, detail)){
                        rsPeers->assignPeerToGroup(groupInfo.id, detail.gpg_id, true);
                    }
                }

            }
        }
        //send off the network
        std::cerr <<"ServiceCreate_Return p3GxsChats::service_PublishGroup() "<<std::endl;
        for (auto sit=cinfo.second.begin(); sit !=cinfo.second.end(); sit++){
            if(sit->chatPeerId  != ownChatId->chatPeerId && rsPeers->isOnline(sit->chatPeerId)){
                tempSendList.insert(sit->chatPeerId); //member of the group and also online status.
                std::cerr<<"Sending to member: "<<sit->nickname <<std::endl;
            }
        }
        if(!tempSendList.empty()){
            std::list<RsPeerId> sendlist(tempSendList.begin(), tempSendList.end());
            netService->PublishChatGroup(grp,sendlist);
            //if(cinfo.first !=RsGxsChatGroup::CHANNEL)
            //    groupShareKeys(grp->grpId,tempSendList);          //sharing publish key to all invite members.
        }

    }

    return SERVICE_CREATE_SUCCESS;
}

RsGenExchange::ServiceCreate_Return p3GxsChats::service_CreateMessage(RsNxsMsg* msg){
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::service_CreateMessage()  MsgId: " << msg->msgId << " and GroupId: " << msg->grpId<< std::endl;
#endif
    if (ownChatId==NULL){
        //initalized chatInfo
        initChatId();
    }
    std::map<RsGxsGroupId, RsGroupMetaData>::iterator it;
    it = mSubscribedGroups.find(msg->grpId);

    if (it == mSubscribedGroups.end() )
        return SERVICE_CREATE_FAIL;  //message doesn't belong to any group.

    auto mit = grpMembers.find(msg->grpId);
    if(mit == grpMembers.end())
        return SERVICE_CREATE_FAIL;  //message doesn't belong to any group.

    RsGroupMetaData chatGrpMeta =it->second;
    RsNetworkExchangeService *netService = RsGenExchange::getNetworkExchangeService();


    std::list<RsPeerId> ids;
    rsPeers->getOnlineList(ids);

    switch(chatGrpMeta.mCircleType){
    case GXS_CIRCLE_TYPE_PUBLIC:
    {
        //get all online friends and send this message to them.
        netService->PublishChat(msg,ids);
        break;
    }
    case GXS_CIRCLE_TYPE_EXTERNAL:// restricted to an external circle, made of RsGxsId
    {
        std::cerr << "CircleType: GXS_CIRCLE_TYPE_EXTERNAL"<<std::endl;
        break;
    }   
    case GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY:	// restricted to a subset of friend nodes of a given RS node given by a RsPgpId list
    {
        std::list<RsPeerId> tempSendList;
        /*
        //getting all member of group node memberlist.
        if(!chatGrpMeta.mInternalCircle.isNull())
        {
            RsGroupInfo ginfo ;
            RsNodeGroupId  groupId = RsNodeGroupId(chatGrpMeta.mInternalCircle);

            if(rsPeers->getGroupInfo(groupId,ginfo))
            {
                for (auto it = ginfo.peerIds.begin(); it != ginfo.peerIds.end(); it++ ){ //list of PgPId
                    std::list<RsPeerId> nodeIds;
                    if (rsPeers->getAssociatedSSLIds(*it,nodeIds)){
                        for(auto lit=nodeIds.begin(); lit !=nodeIds.end(); lit++){
                            if (rsPeers->isOnline(*lit))
                                tempSendList.insert(*lit); //filter for only online status friend.
                        }
                    }
                }//end for loop
            }
        }
        */
        //getting all member on conversation memberlist.
        //privateGroup. sending to only membership, except the sender.
        std::cerr << "************p3GxsChats::service_CreateMessage()  MsgId: " << msg->msgId << " and GroupId: " << msg->grpId<< std::endl;
        ChatInfo cinfo = mit->second;
        for (auto sit=cinfo.second.begin(); sit !=cinfo.second.end(); sit++){
            if(sit->chatPeerId  != ownChatId->chatPeerId && rsPeers->isOnline(sit->chatPeerId)){
                tempSendList.push_back(sit->chatPeerId); //member of the group and also online status.
                std::cerr <<"*********Group's Member: "<<sit->nickname << " and RsPeerId: "<<sit->chatPeerId <<std::endl;
            }
        }


        if(!tempSendList.empty()){
            //std::list<RsPeerId> sendlist(tempSendList.begin(),tempSendList.end()); //convert Set to List type. Using Set to eliminate any duplicate between both group.
            netService->PublishChat(msg,tempSendList);
        }

        break;
    }

    }//end switch
    return SERVICE_CREATE_SUCCESS;
}


RsGenExchange::ServiceCreate_Return p3GxsChats::service_RecvBounceGroup(RsNxsGrp *grp, bool isNew){
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::service_RecvBounceGroup()  GroupId: " << grp->grpId << " and FromPeerId: " << grp->PeerId()<< std::endl;
#endif
    RS_STACK_MUTEX(mChatMtx) ;

    RsNxsGrp *newGrp = new RsNxsGrp(grp->PacketService());  //preventing delete of the pointer from GenExchange Service.
    //RsNxsGrp *newGrp = grp->clone();
    newGrp->PeerId(grp->PeerId());
    newGrp->grpId = grp->grpId;
    newGrp->meta.setBinData(grp->meta.bin_data, grp->meta.bin_len);
    newGrp->grp.setBinData(grp->grp.bin_data, grp->grp.bin_len);
    newGrp->metaData = grp->metaData;

    groupBouncePending.push_back(std::make_pair(newGrp, isNew));

    ChatInfo cinfo;
    auto mit = grpMembers.find(grp->grpId);
    if(mit != grpMembers.end()){
        cinfo = mit->second;
    }

    RsTlvBinaryData& data = grp->grp;
    RsItem* item = NULL;
    if(data.bin_len != 0)
        item = mSerialiser->deserialise(data.bin_data, &data.bin_len);

    if(item)
    {
        RsGxsGrpItem* gItem = dynamic_cast<RsGxsGrpItem*>(item);
        if (gItem)
        {
            gItem->meta = *(grp->metaData);
            RsGxsChatGroupItem* groupItem = dynamic_cast<RsGxsChatGroupItem*>(gItem);
            if(groupItem){
                RsGroupMetaData chatGrpMeta = groupItem->meta;
                if(chatGrpMeta.mCircleType==GXS_CIRCLE_TYPE_PUBLIC)
                    return SERVICE_CREATE_SUCCESS;

                cinfo = std::make_pair(groupItem->type,groupItem->members);
                grpMembers.insert(std::make_pair(grp->grpId,cinfo));
            }
        }
    }

   if(isNew){
        //check groupNodeId
        RsGroupInfo groupInfo2;
        grp->metaData->mInternalCircle = RsGxsCircleId(grp->grpId);

        if( !rsPeers->getGroupInfoByName(grp->grpId.toStdString(), groupInfo2))
        {
            RsGroupInfo groupInfo;
            //groupInfo.id = RsNodeGroupId(it->second.mMeta.mGroupId);
            groupInfo.id = RsNodeGroupId(RsGxsCircleId(grp->grpId)); //.clear(); // RS will generate an ID
            groupInfo.name = grp->grpId.toStdString();

            switch(cinfo.first){
            case RsGxsChatGroup::ONE2ONE:   groupInfo.type =RsGroupInfo::ONE2ONE;   break;
            case RsGxsChatGroup::GROUPCHAT: groupInfo.type =RsGroupInfo::GROUPCHAT; break;
            case RsGxsChatGroup::CHANNEL:   groupInfo.type =RsGroupInfo::CHANNEL;   break;
            }
            //create local groupnode and add this group to the groupnode list in the local here:
            if(rsPeers->addGroupWithId(groupInfo, true))
            {
                grp->metaData->mInternalCircle = RsGxsCircleId(grp->grpId);
            }
            std::set<RsPgpId> pgpIds;
            //get all PgpId from the member list of this group
            for(std::set<GxsChatMember>::iterator it2 = cinfo.second.begin(); it2 != cinfo.second.end(); ++it2)
            {
                RsPeerDetails detail;
                if (rsPeers->getPeerDetails((*it2).chatPeerId, detail))
                {
                    pgpIds.insert(detail.gpg_id);
                }
            }
            std::set<RsPgpId>::iterator it;
            for (it = pgpIds.begin(); it != pgpIds.end(); ++it) {
                rsPeers->assignPeerToGroup(groupInfo.id, *it, true);
            }
        }
        else{
            //already create nodegroupd, just need to upgrade groupId.
            if(rsPeers->removeGroup(groupInfo2.id)){
                groupInfo2.id=RsNodeGroupId(RsGxsCircleId(grp->grpId));
                //create local groupnode and add this group to the groupnode list in the local here:
                if(rsPeers->addGroupWithId(groupInfo2, true))
                {
                    grp->metaData->mInternalCircle = RsGxsCircleId(grp->grpId);
                }
            }

        }
   }else{//check if more members has added.
       RsGroupInfo groupInfo;
       RsNodeGroupId nodeId(RsGxsCircleId(grp->grpId));
       if(rsPeers->getGroupInfo(nodeId, groupInfo) ){
           groupInfo.peerIds.clear();
           rsPeers->addGroupWithId(groupInfo, true);

           for(std::set<GxsChatMember>::iterator it2 = cinfo.second.begin(); it2 != cinfo.second.end(); ++it2)
           {
               RsPeerDetails detail;
               if (rsPeers->getPeerDetails((*it2).chatPeerId, detail)){
                   rsPeers->assignPeerToGroup(groupInfo.id, detail.gpg_id, true);
               }
           }

       }

   }


    return SERVICE_CREATE_SUCCESS;
}

RsGenExchange::ServiceCreate_Return p3GxsChats::service_RecvBounceMessage(RsNxsMsg* msg, bool isNew){
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::service_RecvBounceMessage()  MsgId: " << msg->msgId << " and GroupId: " << msg->grpId<< "FromPeerId: " <<msg->PeerId()<< std::endl;
#endif
    RS_STACK_MUTEX(mChatMtx) ;

    RsNxsMsg *newMsg = new RsNxsMsg(msg->PacketService());
    newMsg->grpId = msg->grpId;
    newMsg->msgId = msg->msgId;
    newMsg->meta.setBinData(msg->meta.bin_data, msg->meta.bin_len);
    newMsg->msg.setBinData(msg->msg.bin_data, msg->msg.bin_len);
    newMsg->PeerId(msg->PeerId());
    newMsg->metaData = msg->metaData;


    messageBouncePending.push_back(std::make_pair(newMsg, isNew));
    return SERVICE_CREATE_SUCCESS;
}

bool p3GxsChats::getOwnMember(GxsChatMember &member){
    if (ownChatId==NULL)
        initChatId();

    member.chatGxsId = ownChatId->chatGxsId;
    member.chatPeerId=ownChatId->chatPeerId;
    member.chatinfo = ownChatId->chatinfo;
    member.nickname = ownChatId->nickname;

    return true;
}

void p3GxsChats::notifyReceivePublishKey(const RsGxsGroupId &grpId, const RsPeerId &peerid){
    //RS_STACK_MUTEX(mChatMtx) ;
    RsGenExchange::notifyReceivePublishKey(grpId,peerid);

#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::notifyReceivePublishKey() : groupId"<<grpId << " and PeerId: "<< peerid<< std::endl;
#endif

    shareKeyBouncePending.push_back(std::make_pair(grpId,peerid));
    //bouncing publish key..in futue, change public group policy that no need to share publish key.

}
void p3GxsChats::handleBounceShareKey(){

    RS_STACK_MUTEX(mChatMtx) ;

    if (shareKeyBouncePending.empty())
        return; //no new share key.

#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::handleBounceShareKey() "<< std::endl;
#endif

    for(auto it = shareKeyBouncePending.begin(); it !=shareKeyBouncePending.end(); it++){
        RsGxsGroupId grpId = it->first;
        RsPeerId sender = it->second;

        auto mit = grpMembers.find(grpId);
        if (mit == grpMembers.end()) //does nothing if group is not there to share publish permission.
            continue;

        std::map<RsGxsGroupId, RsGroupMetaData>::iterator sit;
        sit = mSubscribedGroups.find(grpId);

        if(sit == mSubscribedGroups.end())
            continue; //group is not yet subscribe...no share the key.

        RsGroupMetaData grpMeta = sit->second;
        if(grpMeta.mCircleType !=GXS_CIRCLE_TYPE_PUBLIC)
            continue; //skip reshare if the group is private.

        ChatInfo cinfo = mit->second;
        std::set<GxsChatMember> friendList = cinfo.second;
        std::set<RsPeerId> tempSendList;
        std::list<RsPeerId> ids;
        //ids.remove(sender);
        rsPeers->getOnlineList(ids);
        for(auto id=ids.begin(); id !=ids.end(); id++){
            if(*id == sender || *id == ownChatId->chatPeerId){
                //don't add to the sendlist.
                continue;
            }else{
                std::cerr <<"Adding PeerId:"<<*(id)<<std::endl;
                tempSendList.insert(*id);
            }
        }

        switch(cinfo.first){
            case RsGxsChatGroup::CHANNEL:
            case RsGxsChatGroup::ONE2ONE: break;  //drop, no bouncing off
            case RsGxsChatGroup::GROUPCHAT:
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::notifyReceivePublishKey() : GROUPCHAT  : "<< cinfo.first<< std::endl;
#endif
        if(!tempSendList.empty()){
#ifdef GXSCHATS_DEBUG
             for(auto it=tempSendList.begin(); it !=tempSendList.end(); it++)
                 std::cerr <<"Send to PeerId:"<<*it <<std::endl;
#endif
            groupShareKeys(grpId,tempSendList);
         }

       }//end of switch statement

    }//END FOR LOOP

    shareKeyBouncePending.clear();
}

void p3GxsChats::processRecvBounceGroup(){

    RS_STACK_MUTEX(mChatMtx) ;

    if(groupBouncePending.empty())
        return;

    if(ownChatId==NULL)
        initChatId();
    //if Group is ONE2ONE, Drop the bouncing message.
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::processRecvBounceGroup()  : " << std::endl;
#endif
    RsNetworkExchangeService *netService = RsGenExchange::getNetworkExchangeService();

    //std::vector<RsNxsGrp*>::iterator it;
    for (auto it=groupBouncePending.begin(); it !=groupBouncePending.end(); it++){
        RsNxsGrp *bounceGrp = it->first;
        bool isNew = it->second;
        RsGxsGroupId grpId = bounceGrp->grpId;
        //check to see if the group is already exist on the conversationlist.

        RsTlvBinaryData& data = bounceGrp->grp;
        RsItem* item = NULL;
        if(data.bin_len != 0)
            item = mSerialiser->deserialise(data.bin_data, &data.bin_len);

        if(!item){
            continue;
        }
        RsGxsGrpItem* gItem = dynamic_cast<RsGxsGrpItem*>(item);
        RsGxsChatGroupItem* grpItem = dynamic_cast<RsGxsChatGroupItem*>(gItem);

        auto mit = grpMembers.find(grpId);
        if (mit == grpMembers.end() && isNew){
            //new group just received. We have to deserialized to find out what type of conversation (ONE2ONE, GROUP, or CHANNEL)
            std::list<RsPeerId> ids;
            rsPeers->getOnlineList(ids);
            RsPeerId sender = bounceGrp->PeerId();
            ids.remove(sender);
            switch(grpItem->type){
                case RsGxsChatGroup::ONE2ONE:   break;  //drop, no bouncing off
                case RsGxsChatGroup::GROUPCHAT:
                case RsGxsChatGroup::CHANNEL:                
                if(grpItem->meta.mCircleType==GXS_CIRCLE_TYPE_PUBLIC ){
                    netService->PublishChatGroup(bounceGrp, ids);
                    //this is a questionable if we want to sending to all other friend-of-friends on this public groupchat.
                }else{
                    //first time receive private group, it should not re-broadcast to other friends.
                }

            }//end switch
        }
        else if(mit != grpMembers.end() && !isNew){  //exist group and being update.

            std::map<RsGxsGroupId, RsGroupMetaData>::iterator git;
            git = mSubscribedGroups.find(bounceGrp->grpId);
            if(git !=mSubscribedGroups.end() ){
                continue;
            }
            //exist on the list. Look up the conversation type. probably an Group Update. Send only to memberlist.
            ChatInfo cinfo = mit->second;
            RsGxsGroupId grpId = bounceGrp->grpId;

            switch(cinfo.first){
                case RsGxsChatGroup::ONE2ONE:   break;  //drop, no bouncing off
                case RsGxsChatGroup::GROUPCHAT:
                case RsGxsChatGroup::CHANNEL:
                {
                   if(!isNew){
                        RsPeerId sender = bounceGrp->PeerId();
                        //std::list<GxsChatMember> sendList;
                        std::list<RsPeerId> tempSendList;

                        for (auto sit=cinfo.second.begin(); sit !=cinfo.second.end(); sit++){
                            if(sit->chatPeerId == sender || sit->chatPeerId ==  ownChatId->chatPeerId){
                                //Don't send to itself and sender peer.
                                continue;
                            }
                            else{
                                //sendList.push_back(*sit);
                                //need to check if the member is owner friend and online
                                //if member is not yet a friend, need to establish gxs tunnel if possible.
                                //if not yet member and able to establish gxs tunnel, auto add friend (non-trust mode)
                                //trust node = physical connection, non-trust node = gxs tunnel friend (People)
                                tempSendList.push_back(sit->chatPeerId); //temp without gxs tunneling.
                            }
                        }

                        //forward to all members on this group.
                        if(!tempSendList.empty())
                            netService->PublishChatGroup(bounceGrp,tempSendList);

                    }
                }
            }

        }

    }//end for loop
    groupBouncePending.clear();

}
void p3GxsChats::processRecvBounceMessage(){
    RS_STACK_MUTEX(mChatMtx) ;

    if(messageBouncePending.empty())
        return;

#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::processRecvBounceMessage()  : " << std::endl;
#endif
    if(ownChatId==NULL)
        initChatId();

    RsNetworkExchangeService *netService = RsGenExchange::getNetworkExchangeService();
    std::vector<std::pair<RsNxsMsg*,bool>>::iterator it;

    for (it=messageBouncePending.begin(); it !=messageBouncePending.end(); it++){
        RsNxsMsg *msg = it->first;
        bool isNew = it->second;
        RsGxsGroupId grpId = msg->grpId;

        auto mit = grpMembers.find(grpId);
        if (mit == grpMembers.end()){
             continue; //message doesn't belong to anygroup... Drop it!
        }

        auto sit = mSubscribedGroups.find(grpId);
        if(sit == mSubscribedGroups.end())
             continue; //not a subscribe conversation, no bouncing on this message.

        ChatInfo cinfo = mit->second;

         switch(cinfo.first){
            case RsGxsChatGroup::ONE2ONE: //one2one message, drop off, final destination! no bouncing off!
             break;
            case RsGxsChatGroup::GROUPCHAT:
            case RsGxsChatGroup::CHANNEL:
            {
                RsPeerId sender = msg->PeerId();
                std::list<RsPeerId> tempSendList;
                std::list<RsPeerId> ids;
                rsPeers->getOnlineList(ids);

                RsGroupMetaData grpMeta = sit->second;
                if(grpMeta.mCircleType ==GXS_CIRCLE_TYPE_PUBLIC){
                    ids.remove(sender);
                    tempSendList.merge(ids);
                }else{
                    //bouncing all private members with message on the group, execept the sender of this message.
                    std::set<GxsChatMember> friendList = cinfo.second;
                    GxsChatMember senderCheck;
                    senderCheck.chatPeerId = sender;
                    if(friendList.find(senderCheck) == friendList.end())
                        continue; //skip this message as it's not on memberlist.

                    for (auto sit=friendList.begin(); sit !=friendList.end(); sit++){
                        if(sit->chatPeerId == sender || sit->chatPeerId ==  ownChatId->chatPeerId)
                              continue;  //Don't send to itself and sender peer.
                        else{
                            if(rsPeers->isOnline(sit->chatPeerId)){
                                tempSendList.push_back(sit->chatPeerId); //temp without gxs tunneling.
                            }
                        }
                    }//end forloop
                }
                if (!tempSendList.empty())   //publish the message with group of peerIds.
                   netService->PublishChat(msg, tempSendList);
            }
         }//end switch
    }//end for loop
    messageBouncePending.clear();
}

void p3GxsChats::processRecvBounceNotify(){
   if(notifyMsgCache.empty())
        return;


    RS_STACK_MUTEX(mChatMtx) ;
    p3Notify *notify = RsServer::notify();

    for(auto it = notifyMsgCache.begin(); it !=notifyMsgCache.end(); ){
        RsNxsNotifyChat *msgnotify = it->first;
        rstime_t ts = it->second;
        RsGxsPersonPair personIdPair = msgnotify->sendFrom.first;
        std::string personName = msgnotify->sendFrom.second;
        //username|RsPeerId|GxsId
        std::string sender = personName + "|" + personIdPair.first.toStdString()  + "|" + personIdPair.second.toStdString();
        //command type:argument, chatStatus:typing... audio_call:ICEINFO, ...
        std::string command = msgnotify->command.first + ":" + msgnotify->command.second;

#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::processRecvBounceNotify() Nofify ->Sender:"<< sender << " command:"<<command<< std::endl;
#endif
        //unseenp2p - meiyousixin - add Notify from notifyQt
        //emit NotifyQ
        RsServer::notify()->receiveGxsChatTyping(msgnotify->grpId, personName, personIdPair.first, personIdPair.second );
        //send notification to application and GUI Layer.
        notify->AddFeedItem(RS_FEED_ITEM_CHATS_NOTIFY, msgnotify->grpId.toStdString(), sender,command);
        //bounce this message if it's groupchat public/private only.
        //channel and one2one will not bounce notification messages.
        publishBounceNotifyMessage(msgnotify);
        //delete notification cache.
        it = notifyMsgCache.erase(it);
    }
    notifyMsgCache.clear();
}

void p3GxsChats::publishNotifyMessage(const RsGxsGroupId &grpId,std::pair<std::string,std::string> &command){
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::publishNotifyMessage()  : " << std::endl;
#endif
    auto sit = mSubscribedGroups.find(grpId);
    if(sit == mSubscribedGroups.end())
        return; //group is not yet subscribe...no share the key

    auto mit = grpMembers.find(grpId);
    if (mit == grpMembers.end())
        return;

    if(ownChatId==NULL)
        initChatId();

    RsNxsNotifyChat *notifyMsg = new RsNxsNotifyChat(RS_SERVICE_GXS_TYPE_CHATS);
    RsGxsPersonPair personIdPair=std::make_pair(ownChatId->chatPeerId,ownChatId->chatGxsId);
    notifyMsg->sendFrom=std::make_pair(personIdPair,ownChatId->nickname);
    notifyMsg->grpId = grpId;
    notifyMsg->msgId = RSRandom::random_u32(); //randomID
    notifyMsg->command = command;

    {
        RS_STACK_MUTEX(mChatMtx) ;
        already_notifyMsg.insert(std::make_pair(notifyMsg->msgId, time(NULL)));
        //prevent bounce back messages.
    }

    ChatInfo cinfo = mit->second;
     //one2one or channel notification, drop off, final destination!
    switch(cinfo.first){
        case RsGxsChatGroup::CHANNEL:    //drop, no bouncing off
        case RsGxsChatGroup::ONE2ONE:
        case RsGxsChatGroup::GROUPCHAT:

        RsNetworkExchangeService *netService = RsGenExchange::getNetworkExchangeService();
        RsGroupMetaData grpMeta = sit->second;

        std::list<RsPeerId> tempSendList;
        std::list<RsPeerId> ids;
        rsPeers->getOnlineList(ids);

        if(grpMeta.mCircleType ==GXS_CIRCLE_TYPE_PUBLIC){
            //publicgroup with all user onlines. all online friends + groupMembership added & except the sender.
            tempSendList.merge(ids);
        }else{
            //privateGroup. sending to only membership, except the sender.
            for (auto sit=cinfo.second.begin(); sit !=cinfo.second.end(); sit++){
                if(sit->chatPeerId  != ownChatId->chatPeerId && rsPeers->isOnline(sit->chatPeerId))
                    tempSendList.push_back(sit->chatPeerId); //temp without gxs tunneling.
            }
        }
        if(!tempSendList.empty())
            netService->PublishChatNotify(notifyMsg, tempSendList);

     }//send switch


}
void p3GxsChats::publishBounceNotifyMessage(RsNxsNotifyChat * notifyMsg){
    if(notifyMsg ==NULL)
        return;

#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::publishBounceNotifyMessage()  : " << std::endl;
#endif
    if(ownChatId==NULL)
        initChatId();

    auto mit = grpMembers.find(notifyMsg->grpId);
    if (mit == grpMembers.end()) return;

    ChatInfo cinfo = mit->second;

#ifdef GXSCHATS_DEBUG
    RsGxsPersonPair personIdPair = notifyMsg->sendFrom.first;
    std::string personName = notifyMsg->sendFrom.second;
    std::cerr <<"GroupId: "<<notifyMsg->grpId << " and MsgId: "<<notifyMsg->msgId<<std::endl;
    std::cerr <<"Sender: "<< personName << " and pair(sslid:"<<personIdPair.first<<", gxsid:"<< personIdPair.second<<") "<<std::endl;
#endif

     //one2one or channel notification, drop off, final destination!
    switch(cinfo.first){
        case RsGxsChatGroup::ONE2ONE:  break;
        case RsGxsChatGroup::CHANNEL:  break;  //drop, no bouncing off
        case RsGxsChatGroup::GROUPCHAT:{
        //std::map<RsGxsGroupId, RsGroupMetaData>::iterator sit;
        auto sit = mSubscribedGroups.find(notifyMsg->grpId);
        if(sit == mSubscribedGroups.end())
            return; //group is not yet subscribe...no share the key.

        RsNetworkExchangeService *netService = RsGenExchange::getNetworkExchangeService();
        RsGroupMetaData grpMeta = sit->second;
        std::list<RsPeerId> tempSendList;
        std::list<RsPeerId> ids;
        rsPeers->getOnlineList(ids);
        RsPeerId sender = notifyMsg->PeerId();

        if(grpMeta.mCircleType ==GXS_CIRCLE_TYPE_PUBLIC){
            //publicgroup with all user onlines. all online friends + groupMembership added & except the sender.
            ids.remove(sender);
            tempSendList.merge(ids);
                //or notifyMsg.
        }else{
            //privateGroup. sending to only membership, except the sender.
            for (auto sit=cinfo.second.begin(); sit !=cinfo.second.end(); sit++){
                if( sit->chatPeerId != sender ){
                    if ( sit->chatPeerId  != ownChatId->chatPeerId && rsPeers->isOnline(sit->chatPeerId))
                        tempSendList.push_back(sit->chatPeerId);
                }
            }
        }
        //send to all member of this group
        if(!tempSendList.empty())
            netService->PublishChatNotify(notifyMsg, tempSendList);

       }//end groupchat case
    }//end switch
}

void p3GxsChats::processRecvBounceNotifyClear(){
    if(already_notifyMsg.empty())
        return;

#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::processRecvBounceNotifyClear()  : " << std::endl;
#endif
    RS_STACK_MUTEX(mChatMtx) ;
    rstime_t now = time(NULL);
    for(auto it = already_notifyMsg.begin(); it !=already_notifyMsg.end(); ){
        //remove all already_exist messages that is 5min old, need to expire it. Otherwise, it's growing too big!
        if(now > it->second + 300){
            it =already_notifyMsg.erase(it);
        }
        else{
            it++;
        }
    }


}
void p3GxsChats::notifyChanges(std::vector<RsGxsNotify *> &changes)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::notifyChanges() : " << changes.size() << " changes to notify" << std::endl;
#endif

    p3Notify *notify = NULL;
    if (!changes.empty())
    {
        notify = RsServer::notify();
    }

    /* iterate through and grab any new messages */
    std::list<RsGxsGroupId> unprocessedGroups;

    std::vector<RsGxsNotify *>::iterator it;
    for(it = changes.begin(); it != changes.end(); ++it)
    {
        RsGxsMsgChange *msgChange = dynamic_cast<RsGxsMsgChange *>(*it);
        if (msgChange)
        {
            if (msgChange->getType() == RsGxsNotify::TYPE_RECEIVED_NEW)
            {
                /* message received */
                if (notify)
                {

#ifdef GXSCHATS_DEBUG
                    std::cerr <<"p3GxsChats::notifyChanges(): Received New Messages: "<<std::endl;
#endif
                    std::map<RsGxsGroupId, std::set<RsGxsMessageId> > &msgChangeMap = msgChange->msgChangeMap;
                    for (auto mit = msgChangeMap.begin(); mit != msgChangeMap.end(); ++mit)
                        for (auto mit1 = mit->second.begin(); mit1 != mit->second.end(); ++mit1)
                        {
                            notify->AddFeedItem(RS_FEED_ITEM_CHATS_MSG, mit->first.toStdString(), mit1->toStdString());
                            std::cerr<<"GroupId: "<<mit->first.toStdString() <<" and MsgId: "<<mit1->toStdString() <<std::endl;
                        }
                }
            }

            if (!msgChange->metaChange())
            {
#ifdef GXSCHATS_DEBUG
                std::cerr << "p3GxsChats::notifyChanges() Found Message Change Notification";
                std::cerr << std::endl;
#endif

                std::map<RsGxsGroupId, std::set<RsGxsMessageId> > &msgChangeMap = msgChange->msgChangeMap;
                for(auto mit = msgChangeMap.begin(); mit != msgChangeMap.end(); ++mit)
                {
#ifdef GXSCHATS_DEBUG
                    std::cerr << "p3GxsChats::notifyChanges() Msgs for Group: " << mit->first<<std::endl;
#endif
                                bool enabled = false ;

                }
            }
        }
        else
        {
            if (notify)
            {
                RsGxsGroupChange *grpChange = dynamic_cast<RsGxsGroupChange*>(*it);
                if (grpChange)
                {
                    switch (grpChange->getType())
                    {
                    default:
                        case RsGxsNotify::TYPE_PROCESSED:
                        case RsGxsNotify::TYPE_PUBLISHED:
                            break;

                        case RsGxsNotify::TYPE_RECEIVED_NEW:
                        {
                            /* group received */
                            std::list<RsGxsGroupId> &grpList = grpChange->mGrpIdList;
                            std::list<RsGxsGroupId>::iterator git;
                            for (git = grpList.begin(); git != grpList.end(); ++git)
                            {
                                if(mKnownChats.find(*git) == mKnownChats.end())
                                {
                                    notify->AddFeedItem(RS_FEED_ITEM_CHATS_NEW, git->toStdString());
                                    bool subscribe=true;

                                    {
                                        RS_STACK_MUTEX(mChatMtx);
                                        LocalGroupInfo localGrp;
                                        localGrp.msg="New";
                                        localGrp.isSubscribed=subscribe;
                                        mKnownChats.insert(std::make_pair(*git,localGrp)) ;

                                    }
                                    //auto subscribe chat conversation when it's first received.
                                    uint32_t token;
                                    subscribeToGroup(token, *git, subscribe);
                                    RsServer::notify()->NotifyCreateNewGroup(*git);
                                }
                                else
                                    std::cerr << "(II) Not notifying already known chat " << *git << std::endl;
                            }
                            break;
                        }

                        case RsGxsNotify::TYPE_RECEIVED_PUBLISHKEY:
                        {
                            /* group received */
                            std::list<RsGxsGroupId> &grpList = grpChange->mGrpIdList;
                            std::list<RsGxsGroupId>::iterator git;
                            for (git = grpList.begin(); git != grpList.end(); ++git)
                            {
                                notify->AddFeedItem(RS_FEED_ITEM_CHATS_PUBLISHKEY, git->toStdString());
                            }
                            break;
                        }
                    }
                }
            }
        }

        /* shouldn't need to worry about groups - as they need to be subscribed to */
    }

    request_SpecificSubscribedGroups(unprocessedGroups);

    RsGxsIfaceHelper::receiveChanges(changes);
}

void	p3GxsChats::service_tick()
{

static  rstime_t last_dummy_tick = 0;
static  rstime_t last_notifyClear = 0;

    if (time(NULL) > last_dummy_tick + 5)
    {
        dummy_tick();
        last_dummy_tick = time(NULL);

    }

    processRecvBounceGroup();
    processRecvBounceMessage();
    //handleBounceShareKey();
    processRecvBounceNotify();

    RsTickEvent::tick_events();
    GxsTokenQueue::checkRequests();

    if( time(NULL)  > last_notifyClear + 60){
        processRecvBounceNotifyClear(); //clear every 60 seconds.
        last_notifyClear = time(NULL);
    }
}

bool p3GxsChats::getGroupData(const uint32_t &token, std::vector<RsGxsChatGroup> &groups)
{
    std::vector<RsGxsGrpItem*> grpData;
    bool ok = RsGenExchange::getGroupData(token, grpData);

    if(ok)
    {

        std::vector<RsGxsGrpItem*>::iterator vit = grpData.begin();

        for(; vit != grpData.end(); ++vit)
        {
            RsGxsChatGroupItem* item = dynamic_cast<RsGxsChatGroupItem*>(*vit);
            if (item)
            {
                RsGxsChatGroup grp;
                item->toChatGroup(grp, true);

                auto found = mKnownChats.find(RsGxsGroupId(item->meta.mGroupId));
                if( found != mKnownChats.end()){
                    grp.localMsgInfo = mKnownChats[RsGxsGroupId(item->meta.mGroupId)];
                }
                else{
                    RS_STACK_MUTEX(mChatMtx);
                    LocalGroupInfo localMsg;
                    localMsg.msg ="New";
                    localMsg.update_ts = time(NULL);
                    localMsg.isSubscribed = true;
                    grp.localMsgInfo = localMsg;
                    mKnownChats[RsGxsGroupId(item->meta.mGroupId)] =  localMsg;
                }
                groups.push_back(grp);
                //loadChatsMembers(grp);
                {
                    RS_STACK_MUTEX(mChatMtx);
                    grpMembers[grp.mMeta.mGroupId]=std::make_pair(grp.type,grp.members);
                }

                delete item;
            }
            else
            {
                std::cerr << "p3GxsChats::getGroupData() ERROR in decode";
                std::cerr << std::endl;
                delete(*vit);
            }
        }
    }
    else
    {
        std::cerr << "p3GxsChats::getGroupData() ERROR in request";
        std::cerr << std::endl;
    }

    return ok;
}
void p3GxsChats::loadChatsMembers(RsGxsChatGroup &grp){

    RS_STACK_MUTEX(mChatMtx);
    RsGxsGroupId grpId = grp.mMeta.mGroupId;
    auto it = grpMembers.find(grpId);
    if(it == grpMembers.end()){
        //new group, no need to check...add all members.
        grpMembers[grpId]=std::make_pair(grp.type,grp.members);
        return;
    }

    for(auto nIt=grp.members.begin(); nIt != grp.members.end(); nIt++){
        grpMembers[grpId].second.insert(*nIt);
    }

}

bool p3GxsChats::acceptNewMessage(const RsNxsMsg* msg ,uint32_t size)
{

    RsGxsMsgMetaData* grpMeta = msg->metaData;
    RsGxsGroupId grpId = grpMeta->mGroupId;
    RsGxsMessageId msgId = grpMeta->mMsgId;

    std::map<RsGxsGroupId, RsGroupMetaData>::iterator grpit;
    grpit = mSubscribedGroups.find(grpId);
    if(grpit == mSubscribedGroups.end() ){
           return false; //drop this message if it's not belong to any subscribed group
     }

    RsGroupMetaData chatGrpMeta = grpit->second;

    auto mit = grpMembers.find(grpId);
    if (mit == grpMembers.end()){
         return false; //message doesn't belong to anygroup... Drop it!
    }
    else{
        ChatInfo cinfo = mit->second;
        if(chatGrpMeta.mCircleType !=GXS_CIRCLE_TYPE_PUBLIC){
            GxsChatMember senderCheck;
            senderCheck.chatPeerId = msg->PeerId();
            if(cinfo.second.find(senderCheck) == cinfo.second.end())
                return false; //drop this message as it's not belong on memberlist.
        }

    }
    //look up message with GroupId and MsgId
    RsGxsGrpMsgIdPair grpMsgPair=std::make_pair(grpId,msgId);

    auto found = messageCache.find(grpMsgPair);
    if(found !=messageCache.end() && found->second == size){
        //size is equal to existence message, then it's false.
        std::cerr << "Message is already exist! msgId: "<<msgId << " and GroupId: "<<grpId<<std::endl;
        return false;
    }else{
        RS_STACK_MUTEX(mChatMtx);
        messageCache.insert(std::make_pair(grpMsgPair, size));
    }
    return true;
}

bool p3GxsChats::groupShareKeys(
        const RsGxsGroupId &groupId, const std::set<RsPeerId>& peers )
{
    RsGenExchange::shareGroupPublishKey(groupId,peers);
    return true;
}

bool p3GxsChats::getPostData(const uint32_t &token, std::vector<RsGxsChatMsg> &msgs, std::vector<RsGxsComment> &cmts)
{

    GxsMsgDataMap msgData;
    bool ok = RsGenExchange::getMsgData(token, msgData);

    std::set<RsGxsChatMsg> results; //try to put timestamp order

    if(ok)
    {
        GxsMsgDataMap::iterator mit = msgData.begin();

        for(; mit != msgData.end();  ++mit)
        {
            std::vector<RsGxsMsgItem*>& msgItems = mit->second;
            std::vector<RsGxsMsgItem*>::iterator vit = msgItems.begin();

            for(; vit != msgItems.end(); ++vit)
            {
                RsGxsChatMsgItem* postItem = dynamic_cast<RsGxsChatMsgItem*>(*vit);

                if(postItem)
                {
                    RsGxsChatMsg msg;
                    postItem->toChatPost(msg, true);
                    results.insert(msg);
                    RsGxsGrpMsgIdPair GrpMsgPair = std::make_pair(postItem->meta.mGroupId, postItem->meta.mMsgId);
                    uint32_t size = mSerialiser->size(*vit) ;
                    {
                        RS_STACK_MUTEX(mChatMtx);
                        //messageCache[pair] =msg.mSize;
                        messageCache.insert(std::make_pair(GrpMsgPair, size));

                    }
                    delete postItem;
                }
                else
                {
                    RsGxsCommentItem* cmtItem = dynamic_cast<RsGxsCommentItem*>(*vit);
                    if(cmtItem)
                    {
                        RsGxsComment cmt;
                        RsGxsMsgItem *mi = (*vit);
                        cmt = cmtItem->mMsg;
                        cmt.mMeta = mi->meta;
#ifdef GXSCOMMENT_DEBUG
                        std::cerr << "p3GxsChats::getPostData Found Comment:" << std::endl;
                        cmt.print(std::cerr,"  ", "cmt");
#endif
                        cmts.push_back(cmt);
                        delete cmtItem;
                    }
                    else
                    {
                        RsGxsMsgItem* msg = (*vit);
                        //const uint16_t RS_SERVICE_GXS_TYPE_CHATS    = 0x0231;
                        //const uint8_t  RS_PKT_SUBTYPE_GXSCHATS_POST_ITEM = 0x03;
                        //const uint8_t  RS_PKT_SUBTYPE_GXSCOMMENT_COMMENT_ITEM = 0xf1;
                        std::cerr << "Not a GxschatPostItem neither a RsGxsCommentItem"
                                            << " PacketService=" << std::hex << (int)msg->PacketService() << std::dec
                                            << " PacketSubType=" << std::hex << (int)msg->PacketSubType() << std::dec
                                            << " , deleting!" << std::endl;
                        delete *vit;
                    }
                }
            }
        }
    }
    else
    {
        std::cerr << "p3GxsChats::getPostData() ERROR in request";
        std::cerr << std::endl;
    }

    for(auto nit= results.begin(); nit !=results.end(); nit++){
         msgs.push_back(*nit);
    }

    return ok;
}


void p3GxsChats::request_AllSubscribedGroups()
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::request_SubscribedGroups()";
    std::cerr << std::endl;
#endif // GXSCHATS_DEBUG

    uint32_t ansType = RS_TOKREQ_ANSTYPE_SUMMARY;
    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_GROUP_META;

    uint32_t token = 0;

    RsGenExchange::getTokenService()->requestGroupInfo(token, ansType, opts);
    GxsTokenQueue::queueRequest(token, GXSCHATS_SUBSCRIBED_META);

#define PERIODIC_ALL_PROCESS	300 // TESTING every 5 minutes.
    RsTickEvent::schedule_in(CHAT_PROCESS, PERIODIC_ALL_PROCESS);
}


void p3GxsChats::request_SpecificSubscribedGroups(const std::list<RsGxsGroupId> &groups)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::request_SpecificSubscribedGroups()";
    std::cerr << std::endl;
#endif // GXSCHATS_DEBUG

    uint32_t ansType = RS_TOKREQ_ANSTYPE_SUMMARY;
    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_GROUP_META;

    uint32_t token = 0;

    RsGenExchange::getTokenService()->requestGroupInfo(token, ansType, opts, groups);
    GxsTokenQueue::queueRequest(token, GXSCHATS_SUBSCRIBED_META);
}


void p3GxsChats::load_SubscribedGroups(const uint32_t &token)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::load_SubscribedGroups()";
    std::cerr << std::endl;
#endif // GXSCHATS_DEBUG

    std::list<RsGroupMetaData> groups;
    std::list<RsGxsGroupId> groupList;

    getGroupMeta(token, groups);

    std::list<RsGroupMetaData>::iterator it;
    for(it = groups.begin(); it != groups.end(); ++it)
    {
        if (it->mSubscribeFlags &
            (GXS_SERV::GROUP_SUBSCRIBE_ADMIN |
            GXS_SERV::GROUP_SUBSCRIBE_PUBLISH |
            GXS_SERV::GROUP_SUBSCRIBE_SUBSCRIBED ))
        {
#ifdef GXSCHATS_DEBUG
            std::cerr << "p3GxsChats::load_SubscribedGroups() updating Subscribed Group: " << it->mGroupId;
            std::cerr << std::endl;
#endif

            updateSubscribedGroup(*it);
            bool enabled = false ;

        }
        else
        {
#ifdef GXSCHATS_DEBUG
            std::cerr << "p3GxsChats::load_SubscribedGroups() clearing unsubscribed Group: " << it->mGroupId;
            std::cerr << std::endl;
#endif
            clearUnsubscribedGroup(it->mGroupId);
        }
    }

    /* Query for UNPROCESSED POSTS from checkGroupList */
    request_GroupUnprocessedPosts(groupList);
}



void p3GxsChats::updateSubscribedGroup(const RsGroupMetaData &group)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::updateSubscribedGroup() id: " << group.mGroupId;
    std::cerr << std::endl;
#endif

    mSubscribedGroups[group.mGroupId] = group;
}


void p3GxsChats::clearUnsubscribedGroup(const RsGxsGroupId &id)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::clearUnsubscribedGroup() id: " << id;
    std::cerr << std::endl;
#endif

    //std::map<RsGxsGroupId, RsGrpMetaData> mSubscribedGroups;
    std::map<RsGxsGroupId, RsGroupMetaData>::iterator it;

    it = mSubscribedGroups.find(id);
    if (it != mSubscribedGroups.end())
    {
        mSubscribedGroups.erase(it);
    }
}


bool p3GxsChats::subscribeToGroup(uint32_t &token, const RsGxsGroupId &groupId, bool subscribe)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::subscribedToGroup() id: " << groupId << " subscribe: " << subscribe;
    std::cerr << std::endl;
#endif

    std::list<RsGxsGroupId> groups;
    groups.push_back(groupId);

    // Call down to do the real work.
    bool response = RsGenExchange::subscribeToGroup(token, groupId, subscribe);

    // reload Group afterwards.
    request_SpecificSubscribedGroups(groups);

    return response;
}


void p3GxsChats::request_SpecificUnprocessedPosts(std::list<std::pair<RsGxsGroupId, RsGxsMessageId> > &ids)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::request_SpecificUnprocessedPosts()";
    std::cerr << std::endl;
#endif // GXSCHATS_DEBUG

    uint32_t ansType = RS_TOKREQ_ANSTYPE_DATA;
    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_MSG_DATA;

    // Only Fetch UNPROCESSED messages.
    opts.mStatusFilter = GXS_SERV::GXS_MSG_STATUS_UNPROCESSED;
    opts.mStatusMask = GXS_SERV::GXS_MSG_STATUS_UNPROCESSED;

    uint32_t token = 0;

    /* organise Ids how they want them */
    GxsMsgReq msgIds;
    std::list<std::pair<RsGxsGroupId, RsGxsMessageId> >::iterator it;
    for(it = ids.begin(); it != ids.end(); ++it)
        msgIds[it->first].insert(it->second);

    RsGenExchange::getTokenService()->requestMsgInfo(token, ansType, opts, msgIds);
    GxsTokenQueue::queueRequest(token, GXSCHATS_UNPROCESSED_SPECIFIC);
}


void p3GxsChats::request_GroupUnprocessedPosts(const std::list<RsGxsGroupId> &grouplist)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::request_GroupUnprocessedPosts()";
    std::cerr << std::endl;
#endif // GXSCHATS_DEBUG

    uint32_t ansType = RS_TOKREQ_ANSTYPE_DATA;
    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_MSG_DATA;

    // Only Fetch UNPROCESSED messages.
    opts.mStatusFilter = GXS_SERV::GXS_MSG_STATUS_UNPROCESSED;
    opts.mStatusMask = GXS_SERV::GXS_MSG_STATUS_UNPROCESSED;

    uint32_t token = 0;

    RsGenExchange::getTokenService()->requestMsgInfo(token, ansType, opts, grouplist);
    GxsTokenQueue::queueRequest(token, GXSCHATS_UNPROCESSED_GENERIC);
}


void p3GxsChats::load_SpecificUnprocessedPosts(const uint32_t &token)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::load_SpecificUnprocessedPosts";
    std::cerr << std::endl;
#endif

    std::vector<RsGxsChatMsg> posts;
    if (!getPostData(token, posts))
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::load_SpecificUnprocessedPosts ERROR";
        std::cerr << std::endl;
#endif
        return;
    }

#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::load_SpecificUnprocessedPosts Download"<< std::endl;
        std::cerr <<"Post Size="<<posts.size()<<std::endl;
#endif

    std::vector<RsGxsChatMsg>::iterator it;
    for(it = posts.begin(); it != posts.end(); ++it)
    {
        /* autodownload the files */
#ifdef GXSCHATS_DEBUG
        std::cerr << "*********Post ID = "<<it->mMeta.mMsgId<<"  *******"<<std::endl;
#endif
        handleUnprocessedPost(*it);
    }
}


void p3GxsChats::load_GroupUnprocessedPosts(const uint32_t &token)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::load_GroupUnprocessedPosts";
    std::cerr << std::endl;
#endif

    std::vector<RsGxsChatMsg> posts;
    if (!getPostData(token, posts))
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::load_GroupUnprocessedPosts ERROR";
        std::cerr << std::endl;
#endif
        return;
    }


    std::vector<RsGxsChatMsg>::iterator it;
    for(it = posts.begin(); it != posts.end(); ++it)
    {
        handleUnprocessedPost(*it);
    }
}

void p3GxsChats::handleUnprocessedPost(const RsGxsChatMsg &msg)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::handleUnprocessedPost() GroupId: " << msg.mMeta.mGroupId << " MsgId: " << msg.mMeta.mMsgId;
    std::cerr << std::endl;
#endif

    if (!IS_MSG_UNPROCESSED(msg.mMeta.mMsgStatus))
    {
        std::cerr << "p3GxsChats::handleUnprocessedPost() Msg already Processed";
        std::cerr << std::endl;
        std::cerr << "p3GxsChats::handleUnprocessedPost() ERROR - this should not happen";
        std::cerr << std::endl;
        return;
    }

    bool enabled = false ;

    /* check that autodownload is set */
    if (autoDownloadEnabled(msg.mMeta.mGroupId,enabled) && enabled )
    {


#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::handleUnprocessedPost() AutoDownload Enabled ... handling";
        std::cerr << std::endl;
#endif

        /* check the date is not too old */
        rstime_t age = time(NULL) - msg.mMeta.mPublishTs;

        if (age < (rstime_t) CHAT_DOWNLOAD_PERIOD )
    {
        /* start download */
        // NOTE WE DON'T HANDLE PRIVATE CHANNELS HERE.
        // MORE THOUGHT HAS TO GO INTO THAT STUFF.

#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::handleUnprocessedPost() START DOWNLOAD";
        std::cerr << std::endl;
#endif

        std::list<RsGxsFile>::const_iterator fit;
        for(fit = msg.mFiles.begin(); fit != msg.mFiles.end(); ++fit)
        {
            std::string fname = fit->mName;
            Sha1CheckSum hash  = Sha1CheckSum(fit->mHash);
            uint64_t size     = fit->mSize;

            std::list<RsPeerId> srcIds;
            std::string localpath = "";
            TransferRequestFlags flags = RS_FILE_REQ_BACKGROUND | RS_FILE_REQ_ANONYMOUS_ROUTING;

            if (size < CHAT_MAX_AUTO_DL)
            {
                std::string directory ;
                if(getChannelDownloadDirectory(msg.mMeta.mGroupId,directory))
                    localpath = directory ;

                rsFiles->FileRequest(fname, hash, size, localpath, flags, srcIds);
            }
            else
                std::cerr << "WARNING: Chat file is not auto-downloaded because its size exceeds the threshold of " << CHAT_MAX_AUTO_DL << " bytes." << std::endl;
        }
    }

        /* mark as processed */
        uint32_t token;
        RsGxsGrpMsgIdPair msgId(msg.mMeta.mGroupId, msg.mMeta.mMsgId);
        setMessageProcessedStatus(token, msgId, true);
    }
#ifdef GXSCHATS_DEBUG
    else
    {
        std::cerr << "p3GxsChats::handleUnprocessedPost() AutoDownload Disabled ... skipping";
        std::cerr << std::endl;
    }
#endif
}


    // Overloaded from GxsTokenQueue for Request callbacks.
void p3GxsChats::handleResponse(uint32_t token, uint32_t req_type)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::handleResponse(" << token << "," << req_type << ")";
    std::cerr << std::endl;
#endif // GXSCHATS_DEBUG

    // stuff.
    switch(req_type)
    {
        case GXSCHATS_SUBSCRIBED_META:
            load_SubscribedGroups(token);
            break;

        case GXSCHATS_UNPROCESSED_SPECIFIC:
            load_SpecificUnprocessedPosts(token);
            break;

        case GXSCHATS_UNPROCESSED_GENERIC:
            load_SpecificUnprocessedPosts(token);
            break;

        default:
            /* error */
            std::cerr << "p3GxsService::handleResponse() Unknown Request Type: " << req_type;
            std::cerr << std::endl;
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
/// Blocking API implementation begin
////////////////////////////////////////////////////////////////////////////////

bool p3GxsChats::getChatsSummaries(
        std::list<RsGroupMetaData>& chats )
{
    uint32_t token;
    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_GROUP_META;
    if( !requestGroupInfo(token, opts)
            || waitToken(token) != RsTokenService::COMPLETE ) return false;
    return getGroupSummary(token, chats);
}

bool p3GxsChats::getChatsInfo(
        const std::list<RsGxsGroupId>& chanIds,
        std::vector<RsGxsChatGroup>& chatsInfo )
{
    uint32_t token;
    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_GROUP_DATA;
    if( !requestGroupInfo(token, opts, chanIds)
            || waitToken(token) != RsTokenService::COMPLETE ) return false;
    return getGroupData(token, chatsInfo);
}

bool p3GxsChats::getChatsContent(
        const std::list<RsGxsGroupId>& chanIds,
        std::vector<RsGxsChatMsg>& posts , int page)
{
    uint32_t token;
    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_MSG_DATA;
    opts.page = page;

    if( !requestMsgInfo(token, opts, chanIds)
            || waitToken(token) != RsTokenService::COMPLETE ) return false;
    return getPostData(token, posts);
}

////////////////////////////////////////////////////////////////////////////////
/// Blocking API implementation end
////////////////////////////////////////////////////////////////////////////////


void p3GxsChats::setMessageProcessedStatus(uint32_t& token, const RsGxsGrpMsgIdPair& msgId, bool processed)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::setMessageProcessedStatus()";
    std::cerr << std::endl;
#endif

    uint32_t mask = GXS_SERV::GXS_MSG_STATUS_UNPROCESSED;
    uint32_t status = GXS_SERV::GXS_MSG_STATUS_UNPROCESSED;
    if (processed)
    {
        status = 0;
    }
    setMsgStatusFlags(token, msgId, status, mask);
}

void p3GxsChats::setMessageReadStatus( uint32_t& token,
                                          const RsGxsGrpMsgIdPair& msgId, const std::string shortMsg,
                                          bool read )
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::setMessageReadStatus()";
    std::cerr << std::endl;
#endif

    /* Always remove status unprocessed */
    uint32_t mask = GXS_SERV::GXS_MSG_STATUS_GUI_NEW | GXS_SERV::GXS_MSG_STATUS_GUI_UNREAD;
    uint32_t status = GXS_SERV::GXS_MSG_STATUS_GUI_UNREAD;
    {
        RS_STACK_MUTEX(mChatMtx);
        auto found = mKnownChats.find(msgId.first);
        if (read) {
            status = 0;
            if(found !=mKnownChats.end()){
                for(auto it=mKnownChats[msgId.first].unreadMsgIds.begin(); it!=mKnownChats[msgId.first].unreadMsgIds.end(); it++){
                    RsGxsGrpMsgIdPair msgPair = std::make_pair(msgId.first,*it);
                    setMsgStatusFlags(token, msgPair, status, mask);
                }
                mKnownChats[msgId.first].clear();  //clear all the unread messageId
                mKnownChats[msgId.first].msg = shortMsg;

                slowIndicateConfigChanged();             
            }
        }else if(found !=mKnownChats.end()){
                mKnownChats[msgId.first].unreadMsgIds.insert(msgId.second);
                mKnownChats[msgId.first].msg = shortMsg;
                mKnownChats[msgId.first].update_ts = time(NULL);

                slowIndicateConfigChanged();

        }else{
            std::cerr <<"Not found groupPair"<<std::endl;
            std::cerr<<"PairGroup(): "<<msgId.first<< " and msgId: "<<msgId.second <<std::endl;
            std::cerr<<"Msg : "<<shortMsg <<std::endl;
        }
    }
    setMsgStatusFlags(token, msgId, status, mask);


}

void p3GxsChats::setLocalMessageStatus(const RsGxsGrpMsgIdPair& msgId, const std::string msg){
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::setLocalMessageStatus()";
    std::cerr << std::endl;
#endif
    RS_STACK_MUTEX(mChatMtx);
    auto found = mKnownChats.find(msgId.first);
    if(found !=mKnownChats.end()){
        mKnownChats[msgId.first].unreadMsgIds.insert(msgId.second);
        mKnownChats[msgId.first].msg = msg;
        mKnownChats[msgId.first].update_ts = time(NULL);

        slowIndicateConfigChanged();
    }

}

bool  p3GxsChats::getLocalMessageStatus(const RsGxsGroupId& groupId, LocalGroupInfo &localInfo){

    auto found = mKnownChats.find(groupId);
    if(found ==mKnownChats.end())
        return false;

    localInfo = mKnownChats[groupId];
    return true;
}

void p3GxsChats::slowIndicateConfigChanged()
{
    rstime_t now = time(NULL) ;

    //if(mLastConfigUpdate + DELAY_BETWEEN_CONFIG_UPDATES < now)
    //{
        IndicateConfigChanged() ;
        mLastConfigUpdate = now ;
    //}
}

/********************************************************************************************/
/********************************************************************************************/

bool p3GxsChats::createGroup(uint32_t &token, RsGxsChatGroup &group)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::createGroup()" << std::endl;
#endif

    RsGxsChatGroupItem* grpItem = new RsGxsChatGroupItem();
    grpItem->fromChatGroup(group, true);

    if(grpItem->members.empty() && ownChatId !=NULL ){
        GxsChatMember myself =*ownChatId;
        grpItem->type = group.type; // RsGxsChatGroup::GROUPCHAT;
        grpItem->members.insert(myself);
    }

    RsGenExchange::publishGroup(token, grpItem);
    return true;
}


bool p3GxsChats::updateGroup(uint32_t &token, RsGxsChatGroup &group)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::updateGroup()" << std::endl;
#endif

    RsGxsChatGroupItem* grpItem = new RsGxsChatGroupItem();
    grpItem->fromChatGroup(group, true);

    RsGenExchange::updateGroup(token, grpItem);

#ifdef GXSCHATS_DEBUG
        std::cerr <<"*****************p3GxsChats::updateGroup()**********"<<std::endl;
        std::cerr <<"*****************GroupId:"<<group.mMeta.mGroupId<< "  and   group.type:"<<group.type<<std::endl;
        std::cerr <<"*****************GroupMembers: *************"<<std::endl;
        for(auto it=group.members.begin(); it !=group.members.end(); it++){
            std::cerr << "chatPeerId:"<< it->chatPeerId<< "Username: "<<it->nickname<< std::endl;
        }
#endif
    return true;
}

void p3GxsChats::receiveNewChatMesesage(std::vector<GxsNxsChatMsgItem*>& messages){
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::receiveNewChatMesesage() testing Only!" << std::endl;
#endif

    std::vector<RsNxsMsg *> nxtMsg;
    uint32_t size =0;
    RsServiceInfo  gxsChatInfo = this->getServiceInfo();

    std::vector<GxsNxsChatMsgItem*>::iterator it;
  {
    RS_STACK_MUTEX(mChatMtx);
    for (it=messages.begin(); it != messages.end(); it++){
        GxsNxsChatMsgItem *msg = *it;

        size = mSerialiser->size(msg);
        char* mData = new char[size];
        bool serialOk = mSerialiser->serialise(msg, mData, &size);

        if (serialOk){ //converting RsChatItem to GxsMessage
            RsNxsMsg *newMsg = new RsNxsMsg(RS_PKT_SUBTYPE_GXSCHAT_MSG);
            newMsg->PeerId(msg->PeerId());

            newMsg->grpId = msg->grpId;
            newMsg->msgId = msg->msgId;
            newMsg->msg.setBinData(msg->msg.bin_data, msg->msg.bin_len);
            newMsg->meta.setBinData(msg->meta.bin_data, msg->meta.bin_len);
            nxtMsg.push_back(newMsg);
            //testing publish the new message anyway.
        }
        //delete mData;
    }
  }//end mutex
  RsGenExchange::receiveNewMessages(nxtMsg);  //send new message to validate


}
void p3GxsChats::receiveNewChatGroup(std::vector<GxsNxsChatGroupItem*>& groups){
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::fsreceiveNewChatGroup()" << std::endl;
#endif
}

void p3GxsChats::receiveNotifyMessages(std::vector<RsNxsNotifyChat*>& notifyMessages){
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::receiveNotifyMessages()" << std::endl;
#endif
    RS_STACK_MUTEX(mChatMtx);
    for(auto notify:notifyMessages){
        auto grpIt = mSubscribedGroups.find(notify->grpId);
        if( ( grpIt !=mSubscribedGroups.end()) && (already_notifyMsg.find(notify->msgId) == already_notifyMsg.end()) ){
            RsGroupMetaData chatGrpMeta = grpIt->second;
            auto mit = grpMembers.find(notify->grpId);
            if (mit == grpMembers.end()){
                return ; //message doesn't belong to anygroup... Drop it!
            }
            else{
                ChatInfo cinfo = mit->second;
                if(chatGrpMeta.mCircleType !=GXS_CIRCLE_TYPE_PUBLIC){
                    GxsChatMember senderCheck;
                    senderCheck.chatPeerId = notify->PeerId();
                    if(cinfo.second.find(senderCheck) == cinfo.second.end())
                        return ; //drop this message as it's not belong on memberlist.
                }
            }
            rstime_t now = time(NULL);
            notifyMsgCache.insert(std::make_pair(notify,now));
            already_notifyMsg.insert(std::make_pair(notify->msgId, now ));

        }else{
            std::cerr <<"Notify messageId:"<<notify->msgId <<" is already exists!"<<std::endl;
        }
        //drop the exist to prevent bouncing back messages.
    }
}

bool p3GxsChats::createPost(uint32_t &token, RsGxsChatMsg &msg)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::createChatMsg() GroupId: " << msg.mMeta.mGroupId;
    std::cerr << std::endl;
#endif

    RsGxsChatMsgItem* msgItem = new RsGxsChatMsgItem();
    msgItem->fromChatPost(msg, true);

    //if we want push messages, it should be done before add GxsSync Messages (Poll synchronization).

    RsGenExchange::publishMsg(token, msgItem);
    return true;
}

/********************************************************************************************/
/********************************************************************************************/

bool p3GxsChats::ExtraFileHash(const std::string& path)
{
    TransferRequestFlags flags = RS_FILE_REQ_ANONYMOUS_ROUTING;
    return rsFiles->ExtraFileHash(path, GXSCHAT_STOREPERIOD, flags);
}


bool p3GxsChats::ExtraFileRemove(const RsFileHash &hash)
{
    //TransferRequestFlags tflags = RS_FILE_REQ_ANONYMOUS_ROUTING | RS_FILE_REQ_EXTRA;
    RsFileHash fh = RsFileHash(hash);
    return rsFiles->ExtraFileRemove(fh);
}


/********************************************************************************************/
/********************************************************************************************/

/* so we need the same tick idea as wiki for generating dummy chats
 */

#define 	MAX_GEN_GROUPS		20
#define 	MAX_GEN_POSTS		50

std::string p3GxsChats::genRandomId()
{
    std::string randomId;
    for(int i = 0; i < 20; i++)
    {
        randomId += (char) ('a' + (RSRandom::random_u32() % 26));
    }

    return randomId;
}

bool p3GxsChats::generateDummyData()
{
    mGenCount = 0;

    std::string groupName;
    rs_sprintf(groupName, "Testchat_%d", mGenCount);

#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::generateDummyData() Starting off with Group: " << groupName;
    std::cerr << std::endl;
#endif

    /* create a new group */
    generateGroup(mGenToken, groupName);

    mGenActive = true;

    return true;
}


void p3GxsChats::dummy_tick()
{
    /* check for a new callback */

    if (mGenActive)
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::dummyTick() Gen Active";
        std::cerr << std::endl;
#endif

        uint32_t status = RsGenExchange::getTokenService()->requestStatus(mGenToken);
        if (status != RsTokenService::COMPLETE)
        {
#ifdef GXSCHATS_DEBUG
            std::cerr << "p3GxsChats::dummy_tick() Status: " << status;
            std::cerr << std::endl;
#endif

            if (status == RsTokenService::FAILED)
            {
#ifdef GXSCHATS_DEBUG
                std::cerr << "p3GxsChats::dummy_tick() generateDummyMsgs() FAILED";
                std::cerr << std::endl;
#endif
                mGenActive = false;
            }
            return;
        }

        if (mGenCount < MAX_GEN_GROUPS)
        {
            /* get the group Id */
            RsGxsGroupId groupId;
            RsGxsMessageId emptyId;
            if (!acknowledgeTokenGrp(mGenToken, groupId))
            {
                std::cerr << " ERROR ";
                std::cerr << std::endl;
                mGenActive = false;
                return;
            }

#ifdef GXSCHATS_DEBUG
            std::cerr << "p3GxsChats::dummy_tick() Acknowledged GroupId: " << groupId;
            std::cerr << std::endl;
#endif

            ChatDummyRef ref(groupId, emptyId, emptyId);
            mGenRefs[mGenCount] = ref;
        }
        else if (mGenCount < MAX_GEN_POSTS)
        {
            /* get the msg Id, and generate next snapshot */
            RsGxsGrpMsgIdPair msgId;
            if (!acknowledgeTokenMsg(mGenToken, msgId))
            {
                std::cerr << " ERROR ";
                std::cerr << std::endl;
                mGenActive = false;
                return;
            }

#ifdef GXSCHATS_DEBUG
            std::cerr << "p3GxsChats::dummy_tick() Acknowledged Post <GroupId: " << msgId.first << ", MsgId: " << msgId.second << ">";
            std::cerr << std::endl;
#endif

            /* store results for later selection */

            ChatDummyRef ref(msgId.first, mGenThreadId, msgId.second);
            mGenRefs[mGenCount] = ref;
        }

        else
        {
#ifdef GXSCHATS_DEBUG
            std::cerr << "p3GxsChats::dummy_tick() Finished";
            std::cerr << std::endl;
#endif

            /* done */
            mGenActive = false;
            return;
        }

        mGenCount++;

        if (mGenCount < MAX_GEN_GROUPS)
        {
            std::string groupName;
            rs_sprintf(groupName, "Testchat_%d", mGenCount);

#ifdef GXSCHATS_DEBUG
            std::cerr << "p3GxsChats::dummy_tick() Generating Group: " << groupName;
            std::cerr << std::endl;
#endif

            /* create a new group */
            generateGroup(mGenToken, groupName);
        }
        else if (mGenCount < MAX_GEN_POSTS)
        {
            /* create a new post */
            uint32_t idx = (uint32_t) (MAX_GEN_GROUPS * RSRandom::random_f32());
            ChatDummyRef &ref = mGenRefs[idx];

            RsGxsGroupId grpId = ref.mGroupId;
            RsGxsMessageId parentId = ref.mMsgId;
            mGenThreadId = ref.mThreadId;
            if (mGenThreadId.isNull())
            {
                mGenThreadId = parentId;
            }

#ifdef GXSCHATS_DEBUG
            std::cerr << "p3GxsChats::dummy_tick() Generating Post ... ";
            std::cerr << " GroupId: " << grpId;
            std::cerr << " ThreadId: " << mGenThreadId;
            std::cerr << " ParentId: " << parentId;
            std::cerr << std::endl;
#endif

            generatePost(mGenToken, grpId);
        }
        else
        {
            /* create a new post */
            uint32_t idx = (uint32_t) ((MAX_GEN_POSTS + 5000) * RSRandom::random_f32());
            ChatDummyRef &ref = mGenRefs[idx + MAX_GEN_POSTS];

            RsGxsGroupId grpId = ref.mGroupId;
            RsGxsMessageId parentId = ref.mMsgId;
            mGenThreadId = ref.mThreadId;
            if (mGenThreadId.isNull())
            {
                mGenThreadId = parentId;
            }


        }

    }

    cleanTimedOutSearches();
}


bool p3GxsChats::generatePost(uint32_t &token, const RsGxsGroupId &grpId)
{
    RsGxsChatMsg msg;

    std::string rndId = genRandomId();

    rs_sprintf(msg.mMsg, "chat Msg: GroupId: %s, some randomness: %s",
        grpId.toStdString().c_str(), rndId.c_str());

    msg.mMeta.mMsgName = msg.mMsg;

    msg.mMeta.mGroupId = grpId;
    msg.mMeta.mThreadId.clear() ;
    msg.mMeta.mParentId.clear() ;

    msg.mMeta.mMsgStatus = GXS_SERV::GXS_MSG_STATUS_UNPROCESSED;

    createPost(token, msg);

    return true;
}



bool p3GxsChats::generateGroup(uint32_t &token, std::string groupName)
{
    /* generate a new chat */
    RsGxsChatGroup chat;
    chat.mMeta.mGroupName = groupName;

    createGroup(token, chat);

    return true;
}


    // Overloaded from RsTickEvent for Event callbacks.
void p3GxsChats::handle_event(uint32_t event_type, const std::string &elabel)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::handle_event(" << event_type << ")";
    std::cerr << std::endl;
#endif

    // stuff.
    switch(event_type)
    {
        case CHAT_TESTEVENT_DUMMYDATA:
            generateDummyData();
            break;

        case CHAT_PROCESS:
            request_AllSubscribedGroups();
            break;

        default:
            /* error */
            std::cerr << "p3GxsChats::handle_event() Unknown Event Type: " << event_type << " elabel:" << elabel;
            std::cerr << std::endl;
            break;
    }
}

TurtleRequestId p3GxsChats::turtleGroupRequest(const RsGxsGroupId& group_id)
{
    return netService()->turtleGroupRequest(group_id) ;
}
TurtleRequestId p3GxsChats::turtleSearchRequest(const std::string& match_string)
{
    return netService()->turtleSearchRequest(match_string) ;
}

bool p3GxsChats::clearDistantSearchResults(TurtleRequestId req)
{
    return netService()->clearDistantSearchResults(req);
}
bool p3GxsChats::retrieveDistantSearchResults(TurtleRequestId req,std::map<RsGxsGroupId,RsGxsGroupSummary>& results)
{
    return netService()->retrieveDistantSearchResults(req,results);
}

bool p3GxsChats::retrieveDistantGroup(const RsGxsGroupId& group_id,RsGxsChatGroup& distant_group)
{
    RsGxsGroupSummary gs;

    if(netService()->retrieveDistantGroupSummary(group_id,gs))
    {
        // This is a placeholder information by the time we receive the full group meta data.
        distant_group.mMeta.mGroupId         = gs.mGroupId ;
        distant_group.mMeta.mGroupName       = gs.mGroupName;
        distant_group.mMeta.mGroupFlags      = GXS_SERV::FLAG_PRIVACY_PUBLIC ;
        distant_group.mMeta.mSignFlags       = gs.mSignFlags;

        distant_group.mMeta.mPublishTs       = gs.mPublishTs;
        distant_group.mMeta.mAuthorId        = gs.mAuthorId;

        distant_group.mMeta.mCircleType      = GXS_CIRCLE_TYPE_PUBLIC ;// guessed, otherwise the group would not be search-able.

        // other stuff.
        distant_group.mMeta.mAuthenFlags     = 0;	// wild guess...

        distant_group.mMeta.mSubscribeFlags  = GXS_SERV::GROUP_SUBSCRIBE_NOT_SUBSCRIBED ;

        distant_group.mMeta.mPop             = gs.mPopularity; 			// Popularity = number of friend subscribers
        distant_group.mMeta.mVisibleMsgCount = gs.mNumberOfMessages; 	// Max messages reported by friends
        distant_group.mMeta.mLastPost        = gs.mLastMessageTs; 		// Timestamp for last message. Not used yet.

        return true ;
    }
    else
        return false ;
}

bool p3GxsChats::turtleSearchRequest(
        const std::string& matchString,
        const std::function<void (const RsGxsGroupSummary&)>& multiCallback,
        rstime_t maxWait )
{
    if(matchString.empty())
    {
        std::cerr << __PRETTY_FUNCTION__ << " match string can't be empty!"
                  << std::endl;
        return false;
    }

    TurtleRequestId sId = turtleSearchRequest(matchString);

    RS_STACK_MUTEX(mSearchCallbacksMapMutex);
    mSearchCallbacksMap.emplace(
                sId,
                std::make_pair(
                    multiCallback,
                    std::chrono::system_clock::now() +
                        std::chrono::seconds(maxWait) ) );

    return true;
}

void p3GxsChats::receiveDistantSearchResults(
        TurtleRequestId id, const RsGxsGroupId& grpId )
{
    std::cerr << __PRETTY_FUNCTION__ << "(" << id << ", " << grpId << ")"
              << std::endl;

    RsGenExchange::receiveDistantSearchResults(id, grpId);
    RsGxsGroupSummary gs;
    gs.mGroupId = grpId;
    netService()->retrieveDistantGroupSummary(grpId, gs);

    {
        RS_STACK_MUTEX(mSearchCallbacksMapMutex);
        auto cbpt = mSearchCallbacksMap.find(id);
        if(cbpt != mSearchCallbacksMap.end())
            cbpt->second.first(gs);
    } // end RS_STACK_MUTEX(mSearchCallbacksMapMutex);
}

void p3GxsChats::cleanTimedOutSearches()
{
    RS_STACK_MUTEX(mSearchCallbacksMapMutex);
    auto now = std::chrono::system_clock::now();
    for( auto cbpt = mSearchCallbacksMap.begin();
         cbpt != mSearchCallbacksMap.end(); )
        if(cbpt->second.second <= now)
        {
            clearDistantSearchResults(cbpt->first);
            cbpt = mSearchCallbacksMap.erase(cbpt);
        }
        else ++cbpt;
}

/**  Download Functions ***/
/********************************************************************************************/
/********************************************************************************************/

#if 0
bool p3GxsChats::setChannelAutoDownload(uint32_t &token, const RsGxsGroupId &groupId, bool autoDownload)
{
    std::cerr << "p3GxsChats::setChannelAutoDownload()";
    std::cerr << std::endl;

    // we don't actually use the token at this point....
    bool p3GxsChats::setAutoDownload(const RsGxsGroupId &groupId, bool enabled)



    return;
}
#endif

bool SSGxsChatGroup::load(const std::string &input)
{
    if(input.empty())
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "SSGxsChatGroup::load() asked to load a null string." << std::endl;
#endif
        return true ;
    }
    int download_val;
    mAutoDownload = false;
    mDownloadDirectory.clear();


    RsTemporaryMemory tmpmem(input.length());

    if (1 == sscanf(input.c_str(), "D:%d", &download_val))
    {
        if (download_val == 1)
            mAutoDownload = true;
    }
    else  if( 2 == sscanf(input.c_str(),"v2 {D:%d} {P:%[^}]}",&download_val,(unsigned char*)tmpmem))
    {
        if (download_val == 1)
            mAutoDownload = true;

        std::vector<uint8_t> vals = Radix64::decode(std::string((char*)(unsigned char *)tmpmem)) ;
        mDownloadDirectory = std::string((char*)vals.data(),vals.size());
    }
    else  if( 1 == sscanf(input.c_str(),"v2 {D:%d}",&download_val))
    {
        if (download_val == 1)
            mAutoDownload = true;
    }
    else
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "SSGxsChatGroup::load(): could not parse string \"" << input << "\"" << std::endl;
#endif
        return false ;
    }

#ifdef GXSCHATS_DEBUG
    std::cerr << "DECODED STRING: autoDL=" << mAutoDownload << ", directory=\"" << mDownloadDirectory << "\"" << std::endl;
#endif

    return true;
}

std::string SSGxsChatGroup::save() const
{
    std::string output = "v2 ";

    if (mAutoDownload)
        output += "{D:1}";
    else
        output += "{D:0}";

    if(!mDownloadDirectory.empty())
    {
        std::string encoded_str ;
        Radix64::encode((unsigned char*)mDownloadDirectory.c_str(),mDownloadDirectory.length(),encoded_str);

        output += " {P:" + encoded_str + "}";
    }

#ifdef GXSCHATS_DEBUG
    std::cerr << "ENCODED STRING: " << output << std::endl;
#endif

    return output;
}

bool p3GxsChats::autoDownloadEnabled(const RsGxsGroupId &groupId,bool& enabled)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::autoDownloadEnabled(" << groupId << ")";
    std::cerr << std::endl;
#endif

    std::map<RsGxsGroupId, RsGroupMetaData>::iterator it;

    it = mSubscribedGroups.find(groupId);
    if (it == mSubscribedGroups.end())
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::autoDownloadEnabled() No Entry";
        std::cerr << std::endl;
#endif

        return false;
    }

    /* extract from ServiceString */
    SSGxsChatGroup ss;
    ss.load(it->second.mServiceString);
    enabled = ss.mAutoDownload;

#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::autoDownloadEnabled.mServiceString =" << it->second.mServiceString << " and enabled="<<enabled;
    std::cerr << std::endl;
#endif
    return true;
}

bool p3GxsChats::setAutoDownload(const RsGxsGroupId &groupId, bool enabled)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::setAutoDownload() id: " << groupId << " enabled: " << enabled;
    std::cerr << std::endl;
#endif

    std::map<RsGxsGroupId, RsGroupMetaData>::iterator it;

    it = mSubscribedGroups.find(groupId);
    if (it == mSubscribedGroups.end())
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::setAutoDownload() Missing Group";
        std::cerr << std::endl;
#endif

        return false;
    }

    /* extract from ServiceString */
    SSGxsChatGroup ss;
    ss.load(it->second.mServiceString);
    if (enabled == ss.mAutoDownload)
    {
        /* it should be okay! */
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::setAutoDownload() WARNING setting looks okay already";
        std::cerr << std::endl;
#endif

    }

    /* we are just going to set it anyway. */
    ss.mAutoDownload = enabled;
    std::string serviceString = ss.save();
    uint32_t token;

    it->second.mServiceString = serviceString; // update Local Cache.
    RsGenExchange::setGroupServiceString(token, groupId, serviceString); // update dbase.

    /* now reload it */
    std::list<RsGxsGroupId> groups;
    groups.push_back(groupId);

    request_SpecificSubscribedGroups(groups);

    return true;
}

bool p3GxsChats::setChannelAutoDownload(const RsGxsGroupId &groupId, bool enabled)
{
    return setAutoDownload(groupId, enabled);
}


bool p3GxsChats::getChannelAutoDownload(const RsGxsGroupId &groupId, bool& enabled)
{
    return autoDownloadEnabled(groupId,enabled);
}

bool p3GxsChats::setChannelDownloadDirectory(const RsGxsGroupId &groupId, const std::string& directory)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::setDownloadDirectory() id: " << groupId << " to: " << directory << std::endl;
#endif

    std::map<RsGxsGroupId, RsGroupMetaData>::iterator it;

    it = mSubscribedGroups.find(groupId);
    if (it == mSubscribedGroups.end())
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::setAutoDownload() Missing Group" << std::endl;
#endif
        return false;
    }

    /* extract from ServiceString */
    SSGxsChatGroup ss;
    ss.load(it->second.mServiceString);

    if (directory == ss.mDownloadDirectory)
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::setDownloadDirectory() WARNING setting looks okay already" << std::endl;
#endif

    }

    /* we are just going to set it anyway. */
    ss.mDownloadDirectory = directory;
    std::string serviceString = ss.save();
    uint32_t token;

    it->second.mServiceString = serviceString; // update Local Cache.
    RsGenExchange::setGroupServiceString(token, groupId, serviceString); // update dbase.

    /* now reload it */
    std::list<RsGxsGroupId> groups;
    groups.push_back(groupId);

    request_SpecificSubscribedGroups(groups);

    return true;
}

bool p3GxsChats::getChannelDownloadDirectory(const RsGxsGroupId & groupId,std::string& directory)
{
#ifdef GXSCHATS_DEBUG
    std::cerr << "p3GxsChats::getChannelDownloadDirectory(" << groupId << ")" << std::endl;
#endif

    std::map<RsGxsGroupId, RsGroupMetaData>::iterator it;

    it = mSubscribedGroups.find(groupId);

    if (it == mSubscribedGroups.end())
    {
#ifdef GXSCHATS_DEBUG
        std::cerr << "p3GxsChats::getChannelDownloadDirectory() No Entry" << std::endl;
#endif

        return false;
    }

    /* extract from ServiceString */
    SSGxsChatGroup ss;
    ss.load(it->second.mServiceString);
    directory = ss.mDownloadDirectory;

    return true ;
}
