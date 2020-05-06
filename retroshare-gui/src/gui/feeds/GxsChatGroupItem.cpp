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

#include "GxsChatGroupItem.h"
#include "ui_GxsChatGroupItem.h"

#include "FeedHolder.h"
#include "gui/NewsFeed.h"
#include "gui/RetroShareLink.h"


#define DEBUG_ITEM 1


GxsChatGroupItem::GxsChatGroupItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsGroupId &groupId, bool isHome, bool autoUpdate) :
    GxsGroupFeedItem(feedHolder, feedId, groupId, isHome, rsGxsChats, autoUpdate)
{
    setup();

    requestGroup();
}

GxsChatGroupItem::GxsChatGroupItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsChatGroup &group, bool isHome, bool autoUpdate) :
    GxsGroupFeedItem(feedHolder, feedId, group.mMeta.mGroupId, isHome, rsGxsChats, autoUpdate)
{
    setup();

    setGroup(group);
}

GxsChatGroupItem::~GxsChatGroupItem()
{
    delete(ui);
}

void GxsChatGroupItem::setup()
{
    /* Invoke the Qt Designer generated object setup routine */
    ui = new(Ui::GxsChatGroupItem);
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose, true);

    /* clear ui */
    ui->nameLabel->setText(tr("Loading"));
    ui->titleLabel->clear();
    ui->descLabel->clear();

    /* general ones */
    connect(ui->expandButton, SIGNAL(clicked()), this, SLOT(toggle()));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(removeItem()));

    /* specific */
    connect(ui->subscribeButton, SIGNAL(clicked()), this, SLOT(subscribeChannel()));
    connect(ui->copyLinkButton, SIGNAL(clicked()), this, SLOT(copyGroupLink()));

    ui->expandFrame->hide();
}

bool GxsChatGroupItem::setGroup(const RsGxsChatGroup &group)
{
    if (groupId() != group.mMeta.mGroupId) {
        std::cerr << "GxsChatGroupItem::setContent() - Wrong id, cannot set post";
        std::cerr << std::endl;
        return false;
    }

    mGroup = group;
    fill();

    return true;
}

void GxsChatGroupItem::loadGroup(const uint32_t &token)
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatGroupItem::loadGroup()";
    std::cerr << std::endl;
#endif

    std::vector<RsGxsChatGroup> groups;
    if (!rsGxsChats->getGroupData(token, groups))
    {
        std::cerr << "GxsChatGroupItem::loadGroup() ERROR getting data";
        std::cerr << std::endl;
        return;
    }

    if (groups.size() != 1)
    {
        std::cerr << "GxsChatGroupItem::loadGroup() Wrong number of Items";
        std::cerr << std::endl;
        return;
    }

    setGroup(groups[0]);
}

QString GxsChatGroupItem::groupName()
{
    return QString::fromUtf8(mGroup.mMeta.mGroupName.c_str());
}

void GxsChatGroupItem::fill()
{
    /* fill in */

#ifdef DEBUG_ITEM
    std::cerr << "GxsChatGroupItem::fill()";
    std::cerr << std::endl;
#endif

    RetroShareLink link = RetroShareLink::createGxsGroupLink(RetroShareLink::TYPE_CHATS, mGroup.mMeta.mGroupId, groupName());
    ui->nameLabel->setText(link.toHtml());

    ui->descLabel->setText(QString::fromUtf8(mGroup.mDescription.c_str()));

    if (mGroup.mImage.mData != NULL) {
        QPixmap chanImage;
        chanImage.loadFromData(mGroup.mImage.mData, mGroup.mImage.mSize, "PNG");
        ui->logoLabel->setPixmap(QPixmap(chanImage));
    }

    if (IS_GROUP_SUBSCRIBED(mGroup.mMeta.mSubscribeFlags)) {
        ui->subscribeButton->setEnabled(false);
    } else {
        ui->subscribeButton->setEnabled(true);
    }

    switch(mFeedId)
    {
    case NEWSFEED_CHATPUBKEYLIST:	ui->titleLabel->setText(tr("Publish permission received for chat: "));
                                        break ;

    case NEWSFEED_CHATNEWLIST:	 	ui->titleLabel->setText(tr("New Chat Conversation: "));
                                        break ;
    }

    if (mIsHome)
    {
        /* disable buttons */
        ui->clearButton->setEnabled(false);
    }
}

void GxsChatGroupItem::toggle()
{
    expand(ui->expandFrame->isHidden());
}

void GxsChatGroupItem::doExpand(bool open)
{
    if (mFeedHolder)
    {
        mFeedHolder->lockLayout(this, true);
    }

    if (open)
    {
        ui->expandFrame->show();
        ui->expandButton->setIcon(QIcon(QString(":/images/edit_remove24.png")));
        ui->expandButton->setToolTip(tr("Hide"));
    }
    else
    {
        ui->expandFrame->hide();
        ui->expandButton->setIcon(QIcon(QString(":/images/edit_add24.png")));
        ui->expandButton->setToolTip(tr("Expand"));
    }

    emit sizeChanged(this);

    if (mFeedHolder)
    {
        mFeedHolder->lockLayout(this, false);
    }
}

void GxsChatGroupItem::subscribeChannel()
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatGroupItem::subscribeChannel()";
    std::cerr << std::endl;
#endif

    subscribe();
}
