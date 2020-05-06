/*******************************************************************************
 * libretroshare/src/chat: rschatitems.cc                                      *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright 2007-2008 by Robert Fernie.                                       *
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

#include <stdexcept>
#include "util/rstime.h"
#include "serialiser/rsbaseserial.h"
#include "serialiser/rstlvbase.h"

#include "serialiser/rstypeserializer.h"

#include "chat/rschatitems.h"

//#define CHAT_DEBUG 1

//static const uint32_t RS_CHAT_SERIALIZER_FLAGS_NO_SIGNATURE = 0x0001;

RsItem *RsChatSerialiser::create_item(uint16_t service_id,uint8_t item_sub_id) const
{
	if(service_id != RS_SERVICE_TYPE_CHAT) return NULL;

	switch(item_sub_id)
	{
	case RS_PKT_SUBTYPE_DEFAULT: return new RsChatMsgItem();
	case RS_PKT_SUBTYPE_PRIVATECHATMSG_CONFIG: return new RsPrivateChatMsgConfigItem();
	case RS_PKT_SUBTYPE_CHAT_STATUS: return new RsChatStatusItem();
	case RS_PKT_SUBTYPE_CHAT_AVATAR: return new RsChatAvatarItem();
	case RS_PKT_SUBTYPE_CHAT_LOBBY_SIGNED_MSG: return new RsChatLobbyMsgItem();
	case RS_PKT_SUBTYPE_CHAT_LOBBY_INVITE: return new RsChatLobbyInviteItem();
	case RS_PKT_SUBTYPE_CHAT_LOBBY_INVITE_DEPRECATED: return new RsChatLobbyInviteItem_Deprecated(); // to be removed (deprecated since May 2017)
	case RS_PKT_SUBTYPE_CHAT_LOBBY_CHALLENGE: return new RsChatLobbyConnectChallengeItem();
	case RS_PKT_SUBTYPE_CHAT_LOBBY_UNSUBSCRIBE: return new RsChatLobbyUnsubscribeItem();
	case RS_PKT_SUBTYPE_CHAT_LOBBY_SIGNED_EVENT: return new RsChatLobbyEventItem();
	case RS_PKT_SUBTYPE_CHAT_LOBBY_LIST_REQUEST: return new RsChatLobbyListRequestItem();
	case RS_PKT_SUBTYPE_CHAT_LOBBY_LIST: return new RsChatLobbyListItem();
	case RS_PKT_SUBTYPE_CHAT_LOBBY_CONFIG: return new RsChatLobbyConfigItem();
    case RS_PKT_SUBTYPE_CHAT_LOBBY_INFO: return new RsChatLobbyInfoItem();
	case RS_PKT_SUBTYPE_OUTGOING_MAP: return new PrivateOugoingMapItem();
    case RS_PKT_SUBTYPE_GXSCHAT_GROUP: return new GxsNxsChatGroupItem();  //adding direct chat gxs group message
    case RS_PKT_SUBTYPE_GXSCHAT_MSG: return new GxsNxsChatMsgItem();          //adding direct chat gxs message
    case RS_PKT_SUBTYPE_GXSCHAT_PUBLISH_KEY: return new GxsNxsGroupPublishKeyItem();  //adding direct share publish key gxs group
    case RS_PKT_SUBTYPE_GXSCHAT_REQUEST: return new RsChatMsgItem(RS_PKT_SUBTYPE_GXSCHAT_REQUEST);
    case RS_PKT_SUBTYPE_GXSCHAT_ACTKN:   return new RsChatMsgItem(RS_PKT_SUBTYPE_GXSCHAT_ACTKN);
    case RS_PKT_SUBTYPE_GXSCHAT_APPROVED: return new RsChatMsgItem(RS_PKT_SUBTYPE_GXSCHAT_APPROVED);
    case RS_PKT_SUBTYPE_GXSCHAT_REJECT: return new RsChatMsgItem(RS_PKT_SUBTYPE_GXSCHAT_REJECT);
    default:
		std::cerr << "Unknown packet type in chat!" << std::endl;
		return NULL;
	}
}

void RsChatMsgItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process(j,ctx,chatFlags,"chatflags") ;
    RsTypeSerializer::serial_process(j,ctx,sendTime,"sendTime") ;
    RsTypeSerializer::serial_process(j,ctx,TLV_TYPE_STR_MSG,message,"message") ;
}

/*************************************************************************/

