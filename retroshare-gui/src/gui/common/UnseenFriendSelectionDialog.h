#pragma once

#include <QDialog>
#include <retroshare/rstypes.h>
#include <retroshare/rsidentity.h>
#include <gui/common/UnseenFriendSelectionWidget.h>

class UnseenFriendSelectionDialog : public QDialog
{
	public:
        static std::set<RsPgpId> selectFriends_PGP(QWidget *parent,const QString& caption,const QString& header_string,
                                UnseenFriendSelectionWidget::Modus  modus   = UnseenFriendSelectionWidget::MODUS_MULTI,
                                UnseenFriendSelectionWidget::ShowTypes = UnseenFriendSelectionWidget::SHOW_GROUP,
                                const std::set<RsPgpId>& pre_selected_ids = std::set<RsPgpId>()) ;

        static std::set<RsPeerId> selectFriends_SSL(QWidget *parent,const QString& caption,const QString& header_string,
                                UnseenFriendSelectionWidget::Modus  modus   = UnseenFriendSelectionWidget::MODUS_MULTI,
                                UnseenFriendSelectionWidget::ShowTypes = UnseenFriendSelectionWidget::SHOW_GROUP | UnseenFriendSelectionWidget::SHOW_SSL,
                                const std::set<RsPeerId>& pre_selected_ids = std::set<RsPeerId>()) ;

        static std::set<RsGxsId> selectFriends_GXS(QWidget *parent,const QString& caption,const QString& header_string,
                                UnseenFriendSelectionWidget::Modus  modus   = UnseenFriendSelectionWidget::MODUS_MULTI,
                                UnseenFriendSelectionWidget::ShowTypes = UnseenFriendSelectionWidget::SHOW_GROUP | UnseenFriendSelectionWidget::SHOW_GXS,
                                const std::set<RsGxsId>& pre_selected_ids = std::set<RsGxsId>()) ;
 
        static std::set<GxsChatMember> selectFriends_GxsChatMember(QWidget *parent, const RsGxsGroupId _groupId, const QString& caption,const QString& header_string,
                                                                   UnseenFriendSelectionWidget::Modus  modus   = UnseenFriendSelectionWidget::MODUS_MULTI,
                                                                   UnseenFriendSelectionWidget::ShowTypes = UnseenFriendSelectionWidget::SHOW_GROUP | UnseenFriendSelectionWidget::SHOW_GXS,
                                                                   const std::set<RsGxsId>& pre_selected_ids = std::set<RsGxsId>()) ;
    private:
        virtual ~UnseenFriendSelectionDialog() ;
        UnseenFriendSelectionDialog(QWidget *parent,const QString& header_string,UnseenFriendSelectionWidget::Modus modus,UnseenFriendSelectionWidget::ShowTypes show_type,
                                                                    UnseenFriendSelectionWidget::IdType pre_selected_id_type,
                                                                    const std::set<std::string>& pre_selected_ids) ;

        UnseenFriendSelectionWidget *friends_widget ;
};

