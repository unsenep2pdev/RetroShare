/****************************************************************
 * This file is distributed under the following license:
 *
 * Copyright (c) 2010, RetroShare Team
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
#include <iostream>

#include "UnseenGroupTreeWidget.h"
#include "ui_UnseenGroupTreeWidget.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMovie>
#include <QToolButton>

#include "retroshare/rsgxsflags.h"

#include "PopularityDefs.h"
#include "RSElidedItemDelegate.h"
#include "RSTreeWidgetItem.h"
#include "gui/common/ElidedLabel.h"
#include "gui/settings/rsharesettings.h"
#include "util/QtVersion.h"
#include "util/DateTime.h"

//unseenp2p - using SmartListView and SmartListModel
#include "gui/UnseenGxsConversationitemdelegate.h"

#include <stdint.h>

#define COLUMN_NAME        0
#define COLUMN_UNREAD      1
#define COLUMN_POPULARITY  2
#define COLUMN_LAST_POST   3
#define COLUMN_COUNT       4
#define COLUMN_DATA        COLUMN_NAME

#define ROLE_ID              Qt::UserRole
#define ROLE_NAME            Qt::UserRole + 1
#define ROLE_DESCRIPTION     Qt::UserRole + 2
#define ROLE_POPULARITY      Qt::UserRole + 3
#define ROLE_LASTPOST        Qt::UserRole + 4
#define ROLE_POSTS           Qt::UserRole + 5
#define ROLE_UNREAD          Qt::UserRole + 6
#define ROLE_SEARCH_SCORE    Qt::UserRole + 7
#define ROLE_SUBSCRIBE_FLAGS Qt::UserRole + 8
#define ROLE_COLOR           Qt::UserRole + 9
#define ROLE_SAVED_ICON      Qt::UserRole + 10
#define ROLE_SEARCH_STRING   Qt::UserRole + 11
#define ROLE_REQUEST_ID      Qt::UserRole + 12

#define FILTER_NAME_INDEX  0
#define FILTER_DESC_INDEX  1

Q_DECLARE_METATYPE(ElidedLabel*)
Q_DECLARE_METATYPE(QLabel*)

UnseenGroupTreeWidget::UnseenGroupTreeWidget(QWidget *parent) :
        QWidget(parent), ui(new Ui::UnseenGroupTreeWidget)
{
	ui->setupUi(this);

	displayMenu = NULL;
	actionSortAscending = NULL;
//	actionSortDescending = NULL;
	actionSortByName = NULL;
	actionSortByPopularity = NULL;
	actionSortByLastPost = NULL;
	actionSortByPosts = NULL;
	actionSortByUnread = NULL;

	compareRole = new RSTreeWidgetItemCompareRole;
	compareRole->setRole(COLUMN_DATA, ROLE_NAME);

	/* Connect signals */
	connect(ui->filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged()));
	connect(ui->filterLineEdit, SIGNAL(filterChanged(int)), this, SLOT(filterChanged()));

    connect(ui->gxsGroupChatTreeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customContextMenuRequested(QPoint)));
   // connect(ui->gxsGroupChatTreeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), this, SLOT(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
   // connect(ui->gxsGroupChatTreeWidget, SIGNAL(itemActivated(QTreeWidgetItem*,int)), this, SLOT(itemActivated(QTreeWidgetItem*,int)));
//	if (!style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, NULL, this)) {
//		// need signal itemClicked too
//		connect(ui->gxsGroupChatTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(itemActivated(QTreeWidgetItem*,int)));
//	}


	int S = QFontMetricsF(font()).height() ;
	int W = QFontMetricsF(font()).width("999") ;
	int D = QFontMetricsF(font()).width("9999-99-99[]") ;


	/* add filter actions */
    ui->filterLineEdit->setPlaceholderText("Search ");
    ui->filterLineEdit->showFilterIcon();


    //MVC GUI for Gxs GroupChat
    if (!ui->gxsGroupChatTreeWidget->model()) {
        smartListModel_ = new UnseenGxsSmartListModel("testing", this);
        ui->gxsGroupChatTreeWidget->setModel(smartListModel_);
        ui->gxsGroupChatTreeWidget->setItemDelegate(new UnseenGxsConversationItemDelegate());
        ui->gxsGroupChatTreeWidget->show();
    }

    // smartlist selection
    QObject::connect(ui->gxsGroupChatTreeWidget->selectionModel(),
        SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
        this,
        SLOT(smartListSelectionChanged(QItemSelection, QItemSelection)));

    ui->gxsGroupChatTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu) ;
    ui->gxsGroupChatTreeWidget->header()->hide();

	/* Initialize display button */
	initDisplayMenu(ui->displayButton);

    sort();
}

