#include <list>

#include <QLayout>
#include <QDialogButtonBox>
#include "UnseenFriendSelectionDialog.h"

std::set<RsPgpId> UnseenFriendSelectionDialog::selectFriends_PGP(QWidget *parent,const QString& caption,const QString& header_text,
                            UnseenFriendSelectionWidget::Modus modus,
                            UnseenFriendSelectionWidget::ShowTypes show_type,
                            const std::set<RsPgpId>& pre_selected_ids)
{
    std::set<std::string> psids ;
    for(std::set<RsPgpId>::const_iterator it(pre_selected_ids.begin());it!=pre_selected_ids.end();++it)
        psids.insert( (*it).toStdString() ) ;

    UnseenFriendSelectionDialog dialog(parent,header_text,modus,show_type,UnseenFriendSelectionWidget::IDTYPE_GPG,psids) ;

    dialog.setWindowTitle(caption) ;

    if(QDialog::Rejected == dialog.exec())
        return std::set<RsPgpId>() ;

    std::set<RsPgpId> sids ;
    dialog.friends_widget->selectedIds<RsPgpId,UnseenFriendSelectionWidget::IDTYPE_GPG>(sids,false) ;

    return sids ;
}
std::set<RsPeerId> UnseenFriendSelectionDialog::selectFriends_SSL(QWidget *parent,const QString& caption,const QString& header_text,
                            UnseenFriendSelectionWidget::Modus modus,
                            UnseenFriendSelectionWidget::ShowTypes show_type,
                            const std::set<RsPeerId>& pre_selected_ids)
{
    std::set<std::string> psids ;
    for(std::set<RsPeerId>::const_iterator it(pre_selected_ids.begin());it!=pre_selected_ids.end();++it)
        psids.insert( (*it).toStdString() ) ;

    UnseenFriendSelectionDialog dialog(parent,header_text,modus,show_type,UnseenFriendSelectionWidget::IDTYPE_SSL,psids) ;

    dialog.setWindowTitle(caption) ;

    if(QDialog::Rejected == dialog.exec())
        return std::set<RsPeerId>() ;

    std::set<RsPeerId> sids ;
    dialog.friends_widget->selectedIds<RsPeerId,UnseenFriendSelectionWidget::IDTYPE_SSL>(sids,false) ;

    return sids ;
}
std::set<RsGxsId> UnseenFriendSelectionDialog::selectFriends_GXS(QWidget *parent,const QString& caption,const QString& header_text,
                            UnseenFriendSelectionWidget::Modus modus,
                            UnseenFriendSelectionWidget::ShowTypes show_type,
                            const std::set<RsGxsId>& pre_selected_ids)
{
    std::set<std::string> psids ;
    for(std::set<RsGxsId>::const_iterator it(pre_selected_ids.begin());it!=pre_selected_ids.end();++it)
        psids.insert( (*it).toStdString() ) ;

    UnseenFriendSelectionDialog dialog(parent,header_text,modus,show_type,UnseenFriendSelectionWidget::IDTYPE_SSL,psids) ;

    dialog.setWindowTitle(caption) ;

    if(QDialog::Rejected == dialog.exec())
        return std::set<RsGxsId>() ;

    std::set<RsGxsId> sids ;
    dialog.friends_widget->selectedIds<RsGxsId,UnseenFriendSelectionWidget::IDTYPE_GXS>(sids,false) ;

    return sids ;
}

std::set<GxsChatMember> UnseenFriendSelectionDialog::selectFriends_GxsChatMember(QWidget *parent, const RsGxsGroupId _groupId, const QString &caption, const QString &header_string, UnseenFriendSelectionWidget::Modus modus, UnseenFriendSelectionWidget::ShowTypes show_type, const std::set<RsGxsId> &pre_selected_ids)
{
    std::set<std::string> psids ;
    for(std::set<RsGxsId>::const_iterator it(pre_selected_ids.begin());it!=pre_selected_ids.end();++it)
        psids.insert( (*it).toStdString() ) ;

    UnseenFriendSelectionDialog dialog(parent,header_string,modus,show_type,UnseenFriendSelectionWidget::IDTYPE_GXS_CHAT_MEMBER,psids) ;

    dialog.setWindowTitle(caption) ;
    dialog.resize(450, 600);
    dialog.friends_widget->setGxsGroupId(_groupId);
    dialog.friends_widget->setModeOfFriendList(UnseenFriendSelectionWidget::MODE_INVITE_FRIENDS);

    if(QDialog::Rejected == dialog.exec())
        return std::set<GxsChatMember>() ;

    std::set<GxsChatMember> sids = dialog.friends_widget->selectedList;
    //dialog.friends_widget->selectedIds<RsGxsId,UnseenFriendSelectionWidget::IDTYPE_GXS_CHAT_MEMBER>(sids,false) ;

    return sids ;
}
UnseenFriendSelectionDialog::UnseenFriendSelectionDialog(QWidget *parent,const QString& header_text,
                                                            UnseenFriendSelectionWidget::Modus modus,
                                                            UnseenFriendSelectionWidget::ShowTypes show_type,
                                                            UnseenFriendSelectionWidget::IdType pre_selected_id_type,
                                                            const std::set<std::string>& pre_selected_ids)
	: QDialog(parent)
{
    friends_widget = new UnseenFriendSelectionWidget(this) ;

	friends_widget->setModus(modus) ;
	friends_widget->setShowType(show_type) ;
	friends_widget->start() ;
    friends_widget->setSelectedIds(pre_selected_id_type, pre_selected_ids, false);

	QLayout *l = new QVBoxLayout ;
	setLayout(l) ;
	
	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	l->addWidget(friends_widget) ;
	l->addWidget(buttonBox) ;
	l->update() ;
}

UnseenFriendSelectionDialog::~UnseenFriendSelectionDialog()
{
	delete friends_widget ;
}

