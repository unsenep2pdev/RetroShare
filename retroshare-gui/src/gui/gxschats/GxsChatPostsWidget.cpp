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


#include <QDateTime>
#include <QSignalMapper>

#include "retroshare/rsgxscircles.h"
#include "ui_GxsChatPostsWidget.h"
#include "GxsChatPostsWidget.h"
#include "gui/feeds/GxsChatPostItem.h"
#include "gui/gxschats/CreateGxsChatMsg.h"
#include "gui/common/UIStateHelper.h"
#include "gui/settings/rsharesettings.h"
#include "gui/feeds/SubFileItem.h"
#include "gui/notifyqt.h"
#include <algorithm>
#include "util/DateTime.h"

#define CHAN_DEFAULT_IMAGE ":/images/channels.png"

#define ROLE_PUBLISH FEED_TREEWIDGET_SORTROLE


//#define DEBUG_CHAT


/* View mode */
#define VIEW_MODE_FEEDS  1
#define VIEW_MODE_FILES  2

/** Constructor */
GxsChatPostsWidget::GxsChatPostsWidget(const RsGxsGroupId &channelId, QWidget *parent) :
    GxsMessageFramePostWidget(rsGxsChats, parent),
    ui(new Ui::GxsChatPostsWidget)
{
    /* Invoke the Qt Designer generated object setup routine */
    ui->setupUi(this);


    /* Setup UI helper */

    mStateHelper->addWidget(mTokenTypeAllPosts, ui->progressBar, UISTATE_LOADING_VISIBLE);
    mStateHelper->addWidget(mTokenTypeAllPosts, ui->loadingLabel, UISTATE_LOADING_VISIBLE);
    mStateHelper->addWidget(mTokenTypeAllPosts, ui->filterLineEdit);

    mStateHelper->addWidget(mTokenTypePosts, ui->loadingLabel, UISTATE_LOADING_VISIBLE);

    mStateHelper->addLoadPlaceholder(mTokenTypeGroupData, ui->nameLabel);

    mStateHelper->addWidget(mTokenTypeGroupData, ui->postButton);
    mStateHelper->addWidget(mTokenTypeGroupData, ui->logoLabel);
    mStateHelper->addWidget(mTokenTypeGroupData, ui->subscribeToolButton);

    /* Connect signals */
    connect(ui->postButton, SIGNAL(clicked()), this, SLOT(createMsg()));
    connect(ui->subscribeToolButton, SIGNAL(subscribe(bool)), this, SLOT(subscribeGroup(bool)));
    connect(NotifyQt::getInstance(), SIGNAL(settingsChanged()), this, SLOT(settingsChanged()));

    ui->postButton->setText(tr("Add new post"));

    /* add filter actions */
    ui->filterLineEdit->addFilter(QIcon(), tr("Title"), FILTER_TITLE, tr("Search Title"));
    ui->filterLineEdit->addFilter(QIcon(), tr("Message"), FILTER_MSG, tr("Search Message"));
    ui->filterLineEdit->addFilter(QIcon(), tr("Filename"), FILTER_FILE_NAME, tr("Search Filename"));
    connect(ui->filterLineEdit, SIGNAL(textChanged(QString)), ui->feedWidget, SLOT(setFilterText(QString)));
    connect(ui->filterLineEdit, SIGNAL(textChanged(QString)), ui->fileWidget, SLOT(setFilterText(QString)));
    connect(ui->filterLineEdit, SIGNAL(filterChanged(int)), this, SLOT(filterChanged(int)));

    /* Initialize view button */
    //setViewMode(VIEW_MODE_FEEDS); see processSettings
    ui->infoWidget->hide();

    QSignalMapper *signalMapper = new QSignalMapper(this);
    connect(ui->feedToolButton, SIGNAL(clicked()), signalMapper, SLOT(map()));
    connect(ui->fileToolButton, SIGNAL(clicked()), signalMapper, SLOT(map()));
    signalMapper->setMapping(ui->feedToolButton, VIEW_MODE_FEEDS);
    signalMapper->setMapping(ui->fileToolButton, VIEW_MODE_FILES);
    connect(signalMapper, SIGNAL(mapped(int)), this, SLOT(setViewMode(int)));

    /*************** Setup Left Hand Side (List of Channels) ****************/

    ui->loadingLabel->hide();
    ui->progressBar->hide();

    ui->nameLabel->setMinimumWidth(20);

    /* Initialize feed widget */
    ui->feedWidget->setSortRole(ROLE_PUBLISH, Qt::DescendingOrder);
    ui->feedWidget->setFilterCallback(filterItem);


    /* load settings */
    processSettings(true);

    /* Initialize subscribe button */
    QIcon icon;
    icon.addPixmap(QPixmap(":/images/redled.png"), QIcon::Normal, QIcon::On);
    icon.addPixmap(QPixmap(":/images/start.png"), QIcon::Normal, QIcon::Off);
    mAutoDownloadAction = new QAction(icon, "", this);
    mAutoDownloadAction->setCheckable(true);
    connect(mAutoDownloadAction, SIGNAL(triggered()), this, SLOT(toggleAutoDownload()));

    ui->subscribeToolButton->addSubscribedAction(mAutoDownloadAction);

    /* Initialize GUI */
    setAutoDownload(true);
    settingsChanged();
    setGroupId(channelId);
}

