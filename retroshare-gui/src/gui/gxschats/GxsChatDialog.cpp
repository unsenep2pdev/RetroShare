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

#include <QMenu>
#include <QFileDialog>

#include <retroshare/rsfiles.h>

#include "GxsChatDialog.h"
#include "GxsChatGroupDialog.h"
//unseenp2p - use the channel code, we create the GxsChatDialog (from GxsChannelDialg), also think as UnseenGxsChatDialog (not exist)
// but we edit the GxsChatDialog for our case as we want:
// 1. It will create the group chat with contact list so that user can add the member to list
// 2. It still keeps all other needed functions for gxs like gxsChat or gxsChannel
#include "UnseenGxsChatGroupDialog.h"
#include "GxsChatPostsWidget.h"
#include "CreateGxsChatMsg.h"
#include "GxsChatUserNotify.h"
#include "gui/gxs/GxsGroupShareKey.h"
#include "gui/feeds/GxsChatPostItem.h"
#include "gui/settings/rsharesettings.h"
#include "gui/notifyqt.h"
#include "gui/common/GroupTreeWidget.h"

//#include "gui/gxschats/UnseenGxsChatLobbyDialog.h"

class GxsChatGroupInfoData : public RsUserdata
{
public:
    GxsChatGroupInfoData() : RsUserdata() {}

public:
    QMap<RsGxsGroupId, QIcon> mIcon;
    QMap<RsGxsGroupId, QString> mDescription;
};

/** Constructor */
GxsChatDialog::GxsChatDialog(QWidget *parent)
    : UnseenGxsGroupFrameDialog(rsGxsChats, parent,true)
{
}

GxsChatDialog::~GxsChatDialog()
{
}

QString GxsChatDialog::getHelpString() const
{
    QString hlp_str = tr("<h1><img width=\"32\" src=\":/home/img/question-64.png\">&nbsp;&nbsp;Channels</h1>    \
    <p>Chats allow you to post data (e.g. movies, music) that will spread in the network</p>            \
    <p>You can see the channels your friends are subscribed to, and you automatically forward subscribed channels to \
    your friends. This promotes good channels in the network.</p>\
    <p>Only the channel's creator can post on that channel. Other peers                       \
    in the network can only read from it, unless the channel is private. You can however share \
     the posting rights or the reading rights with friend UnseenP2P nodes.</p>\
     <p>Channels can be made anonymous, or attached to a UnseenP2P identity so that readers can contact you if needed.\
     Enable \"Allow Comments\" if you want to let users comment on your posts.</p>\
    <p>Channel posts are kept for %1 days, and sync-ed over the last %2 days, unless you change this.</p>\
                ").arg(QString::number(rsGxsChats->getDefaultStoragePeriod()/86400)).arg(QString::number(rsGxsChats->getDefaultSyncPeriod()/86400));

    return hlp_str ;
}

UserNotify *GxsChatDialog::getUserNotify(QObject *parent)
{
    return new GxsChatUserNotify(rsGxsChats, parent);
}

void GxsChatDialog::shareOnChannel(const RsGxsGroupId& channel_id,const QList<RetroShareLink>& file_links)
{
    std::cerr << "Sharing file link on gxschat " << channel_id << ": Not yet implemented!" << std::endl;

    CreateGxsChatMsg *msgDialog = new CreateGxsChatMsg(channel_id) ;

    QString txt ;
    for(QList<RetroShareLink>::const_iterator it(file_links.begin());it!=file_links.end();++it)
        txt += (*it).toHtml() + "\n" ;

    if(!file_links.empty())
    {
        QString subject = (*file_links.begin()).name() ;
        msgDialog->addSubject(subject);
    }

    msgDialog->addHtmlText(txt);
    msgDialog->show();
}

