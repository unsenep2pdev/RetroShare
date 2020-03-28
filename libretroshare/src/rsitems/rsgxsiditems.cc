/*******************************************************************************
 * libretroshare/src/rsitems: rsgxsiditems.cc                                  *
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

#include "rsgxsiditems.h"
#include "serialiser/rstlvbase.h"
#include "serialiser/rsbaseserial.h"
#include "serialiser/rstlvstring.h"
#include "util/rsstring.h"

//#define GXSID_DEBUG	1

RsItem *RsGxsIdSerialiser::create_item(uint16_t service_id,uint8_t item_subtype) const
{
    if(service_id != RS_SERVICE_GXS_TYPE_GXSID)
        return NULL ;

    switch(item_subtype)
    {
    case RS_PKT_SUBTYPE_GXSID_GROUP_ITEM     : return new RsGxsIdGroupItem ();
    case RS_PKT_SUBTYPE_GXSID_LOCAL_INFO_ITEM: return new RsGxsIdLocalInfoItem() ;
    default:
        return NULL ;
    }
}

RsItem *RsGxsIdSerialiser::deserialise(void *data, uint32_t *size)
{
    //doing a default deserialise first, it is failed. Try to deserialize as backward compatibility.
    RsItem *item = RsServiceSerializer::deserialise(data, size);

    if(item){
        return item;
    }


    if(mFlags & SERIALIZATION_FLAG_SKIP_HEADER)
    {
        std::cerr << "(EE) Cannot deserialise item with flags SERIALIZATION_FLAG_SKIP_HEADER. Check your code!" << std::endl;
        return NULL ;
    }

    uint32_t rstype = getRsItemId(const_cast<void*>((const void*)data)) ;

    item = create_item(getRsItemService(rstype),getRsItemSubType(rstype)) ;

    if(!item)
    {
        std::cerr << "(EE) " << typeid(*this).name() << ": cannot deserialise unknown item subtype " << std::hex << (int)getRsItemSubType(rstype) << std::dec << std::endl;
        std::cerr << "(EE) Data is: " << RsUtil::BinToHex(static_cast<uint8_t*>(data),std::min(50u,*size)) << ((*size>50)?"...":"") << std::endl;
        return NULL ;
    }

    SerializeContext ctx(const_cast<uint8_t*>(static_cast<uint8_t*>(data)),*size,mFormat,mFlags);
    ctx.mOffset = 8 ;

    //if we have multiple backward compatible version, then we need to loop through to test with version is the actual data.
    RsGxsIdGroupItem *item2 = dynamic_cast<RsGxsIdGroupItem*>(item);
    item2->version = V70;

    item2->serial_process(RsGenericSerializer::DESERIALIZE, ctx) ;

    if(ctx.mSize < ctx.mOffset)
    {
        std::cerr << "RsSerializer::deserialise(): ERROR. offset does not match expected size!" << std::endl;
        delete item ;
        return NULL ;
    }
    *size = ctx.mOffset ;

    if(ctx.mOk)
        return item ;

    delete item ;
    return NULL ;
}
void RsGxsIdLocalInfoItem::clear()
{
    mTimeStamps.clear() ;
}
void RsGxsIdGroupItem::clear()
{
    mPgpIdHash.clear();
    mPgpIdSign.clear();

    mRecognTags.clear();
    mImage.TlvClear();
    profileInfo.clear();
}
void RsGxsIdLocalInfoItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process(j,ctx,mTimeStamps,"mTimeStamps") ;
    RsTypeSerializer::serial_process(j,ctx,mContacts,"mContacts") ;
}

void RsGxsMyContact::clear()
{
    mContactInfo.clear() ;
}
void RsGxsMyContact::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx){

    RsTypeSerializer::serial_process(j,ctx,name,"name") ;
    RsTypeSerializer::serial_process(j,ctx,gxsId,"gsxId") ;
    RsTypeSerializer::serial_process(j,ctx,mPgpId,"mPgpId") ;
    RsTypeSerializer::serial_process(j,ctx,peerId,"sslId") ;
    RsTypeSerializer::serial_process(j,ctx,status,"status") ;
    RsTypeSerializer::serial_process<std::string,std::string>(j,ctx,mContactInfo,"mContactInfo");
}
void RsGxsIdGroupItem::serial_process(RsGenericSerializer::SerializeJob j,RsGenericSerializer::SerializeContext& ctx)
{
    RsTypeSerializer::serial_process(j,ctx,mPgpIdHash,"mPgpIdHash") ;
    RsTypeSerializer::serial_process(j,ctx,TLV_TYPE_STR_SIGN,mPgpIdSign,"mPgpIdSign") ;

    RsTlvStringSetRef rset(TLV_TYPE_RECOGNSET,mRecognTags) ;

    RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,rset,"mRecognTags") ;

    // image is optional

    if(j == RsGenericSerializer::DESERIALIZE && ctx.mOffset == ctx.mSize)
        return ;

    RsTypeSerializer::serial_process<RsTlvItem>(j,ctx,mImage,"mImage") ;

    //backward compatiable support if Unseen version v0.7.0 then we add this into serailization.
    if(current_version==V70){
        RsTypeSerializer::serial_process<std::string,std::string>(j,ctx,profileInfo,"profileInfo") ;
    }
}

bool RsGxsIdGroupItem::fromGxsIdGroup(RsGxsIdGroup &group, bool moveImage)
{
        clear();
        meta = group.mMeta;
        mPgpIdHash = group.mPgpIdHash;
        mPgpIdSign = group.mPgpIdSign;
        mRecognTags = group.mRecognTags;
        version = group.version;

        if (version==V70)
            profileInfo = group.profileInfo;

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
    return true ;
}
bool RsGxsIdGroupItem::toGxsIdGroup(RsGxsIdGroup &group, bool moveImage)
{
        group.mMeta = meta;
        group.mPgpIdHash = mPgpIdHash;
        group.mPgpIdSign = mPgpIdSign;
        group.mRecognTags = mRecognTags;
        group.version = version;

        if (version==V70)
            group.profileInfo = profileInfo;

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
    return true ;
}

