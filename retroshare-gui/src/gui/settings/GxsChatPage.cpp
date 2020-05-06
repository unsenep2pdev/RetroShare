/****************************************************************
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2006, crypton
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

#include "GxsChatPage.h"
#include "rsharesettings.h"
#include "util/misc.h"
#include "gui/notifyqt.h"

GxsChatPage::GxsChatPage(QWidget * parent, Qt::WindowFlags flags)
    : ConfigPage(parent, flags)
{
    ui.setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);

    /* Initialize GroupFrameSettingsWidget */
    ui.groupFrameSettingsWidget->setOpenAllInNewTabText(tr("Open each GxsChat in a new tab"));
    ui.groupFrameSettingsWidget->setType(GroupFrameSettings::Chats) ;

    connect(ui.loadThreadCheckBox,SIGNAL(toggled(bool)),this,SLOT(updateLoadThread())) ;
}

void GxsChatPage::updateLoadThread()
{
    Settings->setChatsLoadThread(ui.loadThreadCheckBox->isChecked());
    NotifyQt::getInstance()->notifySettingsChanged();
}

GxsChatPage::~GxsChatPage()
{
}

/** Loads the settings for this page */
void GxsChatPage::load()
{
    whileBlocking(ui.loadThreadCheckBox)->setChecked(Settings->getChatsLoadThread());
    ui.groupFrameSettingsWidget->loadSettings(GroupFrameSettings::Chats);
}