QString GxsChatDialog::text(TextType type)
{
    switch (type) {
    case TEXT_NAME:
        return tr("Conversations");
    case TEXT_NEW:
        return tr("Create Conversation");
    case TEXT_TODO:
        return "<b>Open points:</b><ul>"
                "<li>Restore Conversation keys"
                "</ul>";

    case TEXT_YOUR_GROUP:
        return tr("My Conversations");
    case TEXT_SUBSCRIBED_GROUP:
        return tr("Subscribed Conversations");
    case TEXT_POPULAR_GROUP:
        return tr("Popular Conversations");
    case TEXT_OTHER_GROUP:
        return tr("Other Conversations");
    }

    return "";
}

QString GxsChatDialog::icon(IconType type)
{
    switch (type) {
    case ICON_NAME:
        return ":/home/img/face_icon/youtube-play-button_xu_128.png";      //d: update icon
    case ICON_NEW:
        return ":/icons/png/add.png";
    case ICON_YOUR_GROUP:
        return ":/images/folder16.png";
    case ICON_SUBSCRIBED_GROUP:
        return ":/images/folder_red.png";
    case ICON_POPULAR_GROUP:
        return ":/images/folder_green.png";
    case ICON_OTHER_GROUP:
        return ":/images/folder_yellow.png";
    case ICON_SEARCH:
        return ":/images/find.png";
    case ICON_DEFAULT:
        return ":/images/channels.png";
    }

    return "";
}

UnseenGxsGroupDialog *GxsChatDialog::createNewGroupDialog(TokenQueue *tokenQueue)
{
    //unseenp2p comment:
    //Here we can create a new UnseenGxsChatGroupDialog instead of GxsChatGroupDialog (that inherit from GxsGroupDialog)
    // UnseenGxsChatGroupDialog will take GxsChatGroupDialog functions and add some new functions

    return new UnseenGxsChatGroupDialog(tokenQueue, this);
    //return new GxsChatGroupDisalog(tokenQueue, this); //old code
}

// When user click on Edit details of gxs groupchat/ channel/ one2one
UnseenGxsGroupDialog *GxsChatDialog::createGroupDialog(TokenQueue *tokenQueue, RsTokenService *tokenService, UnseenGxsGroupDialog::Mode mode, RsGxsGroupId groupId)
{
    return new UnseenGxsChatGroupDialog(tokenQueue, tokenService, mode, groupId, this);
    //return new GxsChatGroupDialog(tokenQueue, tokenService, mode, groupId, this); //old code
}

int GxsChatDialog::shareKeyType()
{
    return CHAT_KEY_SHARE;
}

GxsMessageFrameWidget *GxsChatDialog::createMessageFrameWidget(const RsGxsGroupId &groupId)
{
    //rsGxsChats->setSyncPeriod(groupId,300.0);   //reset sync message in 5min for this group
    return new GxsChatPostsWidget(groupId);
    //return new UnseenGxsChatLobbyDialog(groupId);
}

