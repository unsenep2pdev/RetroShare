/***************************************************************************
 * Copyright (C) 2017-2019 by Savoir-faire Linux                                *
 * Author: Anthony L�onard <anthony.leonard@savoirfairelinux.com>          *
 * Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>          *
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

#include "UnseenGxsSmartlistmodel.h"
//#include "gui/common/UnseenGroupTreeWidget.h"

// Qt
#include <QDateTime>
#include <QImage>
#include <QSize>
#include <QPainter>
#include <QtXml>
#include <QDomComment>

#include "gui/models/conversationmodel.h"
#include "retroshare/rsgxsflags.h"

#include "util/HandleRichText.h"


#define IMAGE_PUBLIC          ":/chat/img/groundchat.png"               //copy from ChatLobbyWidget
#define IMAGE_PRIVATE         ":/chat/img/groundchat_private.png"       //copy from ChatLobbyWidget
#define IMAGE_UNSEEN          ":/app/images/unseen32.png"

class UnseenGroupItemInfo;
//class UnseenGroupTreeWidget;

// Client
UnseenGxsSmartListModel::UnseenGxsSmartListModel(const std::string& accId, QObject *parent, bool contactList)
    : QAbstractItemModel(parent),
    accId_(accId),
    contactList_(contactList)
{
    setAccount(accId_);
}

static QString readMsgFromXml(const QString &historyMsg)
{
    QDomDocument doc;
    if (!doc.setContent(historyMsg)) return "Media";

    //QDomNodeList rates = doc.elementsByTagName("body");
    QDomElement body = doc.firstChildElement("body");
    QDomElement span1 = body.firstChildElement("span");
    QDomElement span2 = span1.firstChildElement("span");
    QDomElement span3 = span2.firstChildElement("span");

    return span3.text();
}

int UnseenGxsSmartListModel::rowCount(const QModelIndex &parent) const
{

    int count = allGxsGroupList.size();
    return count;
}

int UnseenGxsSmartListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

static QImage getCirclePhoto(const QImage original, int sizePhoto)
{
    QImage target(sizePhoto, sizePhoto, QImage::Format_ARGB32_Premultiplied);
    target.fill(Qt::transparent);

    QPainter painter(&target);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter.setBrush(QBrush(Qt::white));
    auto scaledPhoto = original
            .scaled(sizePhoto, sizePhoto, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int margin = 0;
    if (scaledPhoto.width() > sizePhoto) {
        margin = (scaledPhoto.width() - sizePhoto) / 2;
    }
    painter.drawEllipse(0, 0, sizePhoto, sizePhoto);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.drawImage(0, 0, scaledPhoto, margin, 0);
    return target;
}



QVariant UnseenGxsSmartListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (allGxsGroupList.size() == 0) return QVariant();

    else
    {
        //Get avatar for groupchat or contact item
        UnseenGroupItemInfo chatItem = allGxsGroupList.at(index.row());

        //STATUS FOR CONTACT

        QString presenceForChat = "no-status"; //for groupchat

        QImage avatar(IMAGE_PUBLIC);    //default is public group chat avatar for UnseenP2P

        bool isAdmin      = IS_GROUP_ADMIN(chatItem.subscribeFlags);
        bool isSubscribed = IS_GROUP_SUBSCRIBED(chatItem.subscribeFlags);

        if (isAdmin)      //if this is a group chat that I created
        {
            avatar = QImage(IMAGE_PRIVATE);
        }
        else if (isSubscribed)
                avatar = QImage(IMAGE_PUBLIC);


        ////////////////////////////////////////////
        /////// GET DATETIME for last msg //////////
        ////////////////////////////////////////////
        //date time format for msg: Mon Jan 21 12:39:35 1970 or Sat Oct 26 10:13:09 2019
        //                         "Sat Nov 9 17:49:42 2019"
        // if msg in the day, choose the "12:39"
        // if msg in the 7 days, choose the 3 first character like "Mon"
        // if msg older than 7 days, choose "Jan 21"
        QString timedateForMsgResult;
        //QDateTime dateTime =  QDateTime::fromTime_t(chatItem.lastMsgDatetime);
        QDateTime dateTime = chatItem.lastpost;
        QString timedateForMsg = dateTime.toString();
        qint64 secondsOfDatetime = dateTime.toSecsSinceEpoch();

        qint64 now = QDateTime::currentDateTime().toSecsSinceEpoch();
        //if the time is Thu Jan 1 00:00:00 1970 GMT, so the secondsOfDatetime = 0, need to set the current time
        if (secondsOfDatetime == 0)
            secondsOfDatetime = now - 100;
        // if msg in the day, choose the "12:39"
        if (now - secondsOfDatetime <  86400)       //secondsIn1Day  =  86400;
        {
            // (SystemLocale)	"11/9/19 5:49 PM"	QString
            QString sysLocal = dateTime.toString(Qt::SystemLocaleDate);
            timedateForMsgResult = sysLocal.mid(sysLocal.length() - 8, 8);
        }
        // if msg in the 7 days, choose the 3 first character like "Mon"
        else if (now - secondsOfDatetime < 604800)      //secondsIn7Days = 604800;
        {
            timedateForMsgResult = timedateForMsg.mid(0,3);
        }
        // if msg older than 7 days, choose "Jan 21"
        else
        {
            // (SystemLocale)	"11/9/19 5:49 PM"	QString --> get this one: 11/9/19
            QString sysLocal = dateTime.toString(Qt::SystemLocaleDate);
            timedateForMsgResult = sysLocal.mid(0,8);
            //timedateForMsgResult.append(timedateForMsg.mid(19,4));
        }

        //GET LAST MSG from html format
        QString lastMsgQstr = chatItem.description;
        QDomDocument docCheck;
        QString temp = lastMsgQstr;
        //TODO: need to add last msg/info of the publisher
        QString lastMsg = lastMsgQstr;


        //TODO: GET status of last msg, check if the last msg is "You" or other?
        QString lastMsgStatus =  "";


////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////// BEGIN TO CHOOSE value after PREPARING ALLs.                          ////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

        try {
            switch (role) {
            case Role::Picture:
            case Qt::DecorationRole:

                return QPixmap::fromImage(getCirclePhoto(avatar, avatar.size().width() )) ;
            case Role::DisplayName:
            case Qt::DisplayRole:
            {
                return QVariant(chatItem.name);
            }
            case Role::DisplayID:
            {
                return  QVariant(lastMsg);
            }
            case Role::Presence:
            {
                return QVariant(presenceForChat);
            }
            case Role::URI:
            {
                return QVariant(QString::fromStdString("unseenp2p.com"));
            }
            case Role::UnreadMessagesCount:
                return QVariant(0);
            case Role::LastInteractionDate:
            {
                return QVariant(timedateForMsgResult);
            }
            case Role::LastInteraction:
                return QVariant(lastMsgStatus);
            case Role::LastInteractionType:
                return QVariant(0);
            case Role::ContactType:
            {
                return QVariant(0);
            }
            case Role::UID:
                return QVariant(QString("1234"));
            case Role::ContextMenuOpen:
                return QVariant(isContextMenuOpen);
            }
        } catch (...) {}
    }

    return QVariant();
}

QModelIndex UnseenGxsSmartListModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (column != 0) {
        return QModelIndex();
    }

    if (row >= 0 && row < rowCount()) {
        return createIndex(row, column);
    }
    return QModelIndex();
}

QModelIndex UnseenGxsSmartListModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

Qt::ItemFlags UnseenGxsSmartListModel::flags(const QModelIndex &index) const
{
    auto flags = QAbstractItemModel::flags(index) | Qt::ItemNeverHasChildren | Qt::ItemIsSelectable;
    return flags;
}

void
UnseenGxsSmartListModel::setAccount(const std::string& accId)
{
    beginResetModel();
    accId_ = accId;
    endResetModel();
}

void UnseenGxsSmartListModel::setGxsGroupList(std::vector<UnseenGroupItemInfo> allList)
{
    allGxsGroupList = allList;
}

std::vector<UnseenGroupItemInfo> UnseenGxsSmartListModel::getGxsGroupList()
{
    return allGxsGroupList;
}

void UnseenGxsSmartListModel::sortGxsConversationListByRecentTime()
{
    std::sort(allGxsGroupList.begin(), allGxsGroupList.end(),
              [] (UnseenGroupItemInfo const& a, UnseenGroupItemInfo const& b)
    { return a.lastMsgDatetime > b.lastMsgDatetime; });
}