UnseenGroupTreeWidget::~UnseenGroupTreeWidget()
{
	delete ui;
}

static void getNameWidget(QTreeWidget *treeWidget, QTreeWidgetItem *item, ElidedLabel *&nameLabel, QLabel *&waitLabel)
{
	QWidget *widget = treeWidget->itemWidget(item, COLUMN_NAME);

	if (!widget) {
		widget = new QWidget;
		widget->setAttribute(Qt::WA_TranslucentBackground);
		nameLabel = new ElidedLabel(widget);
		waitLabel = new QLabel(widget);
		QMovie *movie = new QMovie(":/images/loader/circleball-16.gif");
		waitLabel->setMovie(movie);
		waitLabel->setHidden(true);

		widget->setProperty("nameLabel", qVariantFromValue(nameLabel));
		widget->setProperty("waitLabel", qVariantFromValue(waitLabel));

		QHBoxLayout *layout = new QHBoxLayout;
		layout->setSpacing(0);
		layout->setContentsMargins(0, 0, 0, 0);

		layout->addWidget(nameLabel);
		layout->addWidget(waitLabel);

		widget->setLayout(layout);

		treeWidget->setItemWidget(item, COLUMN_NAME, widget);
	} else {
		nameLabel = widget->property("nameLabel").value<ElidedLabel*>();
		waitLabel = widget->property("waitLabel").value<QLabel*>();
	}
}

void UnseenGroupTreeWidget::changeEvent(QEvent *e)
{
	QWidget::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	case QEvent::StyleChange:
		updateColors();
		break;
	default:
		// remove compiler warnings
		break;
	}
}

void UnseenGroupTreeWidget::addToolButton(QToolButton *toolButton)
{
	if (!toolButton) {
		return;
	}

	/* Initialize button */
	toolButton->setAutoRaise(true);
	toolButton->setIconSize(ui->displayButton->iconSize());
	toolButton->setFocusPolicy(ui->displayButton->focusPolicy());

	ui->titleBarFrame->layout()->addWidget(toolButton);
}

// Load and save settings (group must be started from the caller)
void UnseenGroupTreeWidget::processSettings(bool load)
{
	if (Settings == NULL) {
		return;
	}

	const int SORTBY_NAME = 1;
	const int SORTBY_POPULRITY = 2;
	const int SORTBY_LASTPOST = 3;
	const int SORTBY_POSTS = 4;
	const int SORTBY_UNREAD = 5;

    //ui->treeWidget->processSettings(load);

	if (load) {
		// load Settings

		// state of order
		bool ascSort = Settings->value("GroupAscSort", true).toBool();
		actionSortAscending->setChecked(ascSort);
		actionSortDescending->setChecked(!ascSort);

		// state of sort
		int sortby = Settings->value("GroupSortBy").toInt();
		switch (sortby) {
		case SORTBY_NAME:
			if (actionSortByName) {
				actionSortByName->setChecked(true);
			}
			break;
		case SORTBY_POPULRITY:
			if (actionSortByPopularity) {
				actionSortByPopularity->setChecked(true);
			}
			break;
		case SORTBY_LASTPOST:
			if (actionSortByLastPost) {
				actionSortByLastPost->setChecked(true);
			}
			break;
		case SORTBY_POSTS:
			if (actionSortByPosts) {
				actionSortByPosts->setChecked(true);
			}
			break;
		case SORTBY_UNREAD:
			if (actionSortByUnread) {
				actionSortByUnread->setChecked(true);
			}
			break;
		}
	} else {
		// save Settings

		// state of order
		Settings->setValue("GroupAscSort", !(actionSortDescending && actionSortDescending->isChecked())); //True by default

		// state of sort
		int sortby = SORTBY_NAME;
		if (actionSortByName && actionSortByName->isChecked()) {
			sortby = SORTBY_NAME;
		} else if (actionSortByPopularity && actionSortByPopularity->isChecked()) {
			sortby = SORTBY_POPULRITY;
		} else if (actionSortByLastPost && actionSortByLastPost->isChecked()) {
			sortby = SORTBY_LASTPOST;
		} else if (actionSortByPosts && actionSortByPosts->isChecked()) {
			sortby = SORTBY_POSTS;
		} else if (actionSortByUnread && actionSortByUnread->isChecked()) {
			sortby = SORTBY_UNREAD;
		}
		Settings->setValue("GroupSortBy", sortby);
	}
}