void GxsChatDialog::setDefaultDirectory()
{
    RsGxsGroupId grpId = groupId() ;
    if (grpId.isNull())
        return ;

    rsGxsChats->setChannelDownloadDirectory(grpId,"") ;
}
void GxsChatDialog::specifyDownloadDirectory()
{
    RsGxsGroupId grpId = groupId() ;
    if (grpId.isNull())
        return ;

    QString dir = QFileDialog::getExistingDirectory(NULL,tr("Select chat download directory")) ;

    if(dir.isNull())
        return ;

    rsGxsChats->setChannelDownloadDirectory(grpId,std::string(dir.toUtf8())) ;
}
void GxsChatDialog::setDownloadDirectory()
{
    RsGxsGroupId grpId = groupId() ;
    if (grpId.isNull())
        return ;

    QAction *action = qobject_cast<QAction*>(sender()) ;

    if(!action)
        return ;

    QString directory = action->data().toString() ;

    rsGxsChats->setChannelDownloadDirectory(grpId,std::string(directory.toUtf8())) ;
}
void GxsChatDialog::groupTreeCustomActions(RsGxsGroupId grpId, int subscribeFlags, QList<QAction*> &actions)
{
    bool isSubscribed = IS_GROUP_SUBSCRIBED(subscribeFlags);
    bool autoDownload ;
    rsGxsChats->getChannelAutoDownload(grpId,autoDownload);

    if (isSubscribed)
    {
        QAction *action = autoDownload ? (new QAction(QIcon(":/images/redled.png"), tr("Disable Auto-Download"), this))
                                       : (new QAction(QIcon(":/images/start.png"),tr("Enable Auto-Download"), this));

        connect(action, SIGNAL(triggered()), this, SLOT(toggleAutoDownload()));
        actions.append(action);

        std::string dl_directory;
        rsGxsChats->getChannelDownloadDirectory(grpId,dl_directory) ;

        QMenu *mnu = new QMenu(tr("Set download directory")) ;

        if(dl_directory.empty())
            mnu->addAction(QIcon(":/images/start.png"),tr("[Default directory]"), this, SLOT(setDefaultDirectory())) ;
        else
            mnu->addAction(tr("[Default directory]"), this, SLOT(setDefaultDirectory())) ;

        std::list<SharedDirInfo> lst ;
        rsFiles->getSharedDirectories(lst) ;
        bool found = false ;

        for(std::list<SharedDirInfo>::const_iterator it(lst.begin());it!=lst.end();++it)
        {
            QAction *action = NULL;

            if(dl_directory == it->filename)
            {
                action = new QAction(QIcon(":/images/start.png"),QString::fromUtf8(it->filename.c_str()),NULL) ;
                found = true ;
            }
            else
                action = new QAction(QString::fromUtf8(it->filename.c_str()),NULL) ;

            connect(action,SIGNAL(triggered()),this,SLOT(setDownloadDirectory())) ;
            action->setData(QString::fromUtf8(it->filename.c_str())) ;

            mnu->addAction(action) ;
        }

        if(!found && !dl_directory.empty())
        {
            QAction *action = new QAction(QIcon(":/images/start.png"),QString::fromUtf8(dl_directory.c_str()),NULL) ;
            connect(action,SIGNAL(triggered()),this,SLOT(setDownloadDirectory())) ;
            action->setData(QString::fromUtf8(dl_directory.c_str())) ;

            mnu->addAction(action) ;
        }

        mnu->addAction(tr("Specify..."), this, SLOT(specifyDownloadDirectory())) ;

        actions.push_back( mnu->menuAction()) ;
    }
}

RsGxsCommentService *GxsChatDialog::getCommentService()
{
    return rsGxsChats;
}

QWidget *GxsChatDialog::createCommentHeaderWidget(const RsGxsGroupId &grpId, const RsGxsMessageId &msgId)
{
    return new GxsChatPostItem(NULL, 0, grpId, msgId, true, true);
}

void GxsChatDialog::toggleAutoDownload()
{
    RsGxsGroupId grpId = groupId();
    if (grpId.isNull()) {
        return;
    }

    bool autoDownload ;

        if(!rsGxsChats->getChannelAutoDownload(grpId,autoDownload) || !rsGxsChats->setChannelAutoDownload(grpId, !autoDownload))
    {
        std::cerr << "GxsChatDialog::toggleAutoDownload() Auto Download failed to set";
        std::cerr << std::endl;
    }
}
//use RsGxsChatGroup for unseenp2p
void GxsChatDialog::loadGroupSummaryToken2(const uint32_t &token, std::list<RsGxsChatGroup> &groupInfo, RsUserdata *&userdata)
{
    std::vector<RsGxsChatGroup> groups;
    rsGxsChats->getGroupData(token, groups);

    /* Save groups to fill icons and description */
    GxsChatGroupInfoData *gxschatData = new GxsChatGroupInfoData;
    userdata = gxschatData;

    std::vector<RsGxsChatGroup>::iterator groupIt;
    for (groupIt = groups.begin(); groupIt != groups.end(); ++groupIt) {
        RsGxsChatGroup &group = *groupIt;
        groupInfo.push_back(group);

        if (group.mImage.mData != NULL) {
            QPixmap image;
            image.loadFromData(group.mImage.mData, group.mImage.mSize, "PNG");
            gxschatData->mIcon[group.mMeta.mGroupId] = image;
        }

        if (!group.mDescription.empty()) {
            gxschatData->mDescription[group.mMeta.mGroupId] = QString::fromUtf8(group.mDescription.c_str());
        }
    }
}

