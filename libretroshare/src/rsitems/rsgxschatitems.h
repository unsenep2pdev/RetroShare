#ifndef RSGXSCHATITEMS_H
#define RSGXSCHATITEMS_H

#include <map>

#include "rsitems/rsserviceids.h"
#include "rsitems/rsgxscommentitems.h"
#include "rsitems/rsgxsitems.h"

#include "serialiser/rstlvfileitem.h"
#include "serialiser/rstlvimage.h"

#include "retroshare/rsgxschats.h"

#include "serialiser/rsserializer.h"

#include "util/rsdir.h"

const uint8_t RS_PKT_SUBTYPE_GXSCHAT_GROUP_ITEM = 0x02;
const uint8_t RS_PKT_SUBTYPE_GXSCHAT_POST_ITEM  = 0x03;

class RsGxsChatGroupItem : public RsGxsGrpItem
{
public:

    RsGxsChatGroupItem():  RsGxsGrpItem(RS_SERVICE_GXS_TYPE_CHATS, RS_PKT_SUBTYPE_GXSCHAT_GROUP_ITEM) {}
    virtual ~RsGxsChatGroupItem() {}

    void clear();

    virtual void serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx);

    // use conversion functions to transform:
    bool fromChatGroup(RsGxsChatGroup &group, bool moveImage);
    bool toChatGroup(RsGxsChatGroup &group, bool moveImage);

    std::string mDescription;  //conversation display by name
    RsTlvImage mImage;   //avatar image per conversation
    RsGxsChatGroup::ChatType  type;  //one2one,groupchat, and channel
    std::list<GxsChatMember> members;
};


class RsGxsChatMsgItem : public RsGxsMsgItem
{
public:

    RsGxsChatMsgItem(): RsGxsMsgItem(RS_SERVICE_GXS_TYPE_CHATS, RS_PKT_SUBTYPE_GXSCHAT_POST_ITEM) {}
    virtual ~RsGxsChatMsgItem() {}
    void clear();

    virtual void serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx);

    // Slightly unusual structure.
    // use conversion functions to transform:
    bool fromChatPost(RsGxsChatMsg &post, bool moveImage);
    bool toChatPost(RsGxsChatMsg &post, bool moveImage);

    std::string mMsg;          //text message
    RsTlvFileSet mAttachment;  //file attachment
    RsTlvImage mThumbnail;     //photo attachment | share
};

class RsGxsChatSerialiser : public RsGxsCommentSerialiser
{
public:

    RsGxsChatSerialiser() :RsGxsCommentSerialiser(RS_SERVICE_GXS_TYPE_CHATS) {}
    virtual     ~RsGxsChatSerialiser() {}

    virtual RsItem *create_item(uint16_t service_id,uint8_t item_subtype) const ;
};

#endif // RSGXSCHATITEMS_H
