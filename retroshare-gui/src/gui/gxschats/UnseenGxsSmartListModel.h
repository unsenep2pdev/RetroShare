/***************************************************************************
* Copyright (C) 2017-2019 by Savoir-faire Linux                                *
* Author: Anthony L�onard <anthony.leonard@savoirfairelinux.com>          *
*                                                                         *
* This program is free software; you can redistribute it and/or modify    *
* it under the terms of the GNU General Public License as published by    *
* the Free Software Foundation; either version 3 of the License, or       *
* (at your option) any later version.                                     *
*                                                                         *
* This program is distributed in the hope that it will be useful,         *
* but WITHOUT ANY WARRANTY; without even the implied warranty of          *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
* GNU General Public License for more details.                            *
*                                                                         *
* You should have received a copy of the GNU General Public License       *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
**************************************************************************/

#pragma once

//#include "gui/common/UnseenGroupTreeWidget.h"
// Qt include
#include <QAbstractItemModel>
#include <QDateTime>
#include <QIcon>

class UnseenGroupItemInfo
{
public:
    UnseenGroupItemInfo()
      : popularity(0), publishKey(false), adminKey(false)
      , subscribeFlags(0), max_visible_posts(0)
    {}

public:
    QString   id;
    QString   name;
    QString   description;
    int       popularity;
    QDateTime lastpost;
    QIcon     icon;
    bool      publishKey;
    bool      adminKey;
    quint32  subscribeFlags;
    quint32  max_visible_posts ;
    long long lastMsgDatetime;  //unseenp2p: for sorting: lastpost --> QDateTime::currentDateTime().toTime_t()
    QString authorOfLastMsg;    //unseenp2p: for show on the gxs conversation list
};

class UnseenGxsSmartListModel : public QAbstractItemModel
{
    Q_OBJECT
public:


    enum Role {
        DisplayName = Qt::UserRole + 1,
        DisplayID,
        Picture,
        Presence,
        URI,
        UnreadMessagesCount,
        LastInteractionDate,
        LastInteraction,
        LastInteractionType,
        ContactType,
        UID,
        ContextMenuOpen
    };

    explicit UnseenGxsSmartListModel(const std::string& accId, QObject *parent = 0, bool contactList = false);

    // QAbstractItemModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    void setAccount(const std::string& accId);

    // hack for context menu highlight retention
    bool isContextMenuOpen{ false };
    std::vector<UnseenGroupItemInfo> getGxsGroupList();
    void setGxsGroupList(std::vector<UnseenGroupItemInfo> allList);
    void sortGxsConversationListByRecentTime();
private:
    std::string accId_;
    bool contactList_;
    std::vector<UnseenGroupItemInfo> allGxsGroupList;
};