GxsChatPostsWidget::~GxsChatPostsWidget()
{
    // save settings
    processSettings(false);

    delete(mAutoDownloadAction);

    delete ui;
}

void GxsChatPostsWidget::processSettings(bool load)
{
    Settings->beginGroup(QString("ChatPostsWidget"));

    if (load) {
        // load settings

        /* Filter */
        ui->filterLineEdit->setCurrentFilter(Settings->value("filter", FILTER_TITLE).toInt());

        /* View mode */
        setViewMode(Settings->value("viewMode", VIEW_MODE_FEEDS).toInt());
    } else {
        // save settings

        /* Filter */
        Settings->setValue("filter", ui->filterLineEdit->currentFilter());

        /* View mode */
        Settings->setValue("viewMode", viewMode());
    }

    Settings->endGroup();
}

void GxsChatPostsWidget::settingsChanged()
{
    mUseThread = Settings->getChatsLoadThread();

    mStateHelper->setWidgetVisible(ui->progressBar, mUseThread);
}

void GxsChatPostsWidget::groupNameChanged(const QString &name)
{
    if (groupId().isNull()) {
        ui->nameLabel->setText(tr("No Conversation Selected"));
        ui->logoLabel->setPixmap(QPixmap(":/images/channels.png"));
    } else {
        ui->nameLabel->setText(name);
    }
}

QIcon GxsChatPostsWidget::groupIcon()
{
    if (mStateHelper->isLoading(mTokenTypeGroupData) || mStateHelper->isLoading(mTokenTypeAllPosts)) {
        return QIcon(":/images/kalarm.png");
    }

//	if (mNewCount) {
//		return QIcon(":/images/message-state-new.png");
//	}

    return QIcon();
}

/*************************************************************************************/
/*************************************************************************************/
/*************************************************************************************/

QScrollArea *GxsChatPostsWidget::getScrollArea()
{
    return NULL;
}

void GxsChatPostsWidget::deleteFeedItem(QWidget * /*item*/, uint32_t /*type*/)
{
}

void GxsChatPostsWidget::openChat(const RsPeerId & /*peerId*/)
{
}

//// Callback from Widget->FeedHolder->ServiceDialog->CommentContainer->CommentDialog,
void GxsChatPostsWidget::openComments(uint32_t /*type*/, const RsGxsGroupId &groupId, const QVector<RsGxsMessageId>& msg_versions,const RsGxsMessageId &msgId, const QString &title)
{
    emit loadComment(groupId, msg_versions,msgId, title);
}

void GxsChatPostsWidget::createMsg()
{
    if (groupId().isNull()) {
        return;
    }

    if (!IS_GROUP_SUBSCRIBED(subscribeFlags())) {
        return;
    }

    CreateGxsChatMsg *msgDialog = new CreateGxsChatMsg(groupId());
    msgDialog->show();

    /* window will destroy itself! */
}

