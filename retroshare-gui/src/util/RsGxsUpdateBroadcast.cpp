#include <QMap>

#include "RsGxsUpdateBroadcast.h"
#include "gui/notifyqt.h"

#include <retroshare/rsgxsifacehelper.h>

#define DEBUG_GXS_BROADCAST 1

// previously gxs allowed only one event consumer to poll for changes
// this required a single broadcast instance per service
// now the update notify works through rsnotify and notifyqt
// so the single instance per service is not really needed anymore

QMap<RsGxsIfaceHelper*, RsGxsUpdateBroadcast*> updateBroadcastMap;

RsGxsUpdateBroadcast::RsGxsUpdateBroadcast(RsGxsIfaceHelper *ifaceImpl) :
	QObject(NULL), mIfaceImpl(ifaceImpl)
{
    connect(NotifyQt::getInstance(), SIGNAL(gxsChange(RsGxsChanges)), this, SLOT(onChangesReceived(RsGxsChanges)));
}

void RsGxsUpdateBroadcast::cleanup()
{
	QMap<RsGxsIfaceHelper*, RsGxsUpdateBroadcast*>::iterator it;
	for (it = updateBroadcastMap.begin(); it != updateBroadcastMap.end(); ++it) {
		delete(it.value());
	}

	updateBroadcastMap.clear();
}

RsGxsUpdateBroadcast *RsGxsUpdateBroadcast::get(RsGxsIfaceHelper *ifaceImpl)
{
	QMap<RsGxsIfaceHelper*, RsGxsUpdateBroadcast*>::iterator it = updateBroadcastMap.find(ifaceImpl);
	if (it != updateBroadcastMap.end()) {
		return it.value();
	}

	RsGxsUpdateBroadcast *updateBroadcast = new RsGxsUpdateBroadcast(ifaceImpl);
	updateBroadcastMap.insert(ifaceImpl, updateBroadcast);

	return updateBroadcast;
}

void RsGxsUpdateBroadcast::onChangesReceived(const RsGxsChanges& changes)
{
#ifdef DEBUG_GXS_BROADCAST
    std::cerr << "onChangesReceived()" << std::endl;
    
     {
        std::cerr << "Received changes for service " << (void*)changes.mService << ",  expecting service " << (void*)mIfaceImpl->getTokenService() << std::endl;
        for(std::list<RsGxsGroupId>::const_iterator it(changes.mGrps.begin());it!=changes.mGrps.end();++it)
            std::cerr << "[GRP CHANGE]    grp id: " << *it << std::endl;
        for(std::list<RsGxsGroupId>::const_iterator it(changes.mGrpsMeta.begin());it!=changes.mGrpsMeta.end();++it)
            std::cerr << "[GRP CHANGE]    grp meta: " << *it << std::endl;
        for(std::map<RsGxsGroupId,std::set<RsGxsMessageId> >::const_iterator it2(changes.mMsgs.begin());it2!=changes.mMsgs.end();++it2)
        {
            for(std::set<RsGxsMessageId>::iterator it3=it2->second.begin(); it3!=it2->second.end(); ++it3)

            //for(uint32_t i=0;i<it2->second.size();++i)
                std::cerr << "[MSG CHANGE]    grp id: " << it2->first << ". Msg ID " << (*it3).toStdString() << std::endl;
        }
        for(std::map<RsGxsGroupId,std::set<RsGxsMessageId> >::const_iterator it2(changes.mMsgsMeta.begin());it2!=changes.mMsgsMeta.end();++it2)
        {
            for(std::set<RsGxsMessageId>::iterator it3=it2->second.begin(); it3!=it2->second.end(); ++it3)
            //for(uint32_t i=0;i<it->second.size();++i)
                std::cerr << "[MSG CHANGE]    grp id: " << it2->first << ". Msg Meta " << (*it3).toStdString() << std::endl;
        }
    }
#endif
    if(changes.mService != mIfaceImpl->getTokenService())
    {
       // std::cerr << "(EE) Incorrect service. Dropping." << std::endl;
        
        return;
    }

    if (!changes.mMsgs.empty() || !changes.mMsgsMeta.empty())
    {
        emit msgsChanged(changes.mMsgs, changes.mMsgsMeta);
    }

    if (!changes.mGrps.empty() || !changes.mGrpsMeta.empty())
    {
        emit grpsChanged(changes.mGrps, changes.mGrpsMeta);
    }

    if(!changes.mDistantSearchReqs.empty())
        emit distantSearchResultsChanged(changes.mDistantSearchReqs) ;

    emit changed();
}