void GxsChatDialog::loadGroupSummaryToken(const uint32_t &token, std::list<RsGroupMetaData> &groupInfo, RsUserdata *&userdata)

{
    std::vector<RsGxsChatGroup> groups;
    rsGxsChats->getGroupData(token, groups);

    /* Save groups to fill icons and description */
    GxsChatGroupInfoData *gxschatData = new GxsChatGroupInfoData;
    userdata = gxschatData;

    std::vector<RsGxsChatGroup>::iterator groupIt;
    for (groupIt = groups.begin(); groupIt != groups.end(); ++groupIt) {
        RsGxsChatGroup &group = *groupIt;
        groupInfo.push_back(group.mMeta);

        if (group.mImage.mData != NULL) {
            QPixmap image;
            image.loadFromData(group.mImage.mData, group.mImage.mSize, "PNG");
            gxschatData->mIcon[group.mMeta.mGroupId] = image;
        }

        if (!group.mDescription.empty()) {
            gxschatData->mDescription[group.mMeta.mGroupId] = QString::fromUtf8(group.mDescription.c_str());
        }
    }
}

void GxsChatDialog::groupInfoToGroupItemInfo(const RsGroupMetaData &groupInfo, GroupItemInfo &groupItemInfo, const RsUserdata *userdata)
{
    UnseenGxsGroupFrameDialog::groupInfoToGroupItemInfo(groupInfo, groupItemInfo, userdata);

    const GxsChatGroupInfoData *gxschatData = dynamic_cast<const GxsChatGroupInfoData*>(userdata);
    if (!gxschatData) {
        std::cerr << "GxsChatDialog::groupInfoToGroupItemInfo() Failed to cast data to GxsChatGroupInfoData";
        std::cerr << std::endl;
        return;
    }

    QMap<RsGxsGroupId, QString>::const_iterator descriptionIt = gxschatData->mDescription.find(groupInfo.mGroupId);
    if (descriptionIt != gxschatData->mDescription.end()) {
        groupItemInfo.description = descriptionIt.value();
    }

    QMap<RsGxsGroupId, QIcon>::const_iterator iconIt = gxschatData->mIcon.find(groupInfo.mGroupId);
    if (iconIt != gxschatData->mIcon.end()) {
        groupItemInfo.icon = iconIt.value();
    }
}

TurtleRequestId GxsChatDialog::distantSearch(const QString& search_string)
{
    return rsGxsChats->turtleSearchRequest(search_string.toStdString()) ;
}

bool GxsChatDialog::getDistantSearchResults(TurtleRequestId id, std::map<RsGxsGroupId,RsGxsGroupSummary>& group_infos)
{
    return rsGxsChats->retrieveDistantSearchResults(id,group_infos);
}

void GxsChatDialog::checkRequestGroup(const RsGxsGroupId& grpId)
{
    RsGxsChatGroup distant_group;

    if( rsGxsChats->retrieveDistantGroup(grpId,distant_group)) // normally we should also check that the group meta is not already here.
    {
        std::cerr << "GxsChatDialog::checkRequestGroup() sending turtle request for group data for group " << grpId << std::endl;
        rsGxsChats->turtleGroupRequest(grpId);
    }
}
