#pragma once

#include <vector> //std::vector
#include <memory> //std::shared_ptr
#include <thread> //std::thread
#include <iostream> //For debugging

#include <boost/asio.hpp> //networking, io_service
#include <boost/bind.hpp> //boost::bind
#include <boost/signals2.hpp>

#include "LightNode.hpp"


//Forward declaration of friend class
class Rhopalia;

class LightHub
{
public:
	enum DiscoveryMethod_e {
		SWEEP,
		BROADCAST
	};

	enum ListenerType_e {
		NODE_DISCOVER
	};

	//Exception codes
	static const uint16_t LIGHT_HUB_NODE_NOT_FOUND = 1;

	LightHub(uint16_t sendPort, uint16_t recvPort, DiscoveryMethod_e,
		uint32_t discoverPeriod = 1000);
	~LightHub();

	template<class T>
	void addListener(ListenerType_e listenType, T slot) {
		if(listenType == NODE_DISCOVER) {
			sigNodeDiscover.connect(slot);
		}
		else {
			//This shouldn't happen
			std::cout << "[Error] LightNode::addListener: Invalid listener type"
				<< std::endl;
		}
	}

	void discover(DiscoveryMethod_e);

	std::shared_ptr<LightNode> getNodeByName(const std::string&);

	std::shared_ptr<LightNode> getNodeByAddress(const boost::asio::ip::address&);

	std::vector<std::shared_ptr<LightNode>>::iterator begin();
	std::vector<std::shared_ptr<LightNode>>::iterator end();

	size_t getNodeCount() const;

	size_t getConnectedNodeCount() const;

private:
	friend class Rhopalia;

	//Function to update all nodes in vector
	void updateLights();

	//Thread to run io_service.run()
	void threadRoutine();

	//Starts an async read on the UDP socket
	void startListening();

	//Callbacks for network operations
	void handleSendBroadcast(const boost::system::error_code&,
		size_t bytesTransferred);

	void handleReceive(const boost::system::error_code&,
		size_t bytesTransferred);

	//Callback for discovery timer
	void handleDiscoveryTimer(const boost::system::error_code&);

	//Signals
	boost::signals2::signal<void(std::shared_ptr<LightNode>)> sigNodeDiscover;

	//Vector of nodes
	//That have at one point responded
	std::vector<std::shared_ptr<LightNode>> nodes;

	//Thread stuff
	boost::asio::io_service ioService;
	std::unique_ptr<boost::asio::io_service::work> ioWork;
	std::thread asyncThread;

	//Network stuff
	boost::asio::ip::udp::socket sendSocket, recvSocket;
	boost::asio::ip::udp::endpoint receiveEndpoint;
	uint16_t sendPort, recvPort;
	std::array<uint8_t, 512> readBuffer;

	//Autodiscovery stuff
	boost::asio::deadline_timer discoveryTimer;
	DiscoveryMethod_e discoveryMethod;
	uint32_t discoveryPeriod;


};
