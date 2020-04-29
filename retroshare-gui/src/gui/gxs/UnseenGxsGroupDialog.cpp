/*
 * Retroshare Gxs Support
 *
 * Copyright 2012-2013 by Robert Fernie.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License Version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 *
 * Please report all bugs and problems to "retroshare@lunamutt.com".
 *
 */

#include <QMessageBox>

#include "util/misc.h"
#include "util/DateTime.h"
//for unseenp2p

#include "UnseenGxsGroupDialog.h"
#include "gui/common/PeerDefs.h"
#include "retroshare/rsgxsflags.h"

#include <algorithm>

#include <retroshare/rspeers.h>
#include <retroshare/rsgxscircles.h>

#include <gui/settings/rsharesettings.h>

#include <iostream>

#include "gui/chat/ChatDialog.h"


#define GXSGROUP_NEWGROUPID         1
#define GXSGROUP_LOADGROUP          2
#define GXSGROUP_INTERNAL_LOADGROUP 3

#define GXSGROUP_NEWGROUPID_2         6

/** Constructor */
UnseenGxsGroupDialog::UnseenGxsGroupDialog(TokenQueue *tokenExternalQueue, uint32_t enableFlags, uint32_t defaultFlags, QWidget *parent)
    : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint), mTokenService(NULL), mExternalTokenQueue(tokenExternalQueue), mInternalTokenQueue(NULL), mGrpMeta(), mMode(MODE_CREATE), mEnabledFlags(enableFlags), mReadonlyFlags(0), mDefaultsFlags(defaultFlags)
{
	/* Invoke the Qt Designer generated object setup routine */
	ui.setupUi(this);
	
	mInternalTokenQueue = NULL;

    std::set<RsPeerId> friends;
    std::set<RsPgpId> pgpFriends;
    std::set<RsGxsId> gxsFriends;

    init(UnseenFriendSelectionWidget::MODE_CREATE_GROUP, pgpFriends, friends, gxsFriends);
}

UnseenGxsGroupDialog::UnseenGxsGroupDialog(TokenQueue *tokenExternalQueue, RsTokenService *tokenService, Mode mode, RsGxsChatGroup::ChatType _chatType, RsGxsGroupId groupId, uint32_t enableFlags, uint32_t defaultFlags, QWidget *parent)
    : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint), mTokenService(NULL), mExternalTokenQueue(tokenExternalQueue), mInternalTokenQueue(NULL), mGrpMeta(), mMode(mode), mEnabledFlags(enableFlags), mReadonlyFlags(0), mDefaultsFlags(defaultFlags)
{
	/* Invoke the Qt Designer generated object setup routine */
	ui.setupUi(this);

	mTokenService = tokenService;
	mInternalTokenQueue = new TokenQueue(tokenService, this);
	mGrpMeta.mGroupId = groupId;
    chatType = _chatType;

    std::set<RsPeerId> friends;
    std::set<RsPgpId> pgpFriends;
    std::set<RsGxsId> gxsFriends;
    init(UnseenFriendSelectionWidget::MODE_EDIT_GROUP, pgpFriends, friends, gxsFriends);
}

UnseenGxsGroupDialog::~UnseenGxsGroupDialog()
{
	Settings->saveWidgetInformation(this);
	if (mInternalTokenQueue) {
		delete(mInternalTokenQueue);
	}
}

void UnseenGxsGroupDialog::init(UnseenFriendSelectionWidget::ShowFriendListMode _showMode, const std::set<RsPgpId>& peer_list2, const std::set<RsPeerId>& peer_list, const std::set<RsGxsId>& peer_list3)
{
	// connect up the buttons.
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(submitGroup()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(cancelDialog()));

    if(_showMode == UnseenFriendSelectionWidget::MODE_EDIT_GROUP)
    {
        switch (chatType) {
        case RsGxsChatGroup::ONE2ONE:
            ui.typeOne2One->setChecked(true);
            break;
        case RsGxsChatGroup::GROUPCHAT:
            ui.typeGroup->setChecked(true);
            break;
        case RsGxsChatGroup::CHANNEL:
            ui.typeChannel->setChecked(true);
            break;
        }

    }
    else
        ui.typeGroup->setChecked(true);

    setDefaultOptions();

    connect(ui.typeOne2One, SIGNAL(clicked()), this , SLOT(setDefaultOptions()));
    connect(ui.typeGroup, SIGNAL(clicked()), this , SLOT(setDefaultOptions()));
    connect(ui.typeChannel, SIGNAL(clicked()), this , SLOT(setDefaultOptions()));

    //ui.keyShareList->setModus(FriendSelectionWidget::MODUS_CHECK);
    ui.keyShareList->setModus(UnseenFriendSelectionWidget::MODUS_CHECK);
    ui.keyShareList->setShowType(UnseenFriendSelectionWidget::SHOW_GPG);
    ui.keyShareList->setModeOfFriendList(_showMode);
    ui.keyShareList->start();

    ui.keyShareList->setSelectedIds<RsGxsId,UnseenFriendSelectionWidget::IDTYPE_GXS>(peer_list3, false);
    ui.keyShareList->setSelectedIds<RsPeerId,UnseenFriendSelectionWidget::IDTYPE_SSL>(peer_list, false);
    ui.keyShareList->setSelectedIds<RsPgpId,UnseenFriendSelectionWidget::IDTYPE_GPG>(peer_list2, false);

	initMode();

    ui.keyShareList->setFocus();

	Settings->loadWidgetInformation(this);
}