void GxsChatPostsWidget::insertChannelDetails(const RsGxsChatGroup &group)
{
    /* IMAGE */
    QPixmap chanImage;
    if (group.mImage.mData != NULL) {
        chanImage.loadFromData(group.mImage.mData, group.mImage.mSize, "PNG");
    } else {
        chanImage = QPixmap(CHAN_DEFAULT_IMAGE);
    }
    ui->logoLabel->setPixmap(chanImage);

    ui->subscribersLabel->setText(QString::number(group.mMeta.mPop)) ;

    if (group.mMeta.mSubscribeFlags & GXS_SERV::GROUP_SUBSCRIBE_PUBLISH)
    {
        mStateHelper->setWidgetEnabled(ui->postButton, true);
    }
    else
    {
        mStateHelper->setWidgetEnabled(ui->postButton, false);
    }

    ui->subscribeToolButton->setSubscribed(IS_GROUP_SUBSCRIBED(group.mMeta.mSubscribeFlags));

    bool autoDownload ;
            rsGxsChats->getChannelAutoDownload(group.mMeta.mGroupId,autoDownload);

    setAutoDownload(true);

    if (IS_GROUP_SUBSCRIBED(group.mMeta.mSubscribeFlags)) {
        ui->feedToolButton->setEnabled(true);

        ui->fileToolButton->setEnabled(true);
        ui->infoWidget->hide();
        setViewMode(viewMode());

        ui->infoPosts->clear();
        ui->infoDescription->clear();
    } else {
        ui->infoPosts->setText(QString::number(group.mMeta.mVisibleMsgCount));
        if(group.mMeta.mLastPost==0)
            ui->infoLastPost->setText(tr("Never"));
        else
            ui->infoLastPost->setText(DateTime::formatLongDateTime(group.mMeta.mLastPost));
        ui->infoDescription->setText(QString::fromUtf8(group.mDescription.c_str()));

            ui->infoAdministrator->setId(group.mMeta.mAuthorId) ;

            QString distrib_string ( "[unknown]" );

            switch(group.mMeta.mCircleType)
        {
        case GXS_CIRCLE_TYPE_PUBLIC: distrib_string = tr("Public") ;
            break ;
        case GXS_CIRCLE_TYPE_EXTERNAL:
        {
            RsGxsCircleDetails det ;

            // !! What we need here is some sort of CircleLabel, which loads the circle and updates the label when done.

            if(rsGxsCircles->getCircleDetails(group.mMeta.mCircleId,det))
                distrib_string = tr("Restricted to members of circle \"")+QString::fromUtf8(det.mCircleName.c_str()) +"\"";
            else
                distrib_string = tr("Restricted to members of circle ")+QString::fromStdString(group.mMeta.mCircleId.toStdString()) ;
        }
            break ;
        case GXS_CIRCLE_TYPE_YOUR_EYES_ONLY: distrib_string = tr("Your eyes only");
            break ;
        case GXS_CIRCLE_TYPE_LOCAL: distrib_string = tr("You and your friend nodes");
            break ;
        default:
            std::cerr << "(EE) badly initialised group distribution ID = " << group.mMeta.mCircleType << std::endl;
        }

        ui->infoDistribution->setText(distrib_string);

        ui->infoWidget->show();
        ui->feedWidget->hide();
        ui->fileWidget->hide();

        ui->feedToolButton->setEnabled(false);
        ui->fileToolButton->setEnabled(false);
    }
}

int GxsChatPostsWidget::viewMode()
{
    if (ui->feedToolButton->isChecked()) {
        return VIEW_MODE_FEEDS;
    } else if (ui->fileToolButton->isChecked()) {
        return VIEW_MODE_FILES;
    }

    /* Default */
    return VIEW_MODE_FEEDS;
}

void GxsChatPostsWidget::setViewMode(int viewMode)
{
    switch (viewMode) {
    case VIEW_MODE_FEEDS:
        ui->feedWidget->show();
        ui->fileWidget->hide();

        ui->feedToolButton->setChecked(true);
        ui->fileToolButton->setChecked(false);

        break;
    case VIEW_MODE_FILES:
        ui->feedWidget->hide();
        ui->fileWidget->show();

        ui->feedToolButton->setChecked(false);
        ui->fileToolButton->setChecked(true);

        break;
    default:
        setViewMode(VIEW_MODE_FEEDS);
        return;
    }
}

