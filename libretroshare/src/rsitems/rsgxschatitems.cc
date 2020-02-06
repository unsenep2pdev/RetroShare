/*******************************************************************************
 * libretroshare/src/rsitems: rsgxschannelitems.cc                             *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright 2012-2012 by Robert Fernie <retroshare@lunamutt.com>              *
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
#include <iostream>

#include "rsgxschatitems.h"
#include "serialiser/rstlvbase.h"
#include "serialiser/rsbaseserial.h"

#include "serialiser/rstypeserializer.h"


RsItem *RsGxsChatSerialiser::create_item(uint16_t service_id,uint8_t item_subtype) const
{
    if(service_id !=  RS_SERVICE_GXS_TYPE_CHATS)
        return NULL ;

    switch(item_subtype)
    {
    case RS_PKT_SUBTYPE_GXSCHAT_GROUP_ITEM: return new RsGxsChatGroupItem() ;
    case RS_PKT_SUBTYPE_GXSCHAT_POST_ITEM:  return new RsGxsChatMsgItem();
    default:
        return RsGxsCommentSerialiser::create_item(service_id,item_subtype) ;
    }
}

void RsGxsChatGroupItem::clear()
{
    mDescription.clear();
    mImage.TlvClear();
    members.clear();
}

bool RsGxsChatGroupItem::fromChatGroup(RsGxsChatGroup &group, bool moveImage)
{
    clear();
    meta = group.mMeta;
    mDescription = group.mDescription;
    type=group.type;
    members = group.members;

    if (moveImage)
    {
        mImage.binData.bin_data = group.mImage.mData;
        mImage.binData.bin_len = group.mImage.mSize;
        group.mImage.shallowClear();
    }
    else
    {
        mImage.binData.setBinData(group.mImage.mData, group.mImage.mSize);
    }
    return true;
}

bool RsGxsChatGroupItem::toChatGroup(RsGxsChatGroup &group, bool moveImage)
{
    group.mMeta = meta;
    group.mDescription = mDescription;
    group.type = type;
    group.members = members;

    if (moveImage)
    {
        group.mImage.take((uint8_t *) mImage.binData.bin_data, mImage.binData.bin_len);
        // mImage doesn't have a ShallowClear at the moment!
        mImage.binData.TlvShallowClear();
    }
    else
    {
        group.mImage.copy((uint8_t *) mImage.binData.bin_data, mImage.binData.bin_len);
    }
    return true;
}

void RsGxsChatGroupItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process           (j,ctx,TLV_TYPE_STR_DESCR,mDescription,"mDescription") ;
    RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,mImage,"mImage") ;
    RS_SERIAL_PROCESS(type);
    RsTypeSerializer::serial_process<GxsChatMember>(j,ctx,members,"members");
}

bool RsGxsChatMsgItem::fromChatPost(RsGxsChatMsg &post, bool moveImage)
{
    clear();
    meta = post.mMeta;
    mMsg = post.mMsg;

    if (moveImage)
    {
        mThumbnail.binData.bin_data = post.mThumbnail.mData;
        mThumbnail.binData.bin_len = post.mThumbnail.mSize;
        post.mThumbnail.shallowClear();
    }
    else
    {
        mThumbnail.binData.setBinData(post.mThumbnail.mData, post.mThumbnail.mSize);
    }

    std::list<RsGxsFile>::iterator fit;
    for(fit = post.mFiles.begin(); fit != post.mFiles.end(); ++fit)
    {
        RsTlvFileItem fi;
        fi.name = fit->mName;
        fi.filesize = fit->mSize;
        fi.hash = fit->mHash;
        mAttachment.items.push_back(fi);
    }
    return true;
}



bool RsGxsChatMsgItem::toChatPost(RsGxsChatMsg &post, bool moveImage)
{
        post.mMeta = meta;
        post.mMsg = mMsg;
        if (moveImage)
        {
            post.mThumbnail.take((uint8_t *) mThumbnail.binData.bin_data, mThumbnail.binData.bin_len);
            // mThumbnail doesn't have a ShallowClear at the moment!
            mThumbnail.binData.TlvShallowClear();
        }
        else
        {
            post.mThumbnail.copy((uint8_t *) mThumbnail.binData.bin_data, mThumbnail.binData.bin_len);
        }

        post.mCount = 0;
        post.mSize = 0;
        std::list<RsTlvFileItem>::iterator fit;
        for(fit = mAttachment.items.begin(); fit != mAttachment.items.end(); ++fit)
        {
            RsGxsFile fi;
            fi.mName = RsDirUtil::getTopDir(fit->name);
            fi.mSize  = fit->filesize;
            fi.mHash  = fit->hash;

            post.mFiles.push_back(fi);
            post.mCount++;
            post.mSize += fi.mSize;
        }
        return true;

}

void RsGxsChatMsgItem::clear()
{
    mMsg.clear();
    mAttachment.TlvClear();
    mThumbnail.TlvClear();
}

void RsGxsChatMsgItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process           (j,ctx,TLV_TYPE_STR_MSG,mMsg,"mMsg") ;
    RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,mAttachment,"mAttachment") ;
    RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,mThumbnail,"mThumbnail") ;
}