void GxsNxsChatMsgItem::serial_process( RsGenericSerializer::SerializeJob j,
                               RsGenericSerializer::SerializeContext& ctx )
{
    RS_SERIAL_PROCESS(transactionNumber);
    RS_SERIAL_PROCESS(pos);
    RS_SERIAL_PROCESS(msgId);
    RS_SERIAL_PROCESS(grpId);
    RS_SERIAL_PROCESS(msg);
    RS_SERIAL_PROCESS(meta);
}

void GxsNxsChatGroupItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process<uint32_t> (j,ctx,transactionNumber,"transactionNumber") ;
    RsTypeSerializer::serial_process<uint8_t>  (j,ctx,pos              ,"pos") ;
    RsTypeSerializer::serial_process           (j,ctx,grpId            ,"grpId") ;
    RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,grp              ,"grp") ;
    RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,meta             ,"meta") ;
}

void GxsNxsGroupPublishKeyItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process           (j,ctx,grpId            ,"grpId") ;
    RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,private_key      ,"private_key") ;
}

void GxsNxsGroupPublishKeyItem::clear()
{
    private_key.TlvClear();
}

RsChatAvatarItem::~RsChatAvatarItem()
{
	if(image_data != NULL)
	{
		free(image_data) ;
		image_data = NULL ;
	}
}

void RsChatLobbyBouncingObject::serial_process(RsGenericSerializer::SerializeJob j, RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process(j,ctx,lobby_id,"lobby_id") ;
    RsTypeSerializer::serial_process(j,ctx,msg_id  ,"msg_id") ;
    RsTypeSerializer::serial_process(j,ctx,TLV_TYPE_STR_NAME,nick,"nick") ;

    if(!(ctx.mFlags & RsServiceSerializer::SERIALIZATION_FLAG_SIGNATURE))
    	RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,signature,"signature") ;
}

void RsChatLobbyMsgItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsChatMsgItem::serial_process(j,ctx) ;
    RsTypeSerializer::serial_process(j,ctx,parent_msg_id,"parent_msg_id") ;
    RsChatLobbyBouncingObject::serial_process(j,ctx) ;
}

void RsChatLobbyListRequestItem::serial_process(RsGenericSerializer::SerializeJob /*j*/,RsGenericSerializer::SerializeContext& /*ctx*/)
{
    // nothing to do. This is an empty item.
}

template<> void RsTypeSerializer::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx,VisibleChatLobbyInfo& info,const std::string& /*name*/)
{
	RsTypeSerializer::serial_process<uint64_t>(j,ctx,info.id,"info.id") ;

	RsTypeSerializer::serial_process          (j,ctx,TLV_TYPE_STR_NAME,info.name, "info.name") ;
	RsTypeSerializer::serial_process          (j,ctx,TLV_TYPE_STR_NAME,info.topic,"info.topic") ;
	RsTypeSerializer::serial_process<uint32_t>(j,ctx,                  info.count,"info.count") ;
	RsTypeSerializer::serial_process          (j,ctx,                  info.flags,"info.flags") ;
}

void RsChatLobbyListItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process(j,ctx,lobbies,"lobbies") ;
}

void RsChatLobbyEventItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process<uint8_t>(j,ctx,event_type,"event_type") ;
    RsTypeSerializer::serial_process         (j,ctx,TLV_TYPE_STR_NAME,string1,"string1") ;
    RsTypeSerializer::serial_process<uint32_t>(j,ctx,sendTime ,"sendTime") ;

    RsChatLobbyBouncingObject::serial_process(j,ctx) ;
}
void RsChatLobbyUnsubscribeItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process<uint64_t>(j,ctx,lobby_id,"lobby_id") ;
}

void RsChatLobbyConnectChallengeItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process<uint64_t>(j,ctx,challenge_code,"challenge_code") ;
}