void UnseenGroupTreeWidget::initDisplayMenu(QToolButton *toolButton)
{
	displayMenu = new QMenu();
	QActionGroup *actionGroupAsc = new QActionGroup(displayMenu);

	actionSortDescending = displayMenu->addAction(QIcon(":/images/sort_decrease.png"), tr("Sort Descending Order"), this, SLOT(sort()));
	actionSortDescending->setCheckable(true);
	actionSortDescending->setActionGroup(actionGroupAsc);

	actionSortAscending = displayMenu->addAction(QIcon(":/images/sort_incr.png"), tr("Sort Ascending Order"), this, SLOT(sort()));
	actionSortAscending->setCheckable(true);
	actionSortAscending->setActionGroup(actionGroupAsc);

	displayMenu->addSeparator();

	QActionGroup *actionGroup = new QActionGroup(displayMenu);
	actionSortByName = displayMenu->addAction(QIcon(), tr("Sort by Name"), this, SLOT(sort()));
	actionSortByName->setCheckable(true);
	actionSortByName->setChecked(true); // set standard to sort by name
	actionSortByName->setActionGroup(actionGroup);

	actionSortByPopularity = displayMenu->addAction(QIcon(), tr("Sort by Popularity"), this, SLOT(sort()));
	actionSortByPopularity->setCheckable(true);
	actionSortByPopularity->setActionGroup(actionGroup);

	actionSortByLastPost = displayMenu->addAction(QIcon(), tr("Sort by Last Post"), this, SLOT(sort()));
	actionSortByLastPost->setCheckable(true);
	actionSortByLastPost->setActionGroup(actionGroup);

	actionSortByPosts = displayMenu->addAction(QIcon(), tr("Sort by Number of Posts"), this, SLOT(sort()));
	actionSortByPosts->setCheckable(true);
	actionSortByPosts->setActionGroup(actionGroup);

	actionSortByUnread = displayMenu->addAction(QIcon(), tr("Sort by Unread"), this, SLOT(sort()));
	actionSortByUnread->setCheckable(true);
	actionSortByUnread->setActionGroup(actionGroup);

	toolButton->setMenu(displayMenu);
}

void UnseenGroupTreeWidget::updateColors()
{
//	QBrush brush;
//	QBrush standardBrush = ui->treeWidget->palette().color(QPalette::Text);

//	QTreeWidgetItemIterator itemIterator(ui->treeWidget);
//	QTreeWidgetItem *item;
//	while ((item = *itemIterator) != NULL) {
//		++itemIterator;

//		int color = item->data(COLUMN_DATA, ROLE_COLOR).toInt();
//		if (color >= 0) {
//			brush = QBrush(mTextColor[color]);
//		} else {
//			brush = standardBrush;
//		}

//		item->setForeground(COLUMN_NAME, brush);

//		ElidedLabel *nameLabel = NULL;
//		QLabel *waitLabel = NULL;
//		getNameWidget(ui->treeWidget, item, nameLabel, waitLabel);
//		nameLabel->setTextColor(brush.color());
//	}
}

void UnseenGroupTreeWidget::customContextMenuRequested(const QPoint &pos)
{

	emit treeCustomContextMenuRequested(pos);
}

void UnseenGroupTreeWidget::currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous);

	QString id;

	if (current) {
		id = current->data(COLUMN_DATA, ROLE_ID).toString();
	}

	emit treeCurrentItemChanged(id);
}

void UnseenGroupTreeWidget::itemActivated(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);

	QString id;

	if (item) {
		id = item->data(COLUMN_DATA, ROLE_ID).toString();
	}

	emit treeItemActivated(id);
}

//this uses only for add 4 categories of channels: your channel, subscribed channel, popular channel and other channel
// No need for UnseenP2P
//QTreeWidgetItem *UnseenGroupTreeWidget::addCategoryItem(const QString &name, const QIcon &icon, bool expand)
//{
//	QFont font;
//	QTreeWidgetItem *item = new QTreeWidgetItem();
//	ui->treeWidget->addTopLevelItem(item);

//	ElidedLabel *nameLabel = NULL;
//	QLabel *waitLabel = NULL;
//	getNameWidget(ui->treeWidget, item, nameLabel, waitLabel);

