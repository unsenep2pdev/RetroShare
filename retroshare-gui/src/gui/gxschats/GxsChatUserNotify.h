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

#ifndef GXSCHATUSERNOTIFY_H
#define GXSCHATUSERNOTIFY_H

#include <QDateTime>

#include "gui/gxs/GxsUserNotify.h"
#include "retroshare/rsmsgs.h"
#include "retroshare/rsgxschats.h"

struct ActionTagGxs {
    RsGxsGroupId cli;
    QString timeStamp;
    bool removeALL;
};
Q_DECLARE_METATYPE(ActionTagGxs)


struct MsgGxsData {
    QString text;
    bool unread;
};
Q_DECLARE_METATYPE(MsgGxsData)

class GxsChatUserNotify : public GxsUserNotify
{
    Q_OBJECT

public:
    GxsChatUserNotify(RsGxsIfaceHelper *ifaceImpl, QObject *parent = 0);

    virtual bool hasSetting(QString *name, QString *group);

    virtual QIcon getIcon();
    virtual QIcon getMainIcon(bool hasNew);
    virtual void iconClicked();

    void gxsChatNewMessage(RsGxsChatMsg gxsChatMsg, gxsChatId groupChatId, QDateTime time, QString senderName, QString msg);
    void gxsChatCleared(gxsChatId groupChatId, QString anchor, bool onlyUnread=false);
//	void setCheckForNickName(bool value);
//	bool isCheckForNickName() { return _bCheckForNickName;}
//	void setCountUnRead(bool value);
//	bool isCountUnRead() { return _bCountUnRead;}
//	void setCountSpecificText(bool value);
//	bool isCountSpecificText() { return _bCountSpecificText;}
//	void setTextToNotify(QStringList);
//	void setTextToNotify(QString);
//	QString textToNotify() { return _textToNotify.join("\n");}
//	void setTextCaseSensitive(bool value);
//	bool isTextCaseSensitive() {return _bTextCaseSensitive;}

private slots:
    void chatMessageReceived(ChatMessage msg);

signals:
    void countChanged(RsGxsChatMsg gxsChatMsg, gxsChatId id, unsigned int count);
private:
//	virtual QIcon getIcon();
//	virtual QIcon getMainIcon(bool hasNew);
//	virtual unsigned int getNewCount();
//	virtual QString getTrayMessage(bool plural);
//	virtual QString getNotifyMessage(bool plural);
//	virtual void iconClicked();
//	virtual void iconHovered();
    bool checkWord(QString msg, QString word);

//	QString _name;
//	QString _group;

    typedef std::map<QString, MsgGxsData> msg_map;
    typedef	std::map<gxsChatId, msg_map> lobby_map;
    lobby_map _listMsg;

//    typedef	std::map<ChatId, msg_map> p2pchat_map;
//    p2pchat_map _listP2PMsg;

    bool _bCountUnRead;
    bool _bCheckForNickName;
    bool _bCountSpecificText;
    QStringList _textToNotify;
    bool _bTextCaseSensitive;
};


#endif // GXSCHATUSERNOTIFY_H
