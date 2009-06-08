/***************************************************************************
 *   Copyright (C) 2009 by Andrey Afletdinov                               *
 *   afletdinov@mail.dc.baikal.ru                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifdef WITH_NET

#include <sstream>
#include <algorithm>
#include <functional>
#include "settings.h"
#include "server.h"
#include "client.h"
#include "remoteclient.h"

int FH2RemoteClient::callbackCreateThread(void *data)
{
    if(data)
    {
	reinterpret_cast<FH2RemoteClient *>(data)->ConnectionChat();
	reinterpret_cast<FH2RemoteClient *>(data)->thread.Wait();
	return 0;
    }
    return -1;
}

FH2RemoteClient::FH2RemoteClient()
{
}

void FH2RemoteClient::RunThread(void)
{
    if(!thread.IsRun())
	thread.Create(callbackCreateThread, this);
}

void FH2RemoteClient::ShutdownThread(void)
{
    SetModes(ST_SHUTDOWN);
    DELAY(100);

    if(thread.IsRun()) thread.Kill();

    if(sd)
    {
        packet.Reset();
	packet.SetID(MSG_SHUTDOWN);
	packet.Send(*this);
        Close();
    }
    SetModes(0);
}

int FH2RemoteClient::Logout(void)
{
    packet.Reset();
    packet.SetID(MSG_LOGOUT);
    packet.Push(std::string("logout: lost connection"));

    FH2Server & server = FH2Server::Get();
    server.mutex.Lock();
    server.queue.push_back(std::make_pair(packet, player_id));
    server.mutex.Unlock();

    return -1;
}

int FH2RemoteClient::ConnectionChat(void)
{
    Settings & conf = Settings::Get();
    bool extdebug = 2 < conf.Debug();

    player_color = 0;
    player_race = 0;
    player_name.clear();

    // wait thread id
    if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: wait start thread...";
    while(0 == thread.GetID()){ DELAY(10); };
    player_id = thread.GetID();
    if(extdebug) std::cerr << "ok" << std::endl;

    SetModes(ST_CONNECT);

    // send banner
    std::ostringstream banner;
    banner << "Free Heroes II Server, version: " << static_cast<int>(conf.MajorVersion()) << "." << static_cast<int>(conf.MinorVersion()) << std::endl;

    packet.Reset();
    packet.SetID(MSG_READY);
    packet.Push(banner.str());

    // send ready
    if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", send ready...";
    if(!Send(packet, extdebug)) return Logout();
    if(extdebug) std::cerr << "ok" << std::endl;

    // recv hello
    if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", recv hello...";
    if(!Wait(packet, MSG_HELLO, extdebug)) return Logout();
    if(extdebug) std::cerr << "ok" << std::endl;

    packet.Pop(player_name);
    if(conf.Debug())
        std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", connected " << " player: " << player_name << ", host 0x" << std::hex << Host() << ":0x" << Port() << std::endl;

    // send hello, modes, id
    packet.Reset();
    packet.SetID(MSG_HELLO);
    packet.Push(modes);
    packet.Push(player_id);
    if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", send hello...";
    if(!Send(packet, extdebug)) return Logout();
    if(extdebug) std::cerr << "ok" << std::endl;

    if(Modes(ST_ADMIN))
    {
	if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: admin mode" << std::endl;

	// recv maps info
	if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", recv maps...";
	if(!Wait(packet, MSG_MAPS, extdebug)) return Logout();
	if(extdebug) std::cerr << "ok" << std::endl;

        Maps::FileInfo & fi = conf.CurrentFileInfo();

	if(conf.Modes(Settings::DEDICATEDSERVER))
	    Network::PacketPopMapsFileInfo(packet, fi);

	if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", selected maps: " << fi.name << std::endl;

	// fix for dedicated mode
	conf.SetPreferablyCountPlayers(fi.AllowColorsCount());

	// set allow players
	SetModes(ST_ALLOWPLAYERS);

	// send ready
	packet.Reset();
	packet.SetID(MSG_READY);
	if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", send ready...";
	if(!Send(packet, extdebug)) return Logout();
	if(extdebug) std::cerr << "ok" << std::endl;
    }
    else
    {
	if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: client mode" << std::endl;

	// recv ready
	if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", recv ready...";
	if(!Wait(packet, MSG_READY, extdebug)) return Logout();
	if(extdebug) std::cerr << "ok" << std::endl;

	// send maps
        packet.Reset();
        packet.SetID(MSG_MAPS);
        Network::PacketPushMapsFileInfo(packet, conf.CurrentFileInfo());

        if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat send maps...";
	if(!Send(packet, extdebug)) return Logout();
        if(extdebug) std::cerr << "ok" << std::endl;
    }

    // recv ready
    if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: id: 0x" << std::hex << player_id << ", recv ready...";
    if(!Wait(packet, MSG_READY, extdebug)) return Logout();
    if(extdebug) std::cerr << "ok" << std::endl;

    FH2Server & server = FH2Server::Get();

    // send players colors
    server.mutex.Lock();
    server.StorePlayersInfo(players);
    server.mutex.Unlock();

    packet.Reset();
    packet.SetID(MSG_PLAYERS);
    Network::PacketPushPlayersInfo(packet, players);

    if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat send players colors...";
    if(!Send(packet, extdebug)) return Logout();
    if(extdebug) std::cerr << "ok" << std::endl;

    if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat start queue" << std::endl;
    while(1)
    {
        if(Modes(ST_SHUTDOWN) || packet.GetID() == MSG_LOGOUT) break;

	if(Ready())
	{
            if(extdebug) std::cerr << "FH2RemoteClient::ConnectionChat: recv: ";
	    if(!Recv(packet, extdebug)) return Logout();
            if(extdebug) std::cerr << Network::GetMsgString(packet.GetID()) << std::endl;

	    // msg put to queue
	    if(MSG_UNKNOWN != packet.GetID())
	    {
		server.mutex.Lock();
		server.queue.push_back(std::make_pair(packet, player_id));
	        server.mutex.Unlock();
	    }

    	    // msg post processing
    	    switch(packet.GetID())
    	    {
    		case MSG_UNKNOWN:
    		{
    		    packet.Dump();
    		    break;
    		}

    		case MSG_PLAYERS:
    		{
		    Network::PacketPopPlayersInfo(packet, players);
		    server.mutex.Lock();
		    conf.SetPlayersColors(Network::GetPlayersColors(players));
		    server.mutex.Unlock();
		    std::vector<Player>::const_iterator itp = std::find_if(players.begin(), players.end(), std::bind2nd(std::mem_fun_ref(&Player::isID), player_id));
		    if(itp != players.end())
		    {
			player_name = (*itp).player_name;
		        player_color = (*itp).player_color;
		        player_race = (*itp).player_race;
		    }
    		    break;
		}

    		case MSG_MAPS:
		{
		    std::fstream fs("test.sav", std::ios::out | std::ios::binary);
	    	    if(fs.good())
	    	    {
	    		u8 ch;
			while(packet.Pop(ch)) fs.put(static_cast<char>(ch));
			fs.close();
		    }
		    packet.Reset();
		    break;
		}

    		default:
    		    break;
    	    }
	}

        DELAY(10);
    }
    
    Close();
    modes = 0;

    return 0;
}

#endif