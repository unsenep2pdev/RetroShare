/****************************************************************
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2013 Robert Fernie
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

#include <QBuffer>

#include "UnseenGxsChatGroupDialog.h"

#include <retroshare/rsgxschats.h>
#include <iostream>

#include <retroshare/rspeers.h>
// To start with we only have open forums - with distribution controls.

const uint32_t ChannelCreateEnabledFlags = (
            GXS_GROUP_FLAGS_NAME        |
            GXS_GROUP_FLAGS_ICON        |
            GXS_GROUP_FLAGS_DESCRIPTION   |
            GXS_GROUP_FLAGS_DISTRIBUTION  |
            // GXS_GROUP_FLAGS_PUBLISHSIGN   |
            // GXS_GROUP_FLAGS_SHAREKEYS     |	// disabled because the UI doesn't handle it, so no need to show the disabled button. The user can do it in a second step from the channel menu.
            // GXS_GROUP_FLAGS_PERSONALSIGN  |
            GXS_GROUP_FLAGS_COMMENTS      |
            0);

const uint32_t ChannelCreateDefaultsFlags = ( GXS_GROUP_DEFAULTS_DISTRIB_PUBLIC    |
            //GXS_GROUP_DEFAULTS_DISTRIB_GROUP        |
            //GXS_GROUP_DEFAULTS_DISTRIB_LOCAL        |

            GXS_GROUP_DEFAULTS_PUBLISH_OPEN         |
            //GXS_GROUP_DEFAULTS_PUBLISH_THREADS      |
            //GXS_GROUP_DEFAULTS_PUBLISH_REQUIRED     |
            //GXS_GROUP_DEFAULTS_PUBLISH_ENCRYPTED    |

            //GXS_GROUP_DEFAULTS_PERSONAL_GPG         |
            GXS_GROUP_DEFAULTS_PERSONAL_REQUIRED    |
            //GXS_GROUP_DEFAULTS_PERSONAL_IFNOPUB     |

            GXS_GROUP_DEFAULTS_COMMENTS_YES         |
            //GXS_GROUP_DEFAULTS_COMMENTS_NO          |
            0);

const uint32_t ChannelEditEnabledFlags = ChannelCreateEnabledFlags;
const uint32_t ChannelEditDefaultsFlags = ChannelCreateDefaultsFlags;

UnseenGxsChatGroupDialog::UnseenGxsChatGroupDialog(TokenQueue *tokenQueue, QWidget *parent)
    : UnseenGxsGroupDialog(tokenQueue, ChannelCreateEnabledFlags, ChannelCreateDefaultsFlags, parent)
{
}

UnseenGxsChatGroupDialog::UnseenGxsChatGroupDialog(TokenQueue *tokenExternalQueue, RsTokenService *tokenService, Mode mode, RsGxsGroupId groupId, QWidget *parent)
    : UnseenGxsGroupDialog(tokenExternalQueue, tokenService, mode, groupId, ChannelEditEnabledFlags, ChannelEditDefaultsFlags, parent)
{
    GxsChatMember chatId;
    rsGxsChats->getOwnMember(chatId);
    ownChatId=chatId;
    members.insert(chatId);
    chattype=RsGxsChatGroup::GROUPCHAT;  //set default for now...


}

void UnseenGxsChatGroupDialog::initUi()
{
    switch (mode())
    {
    case MODE_CREATE:
        setUiText(UITYPE_SERVICE_HEADER, tr("Create New Conversation"));
        setUiText(UITYPE_BUTTONBOX_OK,   tr("Create"));
        break;
    case MODE_SHOW:
        setUiText(UITYPE_SERVICE_HEADER, tr("Conversation"));
        break;
    case MODE_EDIT:
        setUiText(UITYPE_SERVICE_HEADER, tr("Edit Conversation"));
        setUiText(UITYPE_BUTTONBOX_OK,   tr("Update Conversation"));
        break;
    }

    setUiText(UITYPE_KEY_SHARE_CHECKBOX, tr("Add Conversation Admins"));
    setUiText(UITYPE_CONTACTS_DOCK,      tr("Select Conversation Admins"));
}

QPixmap UnseenGxsChatGroupDialog::serviceImage()
{
    switch (mode())
    {
    case MODE_CREATE:
        return QPixmap(":/icons/png/channels.png");
    case MODE_SHOW:
        return QPixmap(":/icons/png/channels.png");
    case MODE_EDIT:
        return QPixmap(":/icons/png/channels.png");
    }

    return QPixmap();
}

void UnseenGxsChatGroupDialog::prepareGxsChatGroup(RsGxsChatGroup &group, const RsGroupMetaData &meta)
{
    group.mMeta = meta;
    group.mDescription = getDescription().toUtf8().constData();

    //unseenp2p a- meiyousixin: try to get all member list from the Create Conversation Dialog
    std::set<GxsChatMember> selectedList;
    this->getShareFriends(selectedList);
    members = selectedList;

    //End of get member list
    //adding itself to the group, otherwise, your friend won't have your information to response to.
    GxsChatMember owner;
    if(rsGxsChats->getOwnMember(owner))
        members.insert(owner);  //otherwise will failed
    else{
        return;
    }

    group.members= members;

    //unseen2p - meiyousixin - add more function from UnseenGxsGroupDialog to get chat type
    group.type= chattype = this->getChatType();

    QPixmap pixmap = getLogo();

    if (!pixmap.isNull()) {
        QByteArray ba;
        QBuffer buffer(&ba);

        buffer.open(QIODevice::WriteOnly);
        pixmap.save(&buffer, "PNG"); // writes image into ba in PNG format

        group.mImage.copy((uint8_t *) ba.data(), ba.size());
    } else {
        group.mImage.clear();
    }
}

bool UnseenGxsChatGroupDialog::service_CreateGroup(uint32_t &token, const RsGroupMetaData &meta)
{
    // Specific Function.
    RsGxsChatGroup grp;
    prepareGxsChatGroup(grp, meta);

    rsGxsChats->createGroup(token, grp);
    return true;
}

bool UnseenGxsChatGroupDialog::service_EditGroup(uint32_t &token, RsGroupMetaData &editedMeta)
{
    RsGxsChatGroup grp;
    prepareGxsChatGroup(grp, editedMeta);

    std::cerr << "GxsChatGroupDialog::service_EditGroup() submitting changes";
    std::cerr << std::endl;

    rsGxsChats->updateGroup(token, grp);
    return true;
}


bool UnseenGxsChatGroupDialog::service_loadGroup(uint32_t token, Mode /*mode*/, RsGroupMetaData& groupMetaData, QString &description)
{
    std::cerr << "GxsChatGroupDialog::service_loadGroup(" << token << ")";
    std::cerr << std::endl;

    std::vector<RsGxsChatGroup> groups;
    if (!rsGxsChats->getGroupData(token, groups))
    {
        std::cerr << "GxsChatGroupDialog::service_loadGroup() Error getting GroupData";
        std::cerr << std::endl;
        return false;
    }

    if (groups.size() != 1)
    {
        std::cerr << "GxsChatGroupDialog::service_loadGroup() Error Group.size() != 1";
        std::cerr << std::endl;
        return false;
    }

    std::cerr << "GxsChatsGroupDialog::service_loadGroup() Unfinished Loading";
    std::cerr << std::endl;

    const RsGxsChatGroup &group = groups[0];
    groupMetaData = group.mMeta;
    description = QString::fromUtf8(group.mDescription.c_str());

    members = group.members;
    chattype=group.type;

    if (group.mImage.mData) {
        QPixmap pixmap;
        if (pixmap.loadFromData(group.mImage.mData, group.mImage.mSize, "PNG")) {
            setLogo(pixmap);
        }
    }


    return true;
}

bool UnseenGxsChatGroupDialog::Service_AddMembers(uint32_t &token, RsGroupMetaData &editedMeta, std::set<GxsChatMember> friendlist){

    std::copy(friendlist.begin(), friendlist.end(),std::inserter(members, members.begin()));

    RsGxsChatGroup grp;
    prepareGxsChatGroup(grp, editedMeta);

    std::cerr << "GxsChatGroupDialog::Service_AddMembers() submitting changes";
    std::cerr << std::endl;

    rsGxsChats->updateGroup(token, grp);
    return true;
}

bool UnseenGxsChatGroupDialog::Service_RemoveMembers(uint32_t &token, RsGroupMetaData &editedMeta, std::set<GxsChatMember> friendlist){

    for(auto it=friendlist.begin(); it !=friendlist.end(); it++){
        members.erase(*it);
    }
    //need to remove from groupNode to prevent GSX Sync to these members.

    RsGxsChatGroup grp;
    prepareGxsChatGroup(grp, editedMeta);

    std::cerr << "GxsChatGroupDialog::Service_RemoveMembers() submitting changes";
    std::cerr << std::endl;

    rsGxsChats->updateGroup(token, grp);
    return true;
}