QIcon UnseenGxsGroupDialog::serviceWindowIcon()
{
	return qApp->windowIcon();
}

void UnseenGxsGroupDialog::showEvent(QShowEvent*)
{
	ui.headerFrame->setHeaderImage(serviceImage());
	setWindowIcon(serviceWindowIcon());

	initUi();
}

void UnseenGxsGroupDialog::setUiText(UiType uiType, const QString &text)
{
	switch (uiType)
	{
	case UITYPE_SERVICE_HEADER:
		setWindowTitle(text);
		ui.headerFrame->setHeaderText(text);
		break;
	case UITYPE_KEY_SHARE_CHECKBOX:
        //ui.pubKeyShare_cb->setText(text);
		break;
	case UITYPE_CONTACTS_DOCK:
	case UITYPE_ADD_ADMINS_CHECKBOX:
		//ui.contactsdockWidget->setWindowTitle(text);
		break;
	case UITYPE_BUTTONBOX_OK:
        //ui.buttonBox->button(QDialogButtonBox::Ok)->setText(text);
		break;
	}
}

void UnseenGxsGroupDialog::setUiToolTip(UiType uiType, const QString &text)
{
	switch (uiType)
	{
	case UITYPE_KEY_SHARE_CHECKBOX:
        //ui.pubKeyShare_cb->setToolTip(text);
		break;
	case UITYPE_ADD_ADMINS_CHECKBOX:
        //ui.addAdmins_cb->setToolTip(text);
		break;
	case UITYPE_BUTTONBOX_OK:
        //ui.buttonBox->button(QDialogButtonBox::Ok)->setToolTip(text);
    default:
		break;
	}
}

void UnseenGxsGroupDialog::initMode()
{
	setAllReadonly();
	switch (mode())
	{
		case MODE_CREATE:
		{
            ui.stackedWidget->setCurrentIndex(0);
			ui.buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

			newGroup();
		}
		break;

		case MODE_SHOW:
		{
			ui.stackedWidget->setCurrentIndex(1);
			mReadonlyFlags = 0xffffffff; // Force all to readonly.
			ui.buttonBox->setStandardButtons(QDialogButtonBox::Close);
			requestGroup(mGrpMeta.mGroupId);

		}
		break;

		case MODE_EDIT:
		{
            ui.stackedWidget->setCurrentIndex(0);
            //ui.buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Apply);

            connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(editAndUpdateGroup()));
			requestGroup(mGrpMeta.mGroupId);
		}
		break;
	}
}

void UnseenGxsGroupDialog::editAndUpdateGroup()
{
    //Need to compare the member list/ group name before and after click on the Apply button
    //if there is different, so need to update the group

    std::set<GxsChatMember> newMemberList;
    ui.keyShareList->getSelectedContacts(newMemberList);
    QString newName = ui.groupName->text();
    if (newName != oldGroupName || newMemberList != oldMemberList)
    {

        std::list<RsGxsGroupId> groupChatId;
        groupChatId.push_back(mGrpMeta.mGroupId);
        std::vector<RsGxsChatGroup> chatsInfo;
        if (rsGxsChats->getChatsInfo(groupChatId, chatsInfo))
        {

            if (chatsInfo.size() > 0)
            {
                if (newName != oldGroupName)
                {
                    std::cerr << " The new group name changed to: " << newName.toStdString() << std::endl;
                    chatsInfo[0].mMeta.mGroupName = newName.toStdString();

                }
                if (newMemberList != oldMemberList)
                {
                    std::cerr << " The member list changed: " << std::endl;

                    chatsInfo[0].members = newMemberList;
                }
                //for both case: even not change the member list, need to add the own member,
                // because on the choosen list there is no own member
                GxsChatMember chatId;
                rsGxsChats->getOwnMember(chatId);
                chatsInfo[0].members.insert(chatId);

                uint32_t token;
                rsGxsChats->updateGroup(token, chatsInfo[0]);
            }
        }

    }
    else
    {
        std::cerr << " There is no change with the group "  << std::endl;
    }

    close();

}