void GxsChatPostsWidget::filterChanged(int filter)
{
    ui->feedWidget->setFilterType(filter);
    ui->fileWidget->setFilterType(filter);
}

/*static*/ bool GxsChatPostsWidget::filterItem(FeedItem *feedItem, const QString &text, int filter)
{
    GxsChatPostItem *item = dynamic_cast<GxsChatPostItem*>(feedItem);
    if (!item) {
        return true;
    }

    bool bVisible = text.isEmpty();

    if (!bVisible)
    {
        switch(filter)
        {
            case FILTER_TITLE:
                bVisible = item->getTitleLabel().contains(text,Qt::CaseInsensitive);
            break;
            case FILTER_MSG:
                bVisible = item->getMsgLabel().contains(text,Qt::CaseInsensitive);
            break;
            case FILTER_FILE_NAME:
            {
                std::list<SubFileItem *> fileItems = item->getFileItems();
                std::list<SubFileItem *>::iterator lit;
                for(lit = fileItems.begin(); lit != fileItems.end(); ++lit)
                {
                    SubFileItem *fi = *lit;
                    QString fileName = QString::fromUtf8(fi->FileName().c_str());
                    bVisible = (bVisible || fileName.contains(text,Qt::CaseInsensitive));
                }
                break;
            }
            default:
                bVisible = true;
            break;
        }
    }

    return bVisible;
}

void GxsChatPostsWidget::createPostItem(const RsGxsChatMsg& post, bool related)
{
    GxsChatPostItem *item = NULL;

    if(!post.mMeta.mOrigMsgId.isNull())
    {
        FeedItem *feedItem = ui->feedWidget->findGxsFeedItem(post.mMeta.mGroupId, post.mMeta.mOrigMsgId);
        item = dynamic_cast<GxsChatPostItem*>(feedItem);

        if(item)
        {
            ui->feedWidget->removeFeedItem(item) ;

            GxsChatPostItem *item = new GxsChatPostItem(this, 0, post, true, false);
            ui->feedWidget->addFeedItem(item, ROLE_PUBLISH, QDateTime::fromTime_t(post.mMeta.mPublishTs));

            return ;
        }
    }

    if (related)
    {
        FeedItem *feedItem = ui->feedWidget->findGxsFeedItem(post.mMeta.mGroupId, post.mMeta.mMsgId);
        item = dynamic_cast<GxsChatPostItem*>(feedItem);
    }
    if (item) {
        item->setPost(post);
        ui->feedWidget->setSort(item, ROLE_PUBLISH, QDateTime::fromTime_t(post.mMeta.mPublishTs));
    } else {
        GxsChatPostItem *item = new GxsChatPostItem(this, 0, post, true, false);
        ui->feedWidget->addFeedItem(item, ROLE_PUBLISH, QDateTime::fromTime_t(post.mMeta.mPublishTs));
    }

    ui->fileWidget->addFiles(post, related);
}

void GxsChatPostsWidget::fillThreadCreatePost(const QVariant &post, bool related, int current, int count)
{
    /* show fill progress */
    if (count) {
        ui->progressBar->setValue(current * ui->progressBar->maximum() / count);
    }

    if (!post.canConvert<RsGxsChatMsg>()) {
        return;
    }

    createPostItem(post.value<RsGxsChatMsg>(), related);
}

