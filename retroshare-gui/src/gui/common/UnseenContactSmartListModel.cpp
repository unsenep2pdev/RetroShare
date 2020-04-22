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

#include "UnseenContactSmartListModel.h"

// Qt
#include <QDateTime>
#include <QImage>
#include <QSize>
#include <QPainter>
#include <QtXml>
#include <QDomComment>
#include "gui/gxs/GxsIdDetails.h"

//#include "gui/models/conversationmodel.h"
#include "retroshare/rsgxsflags.h"
#include "retroshare/rspeers.h"
#include "util/HandleRichText.h"

#include "retroshare/rsstatus.h"
#include "gui/common/AvatarDefs.h"


#define IMAGE_PUBLIC          ":/chat/img/groundchat.png"               //copy from ChatLobbyWidget
#define IMAGE_PRIVATE         ":/chat/img/groundchat_private.png"       //copy from ChatLobbyWidget
#define IMAGE_UNSEEN          ":/app/images/unseen32.png"


// Client
UnseenContactSmartListModel::UnseenContactSmartListModel(const std::string& accId, QObject *parent, bool contactList)
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

int UnseenContactSmartListModel::rowCount(const QModelIndex &parent) const
{

    int count = allIdentities.size();
    return count;
}

int UnseenContactSmartListModel::columnCount(const QModelIndex &parent) const
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

static QString getHumanReadableDuration(uint32_t seconds)
{
    if(seconds < 60)
        return QString(QObject::tr("%1 seconds ago")).arg(seconds) ;
    else if(seconds < 120)
        return QString(QObject::tr("%1 minute ago")).arg(seconds/60) ;
    else if(seconds < 3600)
        return QString(QObject::tr("%1 minutes ago")).arg(seconds/60) ;
    else if(seconds < 7200)
        return QString(QObject::tr("%1 hour ago")).arg(seconds/3600) ;
    else if(seconds < 24*3600)
        return QString(QObject::tr("%1 hours ago")).arg(seconds/3600) ;
    else if(seconds < 2*24*3600)
        return QString(QObject::tr("%1 day ago")).arg(seconds/86400) ;
    else
        return QString(QObject::tr("%1 days ago")).arg(seconds/86400) ;
}

QVariant UnseenContactSmartListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (allIdentities.size() == 0) return QVariant();

    else
    {
        //Get avatar for groupchat or contact item
        RsGxsGroupId contactId = allIdentities.at(index.row());
        RsIdentityDetails detail;
        if (!rsIdentity->getIdDetails(RsGxsId(contactId), detail))
        {
            return QVariant();
        }

        QList<QIcon> icons ;
        GxsIdDetails::getIcons(detail,icons,GxsIdDetails::ICON_TYPE_AVATAR) ;
        QIcon identicon = icons.front() ;

        QString name = QString::fromUtf8(detail.mNickname.c_str());
        QString presenceForChat = "no-status"; //for groupchat

        RsPeerDetails details;
        RsPeerId sslId;
        if (rsPeers->getGPGDetails(detail.mPgpId, details))
        {
            std::list<RsPeerId> sslIds;
            rsPeers->getAssociatedSSLIds(details.gpg_id, sslIds);
            if (sslIds.size() >= 1) {

                sslId = sslIds.front();
            }

            StatusInfo statusContactInfo;

            rsStatus->getStatus(sslId,statusContactInfo);
            switch (statusContactInfo.status)
            {
                case RS_STATUS_OFFLINE:
                    presenceForChat = "offline";
                    break;
                case RS_STATUS_INACTIVE:
                    presenceForChat = "idle";
                    break;
                case RS_STATUS_ONLINE:
                    presenceForChat = "online";
                    break;
                case RS_STATUS_AWAY:
                    presenceForChat = "away";
                    break;
                case RS_STATUS_BUSY:
                    presenceForChat = "busy";
                    break;
            }
        }

        QImage avatar = identicon.pixmap(identicon.actualSize(QSize(32, 32))).toImage();
        QString lastMsgStatus  = "last seen ";
        rstime_t lastseen = detail.mLastUsageTS;

        time_t now = time(NULL) ;
        lastMsgStatus += getHumanReadableDuration(now - detail.mLastUsageTS) ;
         QString isChoosenContact  = "";

         RsGxsMyContact::STATUS status;
         if (!sslId.isNull())
         {
             status = RsGxsMyContact::TRUSTED;
         }
         else status = RsGxsMyContact::UNKNOWN;

         GxsChatMember contact;
         contact.chatGxsId = detail.mId;
         contact.chatPeerId = sslId;
         contact.nickname = detail.mNickname;
         contact.status = status;

         //RsGxsMyContact thisContact(detail.mId, detail.mPgpId, NULL, detail.mNickname,);
         if(selectedList.find(contact)!= selectedList.end())
            isChoosenContact = QString("•");

         // isChoosenContact = QString("√");


//////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// BEGIN TO CHOOSE value after PREPARING ALLs.                          ////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

        try {
            switch (role) {
            case Role::Picture:
            case Qt::DecorationRole:

                return QPixmap::fromImage(getCirclePhoto(avatar, avatar.size().width() )) ;
            case Role::DisplayName:
            case Qt::DisplayRole:
            {
                return QVariant(name);
            }
            case Role::DisplayID:
            {
                return  QVariant(lastMsgStatus);
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
                return QVariant(isChoosenContact);
            }
            case Role::LastInteraction:
                return QVariant("");
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

QModelIndex UnseenContactSmartListModel::index(int row, int column, const QModelIndex &parent) const
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

QModelIndex UnseenContactSmartListModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

Qt::ItemFlags UnseenContactSmartListModel::flags(const QModelIndex &index) const
{
    auto flags = QAbstractItemModel::flags(index) | Qt::ItemNeverHasChildren | Qt::ItemIsSelectable;
    return flags;
}

void UnseenContactSmartListModel::setAccount(const std::string& accId)
{
    beginResetModel();
    accId_ = accId;
    endResetModel();
}

void UnseenContactSmartListModel::setAllIdentites(std::vector<RsGxsGroupId> allList)
{
    allIdentities = allList;
}

void UnseenContactSmartListModel::setChoosenIdentities(std::set<GxsChatMember> allList)
{
    selectedList = allList;
}

std::vector<RsGxsGroupId> UnseenContactSmartListModel::getAllIdentities()
{
    return allIdentities;
}

void UnseenContactSmartListModel::sortGxsConversationListByRecentTime()
{
}
