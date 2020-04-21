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

#ifndef UNSEENGXSCHATGROUPDIALOG_H
#define UNSEENGXSCHATGROUPDIALOG_H


//#include "gui/gxs/GxsGroupDialog.h"
#include "gui/gxs/UnseenGxsGroupDialog.h"
#include <retroshare/rsgxschats.h>
//unseenp2p comment:
//The GxsGroupDialog GUI is common for all 3 GUI:
// 1. channel,
// 2. forum,
// 3. and posted links,
// it show all elements of these types: message distributions, allow comments,...

// For UnseenP2P we will customize this class as UnseenGxsGroupDialog instead of common GxsGroupDialog
// the UnseenGxsGroupDialog will add the contact list so that user can choose the adding member when creating new gxs Groupchat

class UnseenGxsChatGroupDialog : public UnseenGxsGroupDialog
{
    Q_OBJECT

public:
    UnseenGxsChatGroupDialog(TokenQueue *tokenQueue, QWidget *parent);
    UnseenGxsChatGroupDialog(TokenQueue *tokenExternalQueue, RsTokenService *tokenService, Mode mode, RsGxsChatGroup::ChatType chatType, RsGxsGroupId groupId, QWidget *parent = NULL);

protected:
    virtual void initUi();
    virtual QPixmap serviceImage();
    virtual bool service_CreateGroup(uint32_t &token, const RsGroupMetaData &meta);
    virtual bool service_loadGroup(uint32_t token, Mode mode, RsGroupMetaData& groupMetaData, QString &description);
    virtual bool service_EditGroup(uint32_t &token, RsGroupMetaData &editedMeta);

    virtual bool Service_AddMembers(uint32_t &token, RsGroupMetaData &editedMeta, std::set<GxsChatMember> friendlist);
    virtual bool Service_RemoveMembers(uint32_t &token, RsGroupMetaData &editedMeta, std::set<GxsChatMember> friendlist);

private:
    void prepareGxsChatGroup(RsGxsChatGroup &group, const RsGroupMetaData &meta);
    std::set<GxsChatMember> members;
    RsGxsChatGroup::ChatType chattype;
    GxsChatMember ownChatId;
};


#endif // UNSEENGXSCHATGROUPDIALOG_H