void UnseenGxsGroupDialog::clearForm()
{
	ui.groupName->clear();

	ui.groupName->setFocus();
}

void UnseenGxsGroupDialog::setupDefaults()
{
	/* Enable / Show Parts based on Flags */	


	if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PUBLISH_MASK)
	{
		if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PUBLISH_ENCRYPTED)
		{
            //ui.publish_encrypt->setChecked(true);
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PUBLISH_REQUIRED)
		{
            //ui.publish_required->setChecked(true);
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PUBLISH_THREADS)
		{
           //ui.publish_threads->setChecked(true);
		}
		else
		{
			// default
            //ui.publish_open->setChecked(true);
		}
	}

	if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PERSONAL_MASK)
	{
		if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PERSONAL_PGP)
		{
            //ui.personal_pgp->setChecked(true);
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PERSONAL_REQUIRED)
		{
            //ui.personal_required->setChecked(true);
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PERSONAL_IFNOPUB)
		{
            //ui.personal_ifnopub->setChecked(true);
		}
		else
		{
			// default
            //ui.personal_ifnopub->setChecked(true);
		}
	}

	if (mDefaultsFlags & GXS_GROUP_DEFAULTS_COMMENTS_MASK)
	{
		if (mDefaultsFlags & GXS_GROUP_DEFAULTS_COMMENTS_YES)
		{
			ui.commentsValueLabel->setText(tr("Allowed"));
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_COMMENTS_NO)
		{
            ui.commentsValueLabel->setText(tr("Disallowed"));
		}
		else
		{
			// default
            ui.commentsValueLabel->setText(tr("Allowed"));
		}
	}
        
        QString antispam_string ;
        if(mDefaultsFlags & GXS_GROUP_DEFAULTS_ANTISPAM_TRACK) antispam_string += tr("Message tracking") ;
	if(mDefaultsFlags & GXS_GROUP_DEFAULTS_ANTISPAM_FAVOR_PGP) antispam_string += (antispam_string.isNull()?"":" and ")+tr("PGP signature required") ;
    
    	ui.antiSpamValueLabel->setText(antispam_string) ;
        
}

void UnseenGxsGroupDialog::setupVisibility()
{
    ui.groupName->setVisible(true);
    ui.distribGroupBox->setVisible(true);
    ui.typeGroup->setVisible(true);
    ui.label_2->setVisible(true);
    //ui.keyShareList->setVisible(true);

    ui.label->setVisible(true);

//	ui.extraFrame->setVisible(mEnabledFlags & GXS_GROUP_FLAGS_EXTRA);
}

void UnseenGxsGroupDialog::setAllReadonly()
{
	uint32_t origReadonlyFlags = mReadonlyFlags;
	mReadonlyFlags = 0xffffffff;

	setupReadonly();

	mReadonlyFlags = origReadonlyFlags;
}

void UnseenGxsGroupDialog::setupReadonly()
{

}

void UnseenGxsGroupDialog::newGroup()
{
	setupDefaults();
	setupVisibility();
	setupReadonly();
	clearForm();
    ui.keyShareList->updateDisplay(true);
    ui.keyShareList->start();
}

void UnseenGxsGroupDialog::updateFromExistingMeta(const QString &description)
{
    std::cerr << "void UnseenGxsGroupDialog::updateFromExistingMeta()";
    std::cerr << std::endl;

    circleType = mGrpMeta.mCircleType;
    std::cerr << "void UnseenGxsGroupDialog::updateFromExistingMeta() mGrpMeta.mCircleType: ";
    std::cerr << mGrpMeta.mCircleType << " Internal: " << mGrpMeta.mInternalCircle;
    std::cerr << " External: " << mGrpMeta.mCircleId;
    std::cerr << std::endl;

    setupDefaults();
    setupVisibility();
    setupReadonly();
    clearForm();
    setGroupSignFlags(mGrpMeta.mSignFlags) ;

    /* setup name */
    ui.groupName->setText(QString::fromUtf8(mGrpMeta.mGroupName.c_str()));

    /* Show Mode */
    ui.nameline->setText(QString::fromUtf8(mGrpMeta.mGroupName.c_str()));
    ui.popline->setText(QString::number( mGrpMeta.mPop)) ;
    ui.postsline->setText(QString::number(mGrpMeta.mVisibleMsgCount));
    if(mGrpMeta.mLastPost==0)
        ui.lastpostline->setText(tr("Never"));
    else
        ui.lastpostline->setText(DateTime::formatLongDateTime(mGrpMeta.mLastPost));
    ui.authorLabel->setId(mGrpMeta.mAuthorId);
    ui.IDline->setText(QString::fromStdString(mGrpMeta.mGroupId.toStdString()));
    ui.descriptiontextEdit->setPlainText(description);

    switch (mode())
    {
    case MODE_CREATE:{
    }
        break;
    case MODE_SHOW:{
        ui.headerFrame->setHeaderText(QString::fromUtf8(mGrpMeta.mGroupName.c_str()));
        if (!mPicture.isNull())
            ui.headerFrame->setHeaderImage(mPicture);
    }
        break;
    case MODE_EDIT:{
    }

        break;
    }
    /* set description */
//    ui.groupDesc->setPlainText(description);
    QString distribution_string = "[Unknown]";
    ui.distributionValueLabel->setText(distribution_string) ;

    setDefaultOptions();
}