void GxsChatPostsWidget::insertChannelPosts(std::vector<RsGxsChatMsg> &posts, GxsMessageFramePostThread *thread, bool related)
{
    if (related && thread) {
        std::cerr << "GxsChatPostsWidget::insertChannelPosts fill only related posts as thread is not possible" << std::endl;
        return;
    }

    int count = posts.size();
    int pos = 0;

    if (!thread) {
        ui->feedWidget->setSortingEnabled(false);
    }

    // collect new versions of posts if any

#ifdef DEBUG_CHAT
    std::cerr << "Inserting chat posts" << std::endl;
#endif

    std::vector<uint32_t> new_versions ;
    for (uint32_t i=0;i<posts.size();++i)
    {
        if(posts[i].mMeta.mOrigMsgId == posts[i].mMeta.mMsgId)
            posts[i].mMeta.mOrigMsgId.clear();

#ifdef DEBUG_CHAT
        std::cerr << "  " << i << ": msg_id=" << posts[i].mMeta.mMsgId << ": orig msg id = " << posts[i].mMeta.mOrigMsgId << std::endl;
#endif

        if(!posts[i].mMeta.mOrigMsgId.isNull())
            new_versions.push_back(i) ;
    }

#ifdef DEBUG_CHAT
    std::cerr << "New versions: " << new_versions.size() << std::endl;
#endif

    if(!new_versions.empty())
    {
#ifdef DEBUG_CHAT
        std::cerr << "  New versions present. Replacing them..." << std::endl;
        std::cerr << "  Creating search map."  << std::endl;
#endif

        // make a quick search map
        std::map<RsGxsMessageId,uint32_t> search_map ;
        for (uint32_t i=0;i<posts.size();++i)
            search_map[posts[i].mMeta.mMsgId] = i ;

        for(uint32_t i=0;i<new_versions.size();++i)
        {
#ifdef DEBUG_CHAT
            std::cerr << "  Taking care of new version  at index " << new_versions[i] << std::endl;
#endif

            uint32_t current_index = new_versions[i] ;
            uint32_t source_index  = new_versions[i] ;
#ifdef DEBUG_CHAT
            RsGxsMessageId source_msg_id = posts[source_index].mMeta.mMsgId ;
#endif

            // What we do is everytime we find a replacement post, we climb up the replacement graph until we find the original post
            // (or the most recent version of it). When we reach this post, we replace it with the data of the source post.
            // In the mean time, all other posts have their MsgId cleared, so that the posts are removed from the list.

            //std::vector<uint32_t> versions ;
            std::map<RsGxsMessageId,uint32_t>::const_iterator vit ;

            while(search_map.end() != (vit=search_map.find(posts[current_index].mMeta.mOrigMsgId)))
            {
#ifdef DEBUG_CHAT
                std::cerr << "    post at index " << current_index << " replaces a post at position " << vit->second ;
#endif

                // Now replace the post only if the new versionis more recent. It may happen indeed that the same post has been corrected multiple
                // times. In this case, we only need to replace the post with the newest version

                //uint32_t prev_index = current_index ;
                current_index = vit->second ;

                if(posts[current_index].mMeta.mMsgId.isNull())	// This handles the branching situation where this post has been already erased. No need to go down further.
                {
#ifdef DEBUG_CHAT
                    std::cerr << "  already erased. Stopping." << std::endl;
#endif
                    break ;
                }

                if(posts[current_index].mMeta.mPublishTs < posts[source_index].mMeta.mPublishTs)
                {
#ifdef DEBUG_CHAT
                    std::cerr << " and is more recent => following" << std::endl;
#endif
                    for(std::set<RsGxsMessageId>::const_iterator itt(posts[current_index].mOlderVersions.begin());itt!=posts[current_index].mOlderVersions.end();++itt)
                        posts[source_index].mOlderVersions.insert(*itt);

                    posts[source_index].mOlderVersions.insert(posts[current_index].mMeta.mMsgId);
                    posts[current_index].mMeta.mMsgId.clear();	    // clear the msg Id so the post will be ignored
                }
#ifdef DEBUG_CHAT
                else
                    std::cerr << " but is older -> Stopping" << std::endl;
#endif
            }
        }
    }

#ifdef DEBUG_CHAT
    std::cerr << "Now adding posts..." << std::endl;
#endif

    for (std::vector<RsGxsChatMsg>::const_reverse_iterator it = posts.rbegin(); it != posts.rend(); ++it)
    {
#ifdef DEBUG_CHAT
        std::cerr << "  adding post: " << (*it).mMeta.mMsgId ;
#endif

        if(!(*it).mMeta.mMsgId.isNull())
        {
#ifdef DEBUG_CHAT
            std::cerr << " added" << std::endl;
#endif

            if (thread && thread->stopped())
                break;

            if (thread)
                thread->emitAddPost(qVariantFromValue(*it), related, ++pos, count);
            else
                createPostItem(*it, related);
        }
#ifdef DEBUG_CHAT
        else
            std::cerr << " skipped" << std::endl;
#endif
    }

    if (!thread) {
        ui->feedWidget->setSortingEnabled(true);
    }
}