//	nameLabel->setText(name);
//	item->setData(COLUMN_DATA, ROLE_NAME, name);
//	font = item->font(COLUMN_NAME);
//	font.setBold(true);
//	item->setFont(COLUMN_NAME, font);
//	nameLabel->setFont(font);
//	item->setIcon(COLUMN_NAME, icon);

//	int S = QFontMetricsF(font).height();

//	item->setSizeHint(COLUMN_NAME, QSize(S*1.1, S*1.1));
//	item->setForeground(COLUMN_NAME, QBrush(textColorCategory()));
//	nameLabel->setTextColor(textColorCategory());
//	item->setData(COLUMN_DATA, ROLE_COLOR, GROUPTREEWIDGET_COLOR_CATEGORY);

//	item->setExpanded(expand);

//	return item;
//}

//void UnseenGroupTreeWidget::removeSearchItem(QTreeWidgetItem *item)
//{
//   // ui->treeWidget->takeTopLevelItem(ui->treeWidget->indexOfTopLevelItem(item)) ;
//}

//QTreeWidgetItem *UnseenGroupTreeWidget::addSearchItem(const QString& search_string, uint32_t id, const QIcon& icon)
//{
//    QTreeWidgetItem *item = addCategoryItem(search_string,icon,true);

//    item->setData(COLUMN_DATA,ROLE_SEARCH_STRING,search_string) ;
//    item->setData(COLUMN_DATA,ROLE_REQUEST_ID   ,id) ;

//    return item;
//}


//bool UnseenGroupTreeWidget::isSearchRequestResult(QPoint &point,QString& group_id,uint32_t& search_req_id)
//{
//    QTreeWidgetItem *item = ui->treeWidget->itemAt(point);
//	if (item == NULL)
//		return false;

//    QTreeWidgetItem *parent = item->parent();

//    if(parent == NULL)
//        return false ;

//	search_req_id = parent->data(COLUMN_DATA, ROLE_REQUEST_ID).toUInt();
//    group_id = itemId(item) ;

//    return search_req_id > 0;
//}

//bool UnseenGroupTreeWidget::isSearchRequestItem(QPoint &point,uint32_t& search_req_id)
//{
//    QTreeWidgetItem *item = ui->treeWidget->itemAt(point);
//	if (item == NULL)
//		return false;

//	search_req_id = item->data(COLUMN_DATA, ROLE_REQUEST_ID).toUInt();

//    return search_req_id > 0;
//}

//QString UnseenGroupTreeWidget::itemId(QTreeWidgetItem *item)
//{
//	if (item == NULL) {
//		return "";
//	}

//	return item->data(COLUMN_DATA, ROLE_ID).toString();
//}

QString UnseenGroupTreeWidget::itemIdAt(QPoint &point)
{
    QModelIndex itemIndex = ui->gxsGroupChatTreeWidget->indexAt(point);

    std::vector<UnseenGroupItemInfo> iteminfoList = smartListModel_->getGxsGroupList();

    if (itemIndex.row() < iteminfoList.size())
    {
        UnseenGroupItemInfo itemInfo = iteminfoList.at(itemIndex.row());
        return  itemInfo.id;
    }
    else return "";

}

UnseenGroupItemInfo UnseenGroupTreeWidget::groupItemIdAt(QPoint &point)
{
    QModelIndex itemIndex = ui->gxsGroupChatTreeWidget->indexAt(point);

    std::vector<UnseenGroupItemInfo> iteminfoList = smartListModel_->getGxsGroupList();

    if (itemIndex.row() < iteminfoList.size())
    {
        return iteminfoList.at(itemIndex.row());
    }
    else return UnseenGroupItemInfo();

}

//void UnseenGroupTreeWidget::fillGroupItems(QTreeWidgetItem *categoryItem, const QList<GroupItemInfo> &itemList)
//{
//    if (categoryItem == NULL) {
//        return;
//    }

//    QString filterText = ui->filterLineEdit->text();

//    /* Iterate all items */
//    QList<GroupItemInfo>::const_iterator it;
//    for (it = itemList.begin(); it != itemList.end(); ++it) {
//        const GroupItemInfo &itemInfo = *it;

//        QTreeWidgetItem *item = NULL;

//        /* Search exisiting item */
//        int childCount = categoryItem->childCount();
//        for (int child = 0; child < childCount; ++child) {
//            QTreeWidgetItem *childItem = categoryItem->child(child);
//            if (childItem->data(COLUMN_DATA, ROLE_ID).toString() == itemInfo.id) {
//                /* Found child */
//                item = childItem;
//                break;
//            }
//        }

