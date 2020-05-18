/****************************************************************
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2014 RetroShare Team
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

#include "GxsChatUserNotify.h"
#include "gui/MainWindow.h"
#include <retroshare/rsidentity.h>

#include "gui/notifyqt.h"
#include "gui/SoundManager.h"
#include "gui/settings/rsharesettings.h"
#include "util/DateTime.h"
#include <util/HandleRichText.h>

#include "gui/chat/ChatDialog.h"

static std::map<gxsChatId, int> waitingChats;

GxsChatUserNotify::GxsChatUserNotify(RsGxsIfaceHelper *ifaceImpl, QObject *parent) :
    GxsUserNotify(ifaceImpl, parent)
{
    //connect(NotifyQt::getInstance(), SIGNAL(chatMessageReceived(ChatMessage)), this, SLOT(chatMessageReceived(ChatMessage)));
    //QObject::connect( NotifyQt::getInstance(), SIGNAL(newGxsChatMessageReceive(const gxsChatId&, const RsGxsChatMsg&, std::string, long long, std::string, bool)), this, SLOT(notifyGxsChatMessageReceive(const gxsChatId&, const RsGxsChatMsg&, std::string, long long, std::string, bool)));

    _bCheckForNickName = Settings->valueFromGroup(_group, "CheckForNickName", true).toBool();
    _bCountUnRead = Settings->valueFromGroup(_group, "CountUnRead", true).toBool();
    _bCountSpecificText = Settings->valueFromGroup(_group, "CountSpecificText", false).toBool();
    _textToNotify = Settings->valueFromGroup(_group, "TextToNotify").toStringList();
    _bTextCaseSensitive = Settings->valueFromGroup(_group, "TextCaseSensitive", false).toBool();
}


bool GxsChatUserNotify::hasSetting(QString *name, QString *group)
{
    if (name) *name = tr("New Chats");
    if (group) *group = "New Chats";

    return true;
}

void GxsChatUserNotify::setCheckForNickName(bool value)
{
    if (_bCheckForNickName != value) {
        _bCheckForNickName = value;
        Settings->setValueToGroup(_group, "CheckForNickName", value);
    }
}

void GxsChatUserNotify::setCountUnRead(bool value)
{
    if (_bCountUnRead != value) {
        _bCountUnRead = value;
        Settings->setValueToGroup(_group, "CountUnRead", value);
    }
}

void GxsChatUserNotify::setCountSpecificText(bool value)
{
    if (_bCountSpecificText != value) {
        _bCountSpecificText = value;
        Settings->setValueToGroup(_group, "CountSpecificText", value);
    }
}
void GxsChatUserNotify::setTextToNotify(QStringList value)
{
    if (_textToNotify != value) {
        _textToNotify = value;
        Settings->setValueToGroup(_group, "TextToNotify", value);
    }
}

void GxsChatUserNotify::setTextToNotify(QString value)
{
    while(value.contains("\n\n")) value.replace("\n\n","\n");
    QStringList list = value.split("\n");
    setTextToNotify(list);
}

void GxsChatUserNotify::setTextCaseSensitive(bool value)
{
    if (_bTextCaseSensitive != value) {
        _bTextCaseSensitive = value;
        Settings->setValueToGroup(_group, "TextCaseSensitive", value);
    }
}

QIcon GxsChatUserNotify::getIcon()
{
    return QIcon(":/home/img/face_icon/groupchat_tr.png");
}

QIcon GxsChatUserNotify::getMainIcon(bool hasNew)
{
    return hasNew ? QIcon(":/home/img/face_icon/groupchat_v.png") : QIcon(":/home/img/face_icon/groupchat_tr.png");
}

void GxsChatUserNotify::iconClicked()
{
    MainWindow::showWindow(MainWindow::GxsChats);
}

void GxsChatUserNotify::gxsChatNewMessage(RsGxsChatMsg gxsChatMsg, gxsChatId groupChatId, QDateTime time, QString senderName, QString msg)
{

    bool bGetNickName = false;
    bool bFoundTextToNotify = false;

    if(_bCountSpecificText)
        for (QStringList::Iterator it = _textToNotify.begin(); it != _textToNotify.end(); ++it) {
            bFoundTextToNotify |= checkWord(msg, (*it));
        }

    if ((bGetNickName || bFoundTextToNotify || _bCountUnRead)){
        QString strAnchor = time.toString(Qt::ISODate);
        MsgGxsData msgData;
        msgData.text=RsHtml::plainText(senderName) + ": " + msg;
        msgData.unread=!(bGetNickName || bFoundTextToNotify);

        _listMsg[groupChatId][strAnchor]=msgData;
        emit countChanged(gxsChatMsg, groupChatId, _listMsg[groupChatId].size());
        updateIcon();
        SoundManager::play(SOUND_NEW_LOBBY_MESSAGE);
    }
}

void GxsChatUserNotify::updateAsRead( gxsChatId groupChatId)
{
    lobby_map::iterator itCL=_listMsg.find(groupChatId);
    if (itCL!=_listMsg.end())
    {
        _listMsg.erase(itCL);
        //_listMsg[groupChatId]=msgData;
        emit countChanged(RsGxsChatMsg(), groupChatId, _listMsg[groupChatId].size());
        updateIcon();
    }

}

bool GxsChatUserNotify::checkWord(QString message, QString word)
{
    bool bFound = false;
    int nFound = -1;
    if (((nFound=message.indexOf(word,0,_bTextCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive)) != -1)
        && (!word.isEmpty())) {
        QString eow=" ~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="; // end of word
        bool bFirstCharEOW = (nFound==0)?true:(eow.indexOf(message.at(nFound-1)) != -1);
        bool bLastCharEOW = ((nFound+word.length()-1) < message.length())
            ?(eow.indexOf(message.at(nFound+word.length())) != -1)
           :true;
        bFound = (bFirstCharEOW && bLastCharEOW);
    }
    return bFound;
}

void GxsChatUserNotify::gxsChatCleared(gxsChatId groupChatId, QString anchor, bool onlyUnread /*=false*/)
{
    bool changed = anchor.isEmpty();
    unsigned int count=0;
    if (groupChatId.toGxsGroupId().toStdString()=="") return;
    lobby_map::iterator itCL=_listMsg.find(groupChatId);
    if (itCL!=_listMsg.end()) {
        if (!anchor.isEmpty()) {
            msg_map::iterator itMsg=itCL->second.find(anchor);
            if (itMsg!=itCL->second.end()) {
                MsgGxsData msgData = itMsg->second;
                if(!onlyUnread || msgData.unread) {
                    itCL->second.erase(itMsg);
                    changed=true;
                }
            }
            count = itCL->second.size();
        }
        if (count==0) _listMsg.erase(itCL);
    }
    //TODO: how to
    if (changed) emit countChanged(RsGxsChatMsg(), groupChatId, count);
    updateIcon();
}

unsigned int GxsChatUserNotify::getNewCount()
{
    unsigned int iNum=0;
    for (lobby_map::iterator itCL=_listMsg.begin(); itCL!=_listMsg.end();)
    {
        iNum+=itCL->second.size();

        if (itCL->second.size()==0)
        {
            lobby_map::iterator ittmp = itCL ;
            ++ittmp ;

            _listMsg.erase(itCL);
            itCL = ittmp ;
        }
        else
            ++itCL ;
    }

    return iNum;
}