void UnseenGxsGroupDialog::submitGroup()
{
    std::cerr << "UnseenGxsGroupDialog::submitGroup()";
	std::cerr << std::endl;

	/* switch depending on mode */
	switch (mode())
	{
		case MODE_CREATE:
		{
			/* just close if down */
			createGroup();
		}
		break;

		case MODE_SHOW:
		{
			/* just close if down */
			cancelDialog();
		}
		break;

		case MODE_EDIT:
		{
			editGroup();
		}
		break;
	}
}

void UnseenGxsGroupDialog::editGroup()
{
    std::cerr << "UnseenGxsGroupDialog::editGroup()" << std::endl;

	RsGroupMetaData newMeta;
	newMeta.mGroupId = mGrpMeta.mGroupId;

	if(!prepareGroupMetaData(newMeta))
	{
		/* error message */
        QMessageBox::warning(this, "UnseenP2P", tr("Failed to Prepare Group MetaData - please Review"), QMessageBox::Ok, QMessageBox::Ok);
		return; //Don't add  a empty name!!
	}

    std::cerr << "UnseenGxsGroupDialog::editGroup() calling service_EditGroup";
	std::cerr << std::endl;

    editAndUpdateGroup();

	uint32_t token;
	if (service_EditGroup(token, newMeta))
	{
		// get the Queue to handle response.
		if(mExternalTokenQueue != NULL)
			mExternalTokenQueue->queueRequest(token, TOKENREQ_GROUPINFO, RS_TOKREQ_ANSTYPE_ACK, GXSGROUP_NEWGROUPID);
	}
	else
	{
        std::cerr << "UnseenGxsGroupDialog::editGroup() ERROR";
		std::cerr << std::endl;
	}

	close();
}

bool UnseenGxsGroupDialog::prepareGroupMetaData(RsGroupMetaData &meta)
{
    std::cerr << "UnseenGxsGroupDialog::prepareGroupMetaData()";
	std::cerr << std::endl;

    QString name;
    if(chatType!= RsGxsChatGroup::ONE2ONE)
    {
        name = getName();
    }

    // Fill in the MetaData as best we can.
    meta.mGroupName = std::string(name.toUtf8());

    if(chatType!= RsGxsChatGroup::ONE2ONE && meta.mGroupName.length() == 0) {
        std::cerr << "UnseenGxsGroupDialog::prepareGroupMetaData()";
		std::cerr << " Invalid GroupName";
		std::cerr << std::endl;
		return false;
	}

	// Fill in the MetaData as best we can.
    meta.mSignFlags = getGroupSignFlags();


    std::cerr << "void UnseenGxsGroupDialog::prepareGroupMetaData() meta.mCircleType: ";
	std::cerr << meta.mCircleType << " Internal: " << meta.mInternalCircle;
	std::cerr << " External: " << meta.mCircleId;
	std::cerr << std::endl;

	return true;
}