//        if (item == NULL) {
//            item = new RSTreeWidgetItem(compareRole);
//            item->setData(COLUMN_DATA, ROLE_ID, itemInfo.id);
//            categoryItem->addChild(item);
//        }

//        ElidedLabel *nameLabel = NULL;
//        QLabel *waitLabel = NULL;
//        getNameWidget(ui->treeWidget, item, nameLabel, waitLabel);

//        nameLabel->setText(itemInfo.name);
//        item->setData(COLUMN_DATA, ROLE_NAME, itemInfo.name);
//        item->setData(COLUMN_DATA, ROLE_DESCRIPTION, itemInfo.description);

//        /* Set last post */
//        qlonglong lastPost = itemInfo.lastpost.toTime_t();
//        item->setData(COLUMN_DATA, ROLE_LASTPOST, -lastPost); // negative for correct sorting
//        if(itemInfo.lastpost == QDateTime::fromTime_t(0))
//            item->setText(COLUMN_LAST_POST, tr("Never"));
//        else
//            item->setText(COLUMN_LAST_POST, itemInfo.lastpost.toString(Qt::ISODate).replace("T"," "));


//        /* Set visible posts */
//        item->setData(COLUMN_DATA, ROLE_POSTS, -itemInfo.max_visible_posts);// negative for correct sorting

//        /* Set icon */
//        if (waitLabel->isVisible()) {
//            /* Item is waiting, save icon in role */
//            item->setData(COLUMN_DATA, ROLE_SAVED_ICON, itemInfo.icon);
//        } else {
//            item->setIcon(COLUMN_NAME, itemInfo.icon);
//        }

//        /* Set popularity */
//        QString tooltip = PopularityDefs::tooltip(itemInfo.popularity);

//        item->setIcon(COLUMN_POPULARITY, PopularityDefs::icon(itemInfo.popularity));
//        item->setData(COLUMN_DATA, ROLE_POPULARITY, -itemInfo.popularity); // negative for correct sorting

//        /* Set tooltip */
//        if (itemInfo.adminKey)
//            tooltip += "\n" + tr("You are admin (modify names and description using Edit menu)");
//        else if (itemInfo.publishKey)
//            tooltip += "\n" + tr("You have been granted as publisher (you can post here!)");

//        if(!IS_GROUP_SUBSCRIBED(itemInfo.subscribeFlags))
//            tooltip += "\n" + QString::number(itemInfo.max_visible_posts) + " messages available" ;
//        // if(itemInfo.max_visible_posts)  // wtf? this=0 when there are some posts definitely exist - lastpost is recent
//        if(itemInfo.lastpost == QDateTime::fromTime_t(0))
//            tooltip += "\n" + tr("Last Post") + ": "  + tr("Never") ;
//        else
//            tooltip += "\n" + tr("Last Post") + ": "  + DateTime::formatLongDateTime(itemInfo.lastpost) ;
//        if(!IS_GROUP_SUBSCRIBED(itemInfo.subscribeFlags))
//            tooltip += "\n" + tr("Subscribe to download and read messages") ;

//        tooltip += "\n" + tr("Description") + ": " + itemInfo.description;

//        item->setToolTip(COLUMN_NAME, tooltip);
//        item->setToolTip(COLUMN_UNREAD, tooltip);
//        item->setToolTip(COLUMN_POPULARITY, tooltip);

//        item->setData(COLUMN_DATA, ROLE_SUBSCRIBE_FLAGS, itemInfo.subscribeFlags);

//        /* Set color */
//        QBrush brush;
//        if (itemInfo.publishKey) {
//            brush = QBrush(textColorPrivateKey());
//            item->setData(COLUMN_DATA, ROLE_COLOR, GROUPTREEWIDGET_COLOR_PRIVATEKEY);
//        } else {
//            //brush = ui->treeWidget->palette().color(QPalette::Text);
//            item->setData(COLUMN_DATA, ROLE_COLOR, GROUPTREEWIDGET_COLOR_STANDARD);
//        }
//        item->setForeground(COLUMN_NAME, brush);
//        nameLabel->setTextColor(brush.color());

//        /* Calculate score */
//        calculateScore(item, filterText);
//    }

//    /* Remove all items not in list */
//    int child = 0;
//    int childCount = categoryItem->childCount();
//    while (child < childCount) {
//        QString id = categoryItem->child(child)->data(COLUMN_DATA, ROLE_ID).toString();

