/*
 * RetroShare Service
 * Copyright (C) 2016-2018  Gioacchino Mazzurco <gio@eigenlab.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <set>

#include <QTimer>
#include <QFile>
#include <QTcpServer>
#include <QCoreApplication>
#include <QObject>
#include <iostream>

#include "util/rstime.h"
#include "util/stacktrace.h"
#include "util/rsdir.h"

CrashStackTrace gCrashStackTrace;

//#include <QStringList>

#include "TorControl/TorManager.h"
#include "TorControl/TorControlConsole.h"

#include "retroshare/rsidentity.h"
#include "retroshare/rspeers.h"
#include "retroshare/rsinit.h"
#include "retroshare/rsiface.h"


#ifdef __ANDROID__
#	include <QAndroidService>
#endif // def __ANDROID__

#include "rsserver/rsaccounts.h"
//#include "pqi/authssl.h"
//#include "pqi/sslfns.h"
#include "pqi/authgpg.h"


#ifdef __ANDROID__
#	include "util/androiddebug.h"
#endif

//#ifndef RS_JSONAPI
//#	error Inconsistent build configuration retroshare_service needs rs_jsonapi
//#endif

int main(int argc, char* argv[])
{
#ifdef __ANDROID__
	AndroidStdIOCatcher dbg; (void) dbg;
	QAndroidService app(argc, argv);
#else // def __ANDROID__
	QCoreApplication app(argc, argv);
#endif // def __ANDROID__

	signal(SIGINT,   QCoreApplication::exit);
	signal(SIGTERM,  QCoreApplication::exit);
#ifdef SIGBREAK
	signal(SIGBREAK, QCoreApplication::exit);
#endif // ifdef SIGBREAK

	RsInit::InitRsConfig();

    //check to see if the service want to want to run hidden_mode=TOR
    bool is_auto_tor = false;
    for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--tor")
                is_auto_tor = true;
    }

    int initResult;
	// clumsy way to enable JSON API by default
	if(!QCoreApplication::arguments().contains("--jsonApiPort"))
	{
		int argc2 = argc + 2;
		char* argv2[argc2]; for (int i = 0; i < argc; ++i ) argv2[i] = argv[i];
		char opt[] = "--jsonApiPort";
		char val[] = "9092";
		argv2[argc] = opt;
		argv2[argc+1] = val;
        initResult = RsInit::InitRetroShare(argc2, argv2, true);
	}
    else {
       initResult= RsInit::InitRetroShare(argc, argv, true);
    }


    //1. Set location to HID_XXX
    //2. Generate Tor Hidden Services
    //3. Launch Tor Embed or Bunble Process
    //4. Attach Tor socket/TorControl in to rsPeer-->Proxy.

    if(is_auto_tor)
    {
        // Now that we know the Tor service running, and we know the SSL id, we can make sure it provides a viable hidden service
        //create a HID_XXX Account
        RsPgpId pgp_id("C4466A0E5438F17D");  //fake pgp_id
        std::string passwd("iloveu!!");
        RsPeerId sslId("42eedef688cda0b38bb19c4078d99837"); //fake sslid
        std::string errorMessage;
        std::string passphrase;
        int keynumbits=2048;
        std::string email("sp@unseen.is");
        std::string name("spnodeUSA");
        std::string genLoc("USA");


        bool makeHidden = true;
        bool makeAutoTor = true;

        bool is_pgpid =  AuthGPG::getAuthGPG()->GeneratePGPCertificate(name, email, passphrase, pgp_id, keynumbits, errorMessage);
        bool okGen = RsAccounts::createNewAccount(pgp_id, "", genLoc, "", makeHidden, makeHidden, passwd, sslId, errorMessage);

        //fake initialized
        RsLoginHelper::Location location;
        location.mLocationId = sslId;
        location.mPgpId = pgp_id;
        location.mLocationName=name;
        location.mPpgName="TestSpnode";

        
	 bool ret = rsLoginHelper->createLocation(location, passwd, errorMessage, makeHidden,makeAutoTor );

        if (!ret){
            std::cerr <<"Failed to createLocation C) "<<std::endl;
            return 0;
        }
	
        // setting hidden service
        QString tor_hidden_service_dir = QString::fromStdString(RsAccounts::AccountDirectory()) + QString("/hidden_service/") ;
        QString rs_baseDir = QString::fromStdString(RsAccounts::ConfigDirectory()) + QString("/tor/");

        Tor::TorManager *torManager = Tor::TorManager::instance();
        torManager->setTorDataDirectory(rs_baseDir);
        torManager->setHiddenServiceDirectory(tor_hidden_service_dir);	// re-set it, because now it's changed to the specific location that is run

        RsDirUtil::checkCreateDirectory(std::string(tor_hidden_service_dir.toUtf8())) ;
        torManager->setupHiddenService();

        //launch Tor process
        if(! torManager->start() || torManager->hasError())
        {
            std::cerr<< "Cannot start Tor Manager! \\n and Tor cannot be started on your system: "<< torManager->errorMessage().toStdString() << std::endl ;
            return 1 ;
        }

        {
            TorControlConsole tcd(torManager, NULL) ;
            QString error_msg ;

            while(tcd.checkForTor(error_msg) != TorControlConsole::TOR_STATUS_OK || tcd.checkForHiddenService() != TorControlConsole::HIDDEN_SERVICE_STATUS_OK)	// runs until some status is reached: either tor works, or it fails.
            {
                QCoreApplication::processEvents();
                rstime::rs_usleep(0.2*1000*1000) ;

                if(!error_msg.isNull())
                {
                    std::cerr <<"Cannot start Tor \n Sorry but Tor cannot be started on your system!\n\nThe error reported is:"<< error_msg.toStdString() <<std::endl;
                    return 1;
                }
            }

            if(tcd.checkForHiddenService() != TorControlConsole::HIDDEN_SERVICE_STATUS_OK)
            {
                std::cerr <<"Cannot start a hidden tor service!\n It was not possible to start a hidden service.";
                return 1 ;
            }
        }
        //attach torproxy or sock5 to rsPeers.
        // Tor works with viable hidden service. Let's use it!

        QString service_id ;
        QString onion_address ;
        uint16_t service_port ;
        uint16_t service_target_port ;
        uint16_t proxy_server_port ;
        QHostAddress service_target_address ;
        QHostAddress proxy_server_address ;

        torManager->getHiddenServiceInfo(service_id,onion_address,service_port,service_target_address,service_target_port);
        torManager->getProxyServerInfo(proxy_server_address,proxy_server_port) ;

        std::cerr << "Got hidden service info: " << std::endl;
        std::cerr << "  onion address  : " << onion_address.toStdString() << std::endl;
        std::cerr << "  service_id     : " << service_id.toStdString() << std::endl;
        std::cerr << "  service port   : " << service_port << std::endl;
        std::cerr << "  target port    : " << service_target_port << std::endl;
        std::cerr << "  target address : " << service_target_address.toString().toStdString() << std::endl;

        std::cerr << "Setting proxy server to " << service_target_address.toString().toStdString() << ":" << service_target_port << std::endl;

        rsPeers->setLocalAddress(rsPeers->getOwnId(), service_target_address.toString().toStdString(), service_target_port);
        rsPeers->setHiddenNode(rsPeers->getOwnId(), onion_address.toStdString(), service_port);
        rsPeers->setProxyServer(RS_HIDDEN_TYPE_TOR, proxy_server_address.toString().toStdString(),proxy_server_port) ;


    }

	RsControl::earlyInitNotificationSystem();
	rsControl->setShutdownCallback(QCoreApplication::exit);
	QObject::connect(
	            &app, &QCoreApplication::aboutToQuit,
	            [](){
		if(RsControl::instance()->isReady())
			RsControl::instance()->rsGlobalShutDown(); } );

	return app.exec();
}