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

#ifndef _UNSEENGROUPTREEWIDGET_H
#define _UNSEENGROUPTREEWIDGET_H

#include <QWidget>
#include <QIcon>
#include <QTreeWidgetItem>
#include <QDateTime>

//#include "GroupTreeWidget.h"
//unseenp2p
#include "../gui/gxschats/UnseenGxsSmartlistmodel.h"
//#include "../gui/gxschats/UnseenGxsSmartlistview.h"
#include <vector>


class QToolButton;
class RshareSettings;
class RSTreeWidgetItemCompareRole;
class RSTreeWidget;
class UnseenGxsSmartListView;


#define GROUPTREEWIDGET_COLOR_STANDARD   -1
#define GROUPTREEWIDGET_COLOR_CATEGORY   0
#define GROUPTREEWIDGET_COLOR_PRIVATEKEY 1
#define GROUPTREEWIDGET_COLOR_COUNT      2

namespace Ui {
    class UnseenGroupTreeWidget;
}



//cppcheck-suppress noConstructor
class UnseenGroupTreeWidget : public QWidget
{
	Q_OBJECT

	Q_PROPERTY(QColor textColorCategory READ textColorCategory WRITE setTextColorCategory)
	Q_PROPERTY(QColor textColorPrivateKey READ textColorPrivateKey WRITE setTextColorPrivateKey)

public:
    UnseenGroupTreeWidget(QWidget *parent = 0);
    ~UnseenGroupTreeWidget();

	// Add a tool button to the tool area
	void addToolButton(QToolButton *toolButton);

	// Load and save settings (group must be started from the caller)
	void processSettings(bool load);

	// Add a new category item
	QTreeWidgetItem *addCategoryItem(const QString &name, const QIcon &icon, bool expand);
    // Add a new search item
//    void setDistSearchVisible(bool) ; // shows/hides distant search UI parts.
//	QTreeWidgetItem *addSearchItem(const QString& search_string, uint32_t id, const QIcon &icon) ;
//	void removeSearchItem(QTreeWidgetItem *item);

	// Get id of item
//	QString itemId(QTreeWidgetItem *item);
    QString itemIdAt(QPoint &point);
    UnseenGroupItemInfo groupItemIdAt(QPoint &point);
	// Fill items of a group
//    void fillGroupItems(QTreeWidgetItem *categoryItem, const QList<GroupItemInfo> &itemList);
	// Set the unread count of an item
//	void setUnreadCount(QTreeWidgetItem *item, int unreadCount);

//	bool isSearchRequestItem(QPoint &point,uint32_t& search_req_id);
//	bool isSearchRequestResult(QPoint &point, QString &group_id, uint32_t& search_req_id);

	QTreeWidgetItem *getItemFromId(const QString &id);
//	QTreeWidgetItem *activateId(const QString &id, bool focus);

//	bool setWaiting(const QString &id, bool wait);

	QColor textColorCategory() const { return mTextColor[GROUPTREEWIDGET_COLOR_CATEGORY]; }
	QColor textColorPrivateKey() const { return mTextColor[GROUPTREEWIDGET_COLOR_PRIVATEKEY]; }

	void setTextColorCategory(QColor color) { mTextColor[GROUPTREEWIDGET_COLOR_CATEGORY] = color; }
	void setTextColorPrivateKey(QColor color) { mTextColor[GROUPTREEWIDGET_COLOR_PRIVATEKEY] = color; }
 //       bool getGroupName(QString id, QString& name);

//	int subscribeFlags(const QString &id);

    void updateAllGxsGroupList(std::vector<UnseenGroupItemInfo> allList);
    std::vector<UnseenGroupItemInfo> getAllGxsGroupList();
    void sortGxsConversationListByRecentTime();

    Ui::UnseenGroupTreeWidget *ui;

    UnseenGxsSmartListView *getUnseenGxsListView();

signals:
	void treeCustomContextMenuRequested(const QPoint &pos);
	void treeCurrentItemChanged(const QString &id);
	void treeItemActivated(const QString &id);
    void distantSearchRequested(const QString&) ;

protected:
	void changeEvent(QEvent *e);

private slots:
	void customContextMenuRequested(const QPoint &pos);
	void currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void itemActivated(QTreeWidgetItem *item, int column);
//	void filterChanged();
//	void distantSearch();

	void sort();

    //unseenp2p - using for new chat list
    void smartListSelectionChanged(const QItemSelection  &selected, const QItemSelection  &deselected);


private:
	// Initialize the display menu for sorting
	void initDisplayMenu(QToolButton *toolButton);
//	void calculateScore(QTreeWidgetItem *item, const QString &filterText);
	void resort(QTreeWidgetItem *categoryItem);
	void updateColors();

    //unseenp2p
    void selectConversation(const QModelIndex& index);
private:
	QMenu *displayMenu;
	QAction *actionSortAscending;
	QAction *actionSortDescending;
	QAction *actionSortByName;
	QAction *actionSortByPopularity;
	QAction *actionSortByLastPost;
	QAction *actionSortByPosts;
	QAction *actionSortByUnread;

	RSTreeWidgetItemCompareRole *compareRole;

	/* Color definitions (for standard see qss.default) */
	QColor mTextColor[GROUPTREEWIDGET_COLOR_COUNT];

    UnseenGxsSmartListModel* smartListModel_; //unseenp2p - use the MVC GUI for the gxs chat
    std::vector<UnseenGroupItemInfo> allGxsGroupList;

};

#endif // UNSEENGROUPTREEWIDGET_H