//        for (it = itemList.begin(); it != itemList.end(); ++it) {
//            if (it->id == id) {
//                break;
//            }
//        }

//        if (it == itemList.end()) {
//            delete(categoryItem->takeChild(child));
//            childCount = categoryItem->childCount();
//        } else {
//            ++child;
//        }
//    }

//    resort(categoryItem);
//}

//void UnseenGroupTreeWidget::setUnreadCount(QTreeWidgetItem *item, int unreadCount)
//{
//	if (item == NULL) {
//		return;
//	}
//	ElidedLabel *nameLabel = NULL;
//	QLabel *waitLabel = NULL;
//	getNameWidget(ui->treeWidget, item, nameLabel, waitLabel);

//	QFont font = nameLabel->font();

//	if (unreadCount) {
//		item->setData(COLUMN_DATA, ROLE_UNREAD, unreadCount);
//		item->setText(COLUMN_UNREAD, QString::number(unreadCount));
//		font.setBold(true);
//	} else {
//		item->setText(COLUMN_UNREAD, "");
//		font.setBold(false);
//	}

//	nameLabel->setFont(font);
//}

QTreeWidgetItem *UnseenGroupTreeWidget::getItemFromId(const QString &id)
{
    if (id.isEmpty()) {
        return NULL;
    }

//	/* Search exisiting item */
//	QTreeWidgetItemIterator itemIterator(ui->treeWidget);
//	QTreeWidgetItem *item;
//	while ((item = *itemIterator) != NULL) {
//		++itemIterator;

//		if (item->parent() == NULL) {
//			continue;
//		}
//		if (item->data(COLUMN_DATA, ROLE_ID).toString() == id) {
//			return item;
//		}
//	}
    return NULL ;
}

void UnseenGroupTreeWidget::updateAllGxsGroupList(std::vector<UnseenGroupItemInfo> allList)
{
    allGxsGroupList = allList;
    smartListModel_->setGxsGroupList(allList);
    emit ui->gxsGroupChatTreeWidget->model()->layoutChanged();
}

void UnseenGroupTreeWidget::sort()
{
    //resort(NULL);
    sortGxsConversationListByRecentTime();
}

void UnseenGroupTreeWidget::smartListSelectionChanged(const QItemSelection  &selected, const QItemSelection  &deselected)
{
    Q_UNUSED(deselected);
    QModelIndexList indices = selected.indexes();

    if (indices.isEmpty()) {
        return;
    }

    auto selectedIndex = indices.at(0);

    if (not selectedIndex.isValid()) {
        return;
    }

    selectConversation(selectedIndex);
}

void UnseenGroupTreeWidget::selectConversation(const QModelIndex& index)
{

    //How to get the ConversationModel and get the index of it
    if (!index.isValid()) return;



    std::vector<UnseenGroupItemInfo> list = smartListModel_->getGxsGroupList();

    if (list.size() <= index.row()) return;

    UnseenGroupItemInfo gxsGroupItem = list.at(index.row());

    std::cerr << " gxsGroupItem info, name : " << gxsGroupItem.name.toStdString() << std::endl;




}

//25 Oct 2019 - meiyousixin - using MVC for the chat item list
//void UnseenGroupTreeWidget::showContactChatMVC(std::string chatIdStr)
//{

//    if(_chatOne2One_infos.count(chatIdStr) > 0)
//    {
//        ui->stackedWidget->setCurrentWidget(_chatOne2One_infos[chatIdStr].dialog) ;
//    }
//    else
//    {
//        //create chat dialog and add into this widget
//        RsPeerId peerId(chatIdStr);
//        ChatId chatId(peerId);
//        ChatDialog::chatFriend(chatId);
//    }
//}

std::vector<UnseenGroupItemInfo> UnseenGroupTreeWidget::getAllGxsGroupList()
{
    return smartListModel_->getGxsGroupList();// allGxsGroupList;
}

void UnseenGroupTreeWidget::sortGxsConversationListByRecentTime()
{
    smartListModel_->sortGxsConversationListByRecentTime();
//    std::sort(allGxsGroupList.begin(), allGxsGroupList.end(),
//              [] (UnseenGroupItemInfo const& a, UnseenGroupItemInfo const& b)
    //    { return a.lastMsgDatetime > b.lastMsgDatetime; });
}

UnseenGxsSmartListView *UnseenGroupTreeWidget::getUnseenGxsListView()
{
    return ui->gxsGroupChatTreeWidget;
}