void GxsChatPostsWidget::clearPosts()
{
    ui->feedWidget->clear();
    ui->fileWidget->clear();
}

void GxsChatPostsWidget::blank()
{
    mStateHelper->setWidgetEnabled(ui->postButton, false);
    mStateHelper->setWidgetEnabled(ui->subscribeToolButton, false);

    clearPosts();

    groupNameChanged(QString());

    ui->infoWidget->hide();
    ui->feedWidget->show();
    ui->fileWidget->hide();
}

bool GxsChatPostsWidget::navigatePostItem(const RsGxsMessageId &msgId)
{
    FeedItem *feedItem = ui->feedWidget->findGxsFeedItem(groupId(), msgId);
    if (!feedItem) {
        return false;
    }

    return ui->feedWidget->scrollTo(feedItem, true);
}

void GxsChatPostsWidget::subscribeGroup(bool subscribe)
{
    if (groupId().isNull()) {
        return;
    }

    uint32_t token;
    rsGxsChats->subscribeToGroup(token, groupId(), subscribe);
//	mChannelQueue->queueRequest(token, 0, RS_TOKREQ_ANSTYPE_ACK, TOKEN_TYPE_SUBSCRIBE_CHANGE);
}

void GxsChatPostsWidget::setAutoDownload(bool autoDl)
{
    mAutoDownloadAction->setChecked(autoDl);
    mAutoDownloadAction->setText(autoDl ? tr("Disable Auto-Download") : tr("Enable Auto-Download"));
}

void GxsChatPostsWidget::toggleAutoDownload()
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

bool GxsChatPostsWidget::insertGroupData(const uint32_t &token, RsGroupMetaData &metaData)
{
    std::vector<RsGxsChatGroup> groups;
    rsGxsChats->getGroupData(token, groups);

    if(groups.size() == 1)
    {
        insertChannelDetails(groups[0]);
        metaData = groups[0].mMeta;
        return true;
    }
    else
    {
        RsGxsChatGroup distant_group;
        if(rsGxsChats->retrieveDistantGroup(groupId(),distant_group))
        {
            insertChannelDetails(distant_group);
            metaData = distant_group.mMeta;
            return true ;
        }
    }

    return false;
}

void GxsChatPostsWidget::insertAllPosts(const uint32_t &token, GxsMessageFramePostThread *thread)
{
    std::vector<RsGxsChatMsg> posts;
    rsGxsChats->getPostData(token, posts);

    insertChannelPosts(posts, thread, false);
}

void GxsChatPostsWidget::insertPosts(const uint32_t &token)
{
    std::vector<RsGxsChatMsg> posts;
    rsGxsChats->getPostData(token, posts);

    insertChannelPosts(posts, NULL, true);
}

class GxsChatPostsReadData
{
public:
    GxsChatPostsReadData(bool read)
    {
        mRead = read;
        mLastToken = 0;
    }

public:
    bool mRead;
    uint32_t mLastToken;
};

static void setAllMessagesReadCallback(FeedItem *feedItem, void *data)
{
    GxsChatPostItem *chatPostItem = dynamic_cast<GxsChatPostItem*>(feedItem);
    if (!chatPostItem) {
        return;
    }

    GxsChatPostsReadData *readData = (GxsChatPostsReadData*) data;
    bool is_not_new = !chatPostItem->isUnread() ;

    if(is_not_new == readData->mRead)
        return ;

    RsGxsGrpMsgIdPair msgPair = std::make_pair(chatPostItem->groupId(), chatPostItem->messageId());
    rsGxsChats->setMessageReadStatus(readData->mLastToken, msgPair, chatPostItem->messageId().toStdString(),  readData->mRead);
}

void GxsChatPostsWidget::setAllMessagesReadDo(bool read, uint32_t &token)
{
    if (groupId().isNull() || !IS_GROUP_SUBSCRIBED(subscribeFlags())) {
        return;
    }

    GxsChatPostsReadData data(read);
    ui->feedWidget->withAll(setAllMessagesReadCallback, &data);

    token = data.mLastToken;
}