void UnseenGxsGroupDialog::createGroup()
{
    std::cerr << "UnseenGxsGroupDialog::createGroup()";
	std::cerr << std::endl;
    RsGroupInfo::GroupType groupType = RsGroupInfo::DEFAULTS;

    if (ui.typeOne2One->isChecked())
    {
        chatType = RsGxsChatGroup::ONE2ONE;
        groupType = RsGroupInfo::ONE2ONE;
    }
    else if (ui.typeGroup->isChecked())
    {
        chatType = RsGxsChatGroup::GROUPCHAT;
        groupType = RsGroupInfo::GROUPCHAT;
    }
    else if (ui.typeChannel->isChecked())
    {
        chatType = RsGxsChatGroup::CHANNEL;
        groupType = RsGroupInfo::CHANNEL;
    }

    std::set<RsPgpId> gpgIds;
    std::set<RsGxsId>  gxsFriends;
    ui.keyShareList->selectedIds<RsPeerId,UnseenFriendSelectionWidget::IDTYPE_SSL>(mShareFriends, false);
    ui.keyShareList->selectedIds<RsPgpId,UnseenFriendSelectionWidget::IDTYPE_GPG>(gpgIds, false);
    ui.keyShareList->selectedIds<RsGxsId, UnseenFriendSelectionWidget::IDTYPE_GXS>(gxsFriends, false);
    ui.keyShareList->getSelectedContacts(mSelectedList);

    //Begin to create gxsgroup for 3 types: one2one chat, groupchat (private or public), and channel
    /*
     *  One2one chat:
     *      meta.mGroupFlags = flags = FLAG_PRIVACY_PUBLIC = 0x00000004; // anyone can publish, publish key pair not needed.
     *      + With one2one we just keep the group name as contact name, then the backend will can change then
     *      + One2one chat is private groupchat with 2 members, no bounce, no invite
     *          meta.mInternalCircle:
     *          + private ( GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY)
     *      + meta.mGroupName = Goku|meiyousixin,
     *      + group node name = GxsId of contact
     *    BEFORE creating ONE2ONE chat: need to check if existing that ONE2ONE chat with that contact:
     *    checkExistingOne2OneChat(pgpId), if true, than switch to the existing window, if not, continue to create.

     *  Groupchat:
     *      meta.mGroupFlags = flags = FLAG_PRIVACY_PUBLIC = 0x00000004; // anyone can publish, publish key pair not needed.
     *      meta.mInternalCircle:
     *         + private ( GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY)
               + public ( GXS_CIRCLE_TYPE_PUBLIC)

     *
     * Channel:
     *      meta.mGroupFlags = flags =FLAG_PRIVACY_RESTRICTED = 0x00000002; // publish private key needed to publish. Typical usage: channels.
            meta.mInternalCircle:
     *         + private ( GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY)
               + public ( GXS_CIRCLE_TYPE_PUBLIC)
           - allow comments
           - not allow comments

     */

	/* Check name */
	QString name = getName();
    if(name.isEmpty() && !ui.typeOne2One->isChecked())
	{
		/* error message */
        QMessageBox::warning(this, "UnseenP2P", tr("Please add a group/channel Name"), QMessageBox::Ok, QMessageBox::Ok);
		return; //Don't add  a empty name!!
	}

    if (gpgIds.empty())
    {
        QMessageBox::warning(this, "UnseenP2P", tr("Please choose a contact for chat"), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    if (ui.typeOne2One->isChecked())
    {
        // when choosing the one2one chat, the gxs name will be the name of choosing contact
        if (gpgIds.size() >= 2)
        {
            QMessageBox::warning(this, "UnseenP2P", tr("Please choose only one contact for chat"), QMessageBox::Ok, QMessageBox::Ok);
            return;
        }
        std::set<RsPgpId>::iterator it;
        std::string contactNick;
        RsPgpId contactPgpId;
        for (it = gpgIds.begin(); it != gpgIds.end(); ++it)
        {
            RsPeerDetails detail;

            if (rsPeers->getGPGDetails(*it, detail))
            {
                contactPgpId = (*it);
                contactNick = detail.name;
                break;
            }
        }
        std::string thisContactNick  = rsPeers->getPeerName(rsPeers->getOwnId());
        name = QString::fromStdString(thisContactNick) + "|" + QString::fromStdString(contactNick) ;

        std::cerr << "Create one2one groupname is: " << name.toStdString() << std::endl;
        //need to check the gxs One2One chat existing or not?
        if(rsPeers->checkExistingOne2OneChat(contactPgpId))
        {
            QMessageBox::warning(this, "UnseenP2P", tr("This conversation with this contact already existed, just enter the existing conversation to chat."), QMessageBox::Ok, QMessageBox::Ok);
            //enter to the existing conversation because it was already created
            close();
            return;
        }
    }

    RsGroupInfo groupInfo;
    groupInfo.id.clear(); // RS will generate an ID
     if (!ui.typeOne2One->isChecked())
            groupInfo.name = name.toStdString();
     else if(ui.typeOne2One->isChecked())
     {
         std::set<RsGxsId>::iterator it;
         std::string contactNick;
          for (it = gxsFriends.begin(); it != gxsFriends.end(); ++it)
          {
              groupInfo.name = (*it).toStdString();
              break;
          }
     }
    groupInfo.type = groupType;

    if(!rsPeers->addGroup(groupInfo, true))
    {
        QMessageBox::warning(this, "UnseenP2P", tr("Can not create group because the app can not create local group circle "), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    std::set<RsPgpId>::iterator it;
    for (it = gpgIds.begin(); it != gpgIds.end(); ++it) {
        rsPeers->assignPeerToGroup(groupInfo.id, *it, true);
    }

	uint32_t token;
	RsGroupMetaData meta;

    uint32_t flags;

    if (ui.typeOne2One->isChecked())
    {
        flags = GXS_SERV::FLAG_PRIVACY_PUBLIC;
        meta.mCircleType = GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY;
    }
    else if (ui.typeGroup->isChecked())
    {
        flags = GXS_SERV::FLAG_PRIVACY_PUBLIC;
         if (ui.groupTypeComboBox->currentIndex() == 0)
         {
             meta.mCircleType = GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY;
         }
         else if (ui.groupTypeComboBox->currentIndex() == 1)
         {
             meta.mCircleType = GXS_CIRCLE_TYPE_PUBLIC;
         }
    }
    else if (ui.typeChannel->isChecked())
    {
        flags = GXS_SERV::FLAG_PRIVACY_RESTRICTED;
         if (ui.channelType->currentIndex() == 0)
         {
             meta.mCircleType = GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY;
         }
         else if (ui.channelType->currentIndex() == 1)
         {
             meta.mCircleType = GXS_CIRCLE_TYPE_PUBLIC;
         }
    }

    meta.mInternalCircle = RsGxsCircleId(groupInfo.id);
    meta.mGroupFlags = flags;
    meta.mSignFlags = getGroupSignFlags();
    meta.mGroupName = name.toStdString();
    std::list<RsGxsId> ownIds;
    rsIdentity->getOwnIds(ownIds);
    if(!ownIds.empty())
        meta.mAuthorId = ownIds.front();

	if (!prepareGroupMetaData(meta))
	{
		/* error message */
        QMessageBox::warning(this, "UnseenP2P", tr("Failed to Prepare Group MetaData - please Review"), QMessageBox::Ok, QMessageBox::Ok);
		return; //Don't add with invalid circle.
	}

	if (service_CreateGroup(token, meta))
	{
		// get the Queue to handle response.
        if(mExternalTokenQueue != nullptr)
            //mExternalTokenQueue->queueRequest(token, TOKENREQ_GROUPINFO, RS_TOKREQ_ANSTYPE_ACK, GXSGROUP_NEWGROUPID);
            mExternalTokenQueue->queueRequest(token, TOKENREQ_GROUPINFO, RS_TOKREQ_ANSTYPE_ACK, GXSGROUP_NEWGROUPID_2);
	}

	close();
}
	
uint32_t UnseenGxsGroupDialog::getGroupSignFlags()
{
    /* grab from the ui options -> */
    uint32_t signFlags = 0;

    return signFlags;
}

void UnseenGxsGroupDialog::setGroupSignFlags(uint32_t signFlags)
{
                
        QString antispam_string ;
        if(signFlags & GXS_SERV::FLAG_AUTHOR_AUTHENTICATION_TRACK_MESSAGES) antispam_string += tr("Message tracking") ;
	if(signFlags & GXS_SERV::FLAG_AUTHOR_AUTHENTICATION_GPG_KNOWN)      antispam_string += (antispam_string.isNull()?"":" and ")+tr("PGP signature from known ID required") ;
    	else
	if(signFlags & GXS_SERV::FLAG_AUTHOR_AUTHENTICATION_GPG)            antispam_string += (antispam_string.isNull()?"":" and ")+tr("PGP signature required") ;
    
    ui.antiSpamValueLabel->setText(antispam_string) ;

	/* guess at comments */
	if ((signFlags & GXS_SERV::FLAG_GROUP_SIGN_PUBLISH_THREADHEAD) &&
	    (signFlags & GXS_SERV::FLAG_AUTHOR_AUTHENTICATION_IFNOPUBSIGN))
	{
        	// (cyril) very weird piece of code. Need to clear this up.
        
        	ui.commentsValueLabel->setText("Allowed") ;
	}
	else
	{
        	ui.commentsValueLabel->setText("Allowed") ;
	}
}

/**** Above logic is flawed, and will be removed shortly
 *
 *
 ****/

void UnseenGxsGroupDialog::setDefaultOptions()
{

    if (ui.typeOne2One->isChecked())
    {
        // Need to create one2one gxs chat, hide comment elements
        ui.label_2->setText("Select one friend with which you want to chat (one2one):");
        ui.label->setVisible(false);
        ui.groupName->setVisible(false);
        ui.channelType->setVisible(false);
        ui.groupTypeComboBox->setVisible(false);
        if (mode() ==  MODE_EDIT)
        {
            ui.typeGroup->setDisabled(true);
            ui.typeChannel->setDisabled(true);
        }
    }
    else if (ui.typeGroup->isChecked())
    {
        // Need to create gxs group chat, hide comment elements
        ui.groupTypeComboBox->setCurrentIndex(0);
        ui.label_2->setText("Select the friends with which you want to group chat: ");
        ui.label->setVisible(true);
        ui.groupName->setVisible(true);
        ui.channelType->setVisible(false);
        if (circleType == GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY)
            ui.groupTypeComboBox->setCurrentIndex(0);
        else if (circleType == GXS_CIRCLE_TYPE_PUBLIC)
            ui.groupTypeComboBox->setCurrentIndex(1);
        if (mode() ==  MODE_EDIT)
        {
            ui.typeOne2One->setDisabled(true);
            ui.typeChannel->setDisabled(true);
            ui.groupTypeComboBox->setDisabled(true);

            //need to check the selected contacts in the groupchat, and the inbox string list
            // at first get all the member in the list, remove myself and set it to the UnseenFriendSelectWIdget
            std::list<RsGxsGroupId> groupChatId;
            groupChatId.push_back(mGrpMeta.mGroupId);
            std::vector<RsGxsChatGroup> chatsInfo;
            if (rsGxsChats->getChatsInfo(groupChatId, chatsInfo))
            {

                if (chatsInfo.size() > 0)
                {
                    oldGroupName = QString::fromStdString(chatsInfo[0].mMeta.mGroupName);
                    std::set<GxsChatMember> memberList;
                    memberList.clear();
                    for (auto it(chatsInfo[0].members.begin()); it != chatsInfo[0].members.end(); ++it)
                    {
                        RsIdentityDetails detail;
                        if (rsIdentity->getIdDetails((*it).chatGxsId, detail))
                        {
                            //do not add myself into the contact list
                            if (detail.mPgpId == rsPeers->getGPGOwnId() ) continue;
                        }
                        memberList.insert((*it));
                    }

                    oldMemberList = memberList;
                    ui.keyShareList->setSelectedContacts(memberList);
                }
             }


        }
    }
    else if (ui.typeChannel->isChecked())
    {
        // Create channel, show the comment group elements
        ui.label_2->setText("Select the friends with which you want to make channel: ");
        ui.label->setVisible(true);
        ui.groupName->setVisible(true);
        ui.groupTypeComboBox->setVisible(false);
        ui.channelType->setVisible(true);
        ui.channelType->setCurrentIndex(0);
        if (circleType == GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY)
            ui.channelType->setCurrentIndex(0);
        else if (circleType == GXS_CIRCLE_TYPE_PUBLIC)
            ui.channelType->setCurrentIndex(1);
        if (mode() ==  MODE_EDIT)
        {
            ui.typeOne2One->setDisabled(true);
            ui.typeGroup->setDisabled(true);
            ui.channelType->setDisabled(true);

            //need to check the selected contacts in the groupchat, and the inbox string list
            // at first get all the member in the list, remove myself and set it to the UnseenFriendSelectWIdget
            std::list<RsGxsGroupId> groupChatId;
            groupChatId.push_back(mGrpMeta.mGroupId);
            std::vector<RsGxsChatGroup> chatsInfo;
            if (rsGxsChats->getChatsInfo(groupChatId, chatsInfo))
            {

                if (chatsInfo.size() > 0)
                {
                    oldGroupName = QString::fromStdString(chatsInfo[0].mMeta.mGroupName);
                    std::set<GxsChatMember> memberList;
                    memberList.clear();
                    for (auto it(chatsInfo[0].members.begin()); it != chatsInfo[0].members.end(); ++it)
                    {
                        RsIdentityDetails detail;
                        if (rsIdentity->getIdDetails((*it).chatGxsId, detail))
                        {
                            //do not add myself into the contact list
                            if (detail.mPgpId == rsPeers->getGPGOwnId() ) continue;
                        }
                        memberList.insert((*it));
                    }

                    oldMemberList = memberList;
                    ui.keyShareList->setSelectedContacts(memberList);
                }
             }
        }
    }


}

bool UnseenGxsGroupDialog::setCircleParameters(RsGroupMetaData &meta)
{
    meta.mCircleType = GXS_CIRCLE_TYPE_PUBLIC;
    meta.mCircleId.clear();
    meta.mOriginator.clear();
    meta.mInternalCircle.clear();

    return true;
}

void UnseenGxsGroupDialog::cancelDialog()
{
    std::cerr << "UnseenGxsGroupDialog::cancelDialog() Should Close!";
	std::cerr << std::endl;

	close();
}

void UnseenGxsGroupDialog::addGroupLogo()
{
	QPixmap img = misc::getOpenThumbnailedPicture(this, tr("Load Group Logo"), 64, 64);
	
	if (img.isNull())
		return;

	setLogo(img);
}

QPixmap UnseenGxsGroupDialog::getLogo()
{
	return mPicture;
}

void UnseenGxsGroupDialog::setLogo(const QPixmap &pixmap)
{
	mPicture = pixmap;

	// to show the selected
//	ui.groupLogo->setPixmap(mPicture);
}

QString UnseenGxsGroupDialog::getName()
{
	return misc::removeNewLine(ui.groupName->text());
}

QString UnseenGxsGroupDialog::getDescription()
{
    return ui.groupName->text();
}

void UnseenGxsGroupDialog::getSelectedModerators(std::set<RsGxsId>& ids)
{
//	ui.adminsList->selectedIds<RsGxsId,FriendSelectionWidget::IDTYPE_GXS>(ids, true);
}

void UnseenGxsGroupDialog::setSelectedModerators(const std::set<RsGxsId>& ids)
{
//	ui.adminsList->setSelectedIds<RsGxsId,FriendSelectionWidget::IDTYPE_GXS>(ids, false);

	QString moderatorsListString ;
    RsIdentityDetails det;

    for(auto it(ids.begin());it!=ids.end();++it)
    {
		rsIdentity->getIdDetails(*it,det);

        if(!moderatorsListString.isNull())
            moderatorsListString += ", " ;

        moderatorsListString += det.mNickname.empty()?("[Unknown]"):QString::fromStdString(det.mNickname) ;
    }

	ui.moderatorsLabel->setText(moderatorsListString);
}

/***********************************************************************************
  Share Lists.
 ***********************************************************************************/

void UnseenGxsGroupDialog::sendShareList(std::string /*groupId*/)
{
	close();
}

void UnseenGxsGroupDialog::setAdminsList()
{
//	if (ui.addAdmins_cb->isChecked())
//    {
//		//this->resize(this->size().width() + ui.contactsdockWidget->size().width(), this->size().height());
//		ui.adminsList->show();
//	}
//    else
//    {  // hide share widget
//		ui.adminsList->hide();
//		//this->resize(this->size().width() - ui.contactsdockWidget->size().width(), this->size().height());
//	}
}

void UnseenGxsGroupDialog::setShareList()
{
//	if (ui.pubKeyShare_cb->isChecked()) {
//		QMessageBox::warning(this, "", "ToDo");
//		ui.pubKeyShare_cb->setChecked(false);
//	}
//	if (ui.pubKeyShare_cb->isChecked()){
//		this->resize(this->size().width() + ui.contactsdockWidget->size().width(), this->size().height());
//		ui.contactsdockWidget->show();
//	} else {  // hide share widget
//		ui.contactsdockWidget->hide();
//		this->resize(this->size().width() - ui.contactsdockWidget->size().width(), this->size().height());
//	}
}

/***********************************************************************************
  Loading Group.
 ***********************************************************************************/

void UnseenGxsGroupDialog::requestGroup(const RsGxsGroupId &groupId)
{
	RsTokReqOptions opts;
	opts.mReqType = GXS_REQUEST_TYPE_GROUP_DATA;

	std::list<RsGxsGroupId> groupIds;
	groupIds.push_back(groupId);

    std::cerr << "UnseenGxsGroupDialog::requestGroup() Requesting Group Summary(" << groupId << ")";
	std::cerr << std::endl;

	uint32_t token;
	if (mInternalTokenQueue)
		mInternalTokenQueue->requestGroupInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, groupIds, GXSGROUP_INTERNAL_LOADGROUP) ;
}

void UnseenGxsGroupDialog::loadGroup(uint32_t token)
{
    std::cerr << "UnseenGxsGroupDialog::loadGroup(" << token << ")";
	std::cerr << std::endl;

	QString description;
	if (service_loadGroup(token, mMode, mGrpMeta, description))
	{
		updateFromExistingMeta(description);
	}
}

void UnseenGxsGroupDialog::loadRequest(const TokenQueue *queue, const TokenRequest &req)
{
    std::cerr << "UnseenGxsGroupDialog::loadRequest() UserType: " << req.mUserType;
	std::cerr << std::endl;

	if (queue == mInternalTokenQueue)
	{
		/* now switch on req */
		switch(req.mUserType)
		{
			case GXSGROUP_INTERNAL_LOADGROUP:
				loadGroup(req.mToken);
				break;
            default:
                std::cerr << "UnseenGxsGroupDialog::loadGroup() UNKNOWN UserType ";
				std::cerr << std::endl;
				break;
		}
	}
}

void UnseenGxsGroupDialog::getShareFriends(std::set<GxsChatMember> &selectedList)
{
    selectedList = mSelectedList;

}

RsGxsChatGroup::ChatType UnseenGxsGroupDialog::getChatType()
{
    return chatType;
}