// to be removed (deprecated since May 2017)
void RsChatLobbyInviteItem_Deprecated::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process<uint64_t>(j,ctx,                  lobby_id,   "lobby_id") ;
    RsTypeSerializer::serial_process          (j,ctx,TLV_TYPE_STR_NAME,lobby_name, "lobby_name") ;
    RsTypeSerializer::serial_process          (j,ctx,                  lobby_flags,"lobby_flags") ;
}

void RsChatLobbyInviteItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process<uint64_t>(j,ctx,                  lobby_id,   "lobby_id") ;
    RsTypeSerializer::serial_process          (j,ctx,TLV_TYPE_STR_NAME,lobby_name, "lobby_name") ;
    RsTypeSerializer::serial_process          (j,ctx,TLV_TYPE_STR_NAME,lobby_topic,"lobby_topic") ;
    RsTypeSerializer::serial_process          (j,ctx,                  lobby_flags,"lobby_flags") ;
}

void RsPrivateChatMsgConfigItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    uint32_t x=0 ;

    RsTypeSerializer::serial_process<uint32_t>(j,ctx,                 x,           "place holder value") ;
    RsTypeSerializer::serial_process          (j,ctx,                 configPeerId,"configPeerId") ;
    RsTypeSerializer::serial_process<uint32_t>(j,ctx,                 chatFlags,   "chatFlags") ;
    RsTypeSerializer::serial_process<uint32_t>(j,ctx,                 sendTime,    "sendTime") ;
    RsTypeSerializer::serial_process          (j,ctx,TLV_TYPE_STR_MSG,message,     "message") ;
    RsTypeSerializer::serial_process<uint32_t>(j,ctx,                 recvTime,    "recvTime") ;
}

void RsChatStatusItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process(j,ctx,flags,"flags") ;
    RsTypeSerializer::serial_process(j,ctx,TLV_TYPE_STR_MSG,status_string,"status_string") ;
}

void RsChatAvatarItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::TlvMemBlock_proxy b(image_data,image_size) ;
    RsTypeSerializer::serial_process(j,ctx,b,"image data") ;
}

void RsChatLobbyConfigItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process<uint64_t>(j,ctx,lobby_Id,"lobby_Id") ;
    RsTypeSerializer::serial_process(j,ctx,flags,"flags") ;
}


void RsChatLobbyInfoItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process<uint64_t>(j,ctx,lobby_Id,"lobby_Id") ;
    RsTypeSerializer::serial_process(j,ctx,lobbyInfo,"lobbyInfo") ;
}
/* set data from RsChatMsgItem to RsPrivateChatMsgConfigItem */
void RsPrivateChatMsgConfigItem::set(RsChatMsgItem *ci, const RsPeerId& /*peerId*/, uint32_t confFlags)
{
	PeerId(ci->PeerId());
	configPeerId = ci->PeerId();
	chatFlags = ci->chatFlags;
	configFlags = confFlags;
	sendTime = ci->sendTime;
	message = ci->message;
	recvTime = ci->recvTime;
}

/* get data from RsPrivateChatMsgConfigItem to RsChatMsgItem */
void RsPrivateChatMsgConfigItem::get(RsChatMsgItem *ci)
{
	ci->PeerId(configPeerId);
	ci->chatFlags = chatFlags;
	//configFlags not used
	ci->sendTime = sendTime;
	ci->message = message;
	ci->recvTime = recvTime;
}


void PrivateOugoingMapItem::serial_process(
        RsGenericSerializer::SerializeJob j,
        RsGenericSerializer::SerializeContext& ctx )
{ RS_SERIAL_PROCESS(store); }

int GxsNxsChatGroupItem::refcount = 0;
/** print and clear functions **/
int GxsNxsChatMsgItem::refcount = 0;

void GxsNxsChatMsgItem::clear()
{

    msg.TlvClear();
    meta.TlvClear();
}


std::ostream&GxsNxsChatMsgItem::print(std::ostream& out, uint16_t /*indent*/)
{ return out; }

void GxsNxsChatGroupItem::clear()
{
    grpId.clear();
    grp.TlvClear();
    meta.TlvClear();
}
