#include "LightHub.hpp"
#include "debug.h"

using namespace std;

LightHub::LightHub(uint16_t _sendPort, uint16_t _recvPort, DiscoveryMethod_e _discoveryMethod,
	uint32_t _discoveryPeriod)
	:	sendSocket(ioService, boost::asio::ip::udp::v4())
	,	recvSocket(ioService, boost::asio::ip::udp::endpoint(
			boost::asio::ip::udp::v4(), _recvPort))
	,	discoveryTimer(ioService)	{
	sendPort = _sendPort;
	recvPort = _recvPort;
	discoveryMethod = _discoveryMethod;
	discoveryPeriod = _discoveryPeriod;

	//Allow the socket to send broadcast packets
	sendSocket.set_option(boost::asio::socket_base::broadcast(true));
	sendSocket.set_option(boost::asio::socket_base::reuse_address(true));

	recvSocket.set_option(boost::asio::socket_base::reuse_address(true));
	
	//Construct the work unit for the io_service
	ioWork.reset(new boost::asio::io_service::work(ioService));

	//Start the async thread
	asyncThread = std::thread(std::bind(&LightHub::threadRoutine, this));

	//Start listening for packets
	startListening();

	debug << "Now listening for packets" << std::endl;

	debug << "Performing initial network discovery" << std::endl;

	//Do an initial node discovery
	discover(discoveryMethod);

	//Start the timer for subsequent node discovery
	discoveryTimer.expires_from_now(
		boost::posix_time::milliseconds(discoveryPeriod));
	
	discoveryTimer.async_wait(std::bind(&LightHub::handleDiscoveryTimer, this,
		std::placeholders::_1));
}

LightHub::~LightHub() {
	//Delete the work unit, allow io_service.run() to return
	ioWork.reset();

	//Wait for the async thread to complete
	asyncThread.join();
}

void LightHub::threadRoutine() {
	//Run all async tasks
	ioService.run();

	debug << "Thread closing" << std::endl;
}


void LightHub::discover(LightHub::DiscoveryMethod_e method) {

	switch(method) {
		case SWEEP:
			err << "discover method 'SWEEP' "
				<< "not implemented" << std::endl;
		break;

		case BROADCAST:
		{
			boost::asio::ip::udp::endpoint endpoint(
				boost::asio::ip::address_v4::broadcast(), sendPort);

			//Send ping broadcast packet
			sendSocket.async_send_to(boost::asio::buffer(
				Packet::Ping().asDatagram()),	//Construct ping packet
				endpoint,	//Broadcast endpoint
				boost::bind(&LightHub::handleSendBroadcast, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
		}
		break;
	}
}

std::shared_ptr<LightNode> LightHub::getNodeByName(const std::string& name) {
	
	auto found = std::find_if(std::begin(nodes), std::end(nodes),
		[&name](const std::shared_ptr<LightNode>& node) {
			return node->getName() == name;
		});
	
	if(found == std::end(nodes)) {
		//We didn't find a node
		throw Exception(LIGHT_HUB_NODE_NOT_FOUND, "LightHub::getNodeByName: "
			"node not found");
	}
	else {
		return *found;
	}
}

std::shared_ptr<LightNode> LightHub::getNodeByAddress(
	const boost::asio::ip::address& addr) {

	auto found = std::find_if(std::begin(nodes), std::end(nodes),
		[&addr](const std::shared_ptr<LightNode>& node) {
			return node->getAddress() == addr;
		});

	if(found == std::end(nodes)) {
		//We didn't find a node
		throw Exception(LIGHT_HUB_NODE_NOT_FOUND, "LightHub::getNodeByAddress: "
			"node not found");
	}
	else {
		return *found;
	}
}

std::vector<std::shared_ptr<LightNode>>::iterator LightHub::begin() {
	return nodes.begin();
}

std::vector<std::shared_ptr<LightNode>>::iterator LightHub::end() {
	return nodes.end();
}

size_t LightHub::getNodeCount() const {
	return nodes.size();
}

size_t LightHub::getConnectedNodeCount() const {
	size_t connectedCount = 0;

	for(auto& node : nodes) {
		connectedCount += node->getState() == LightNode::CONNECTED;
	}

	return connectedCount;
}

void LightHub::handleSendBroadcast(const boost::system::error_code& ec,
	size_t) {
	if(ec) {
		err << "Failed to send broadcast ping message: "
			<< ec.message() << std::endl;
	}
}

void LightHub::handleDiscoveryTimer(const boost::system::error_code& ec) {
	if(ec) {
		err << ec.message() << std::endl;
	}
	else {
		//Perform discovery
		discover(discoveryMethod);
	}

	//Reset timer
	discoveryTimer.expires_from_now(
		boost::posix_time::milliseconds(discoveryPeriod));
	discoveryTimer.async_wait(std::bind(&LightHub::handleDiscoveryTimer, this,
		std::placeholders::_1));
}

void LightHub::startListening() {
	//Start the async receive
	recvSocket.async_receive_from(boost::asio::buffer(readBuffer),
		receiveEndpoint,
		boost::bind(&LightHub::handleReceive, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void LightHub::handleReceive(const boost::system::error_code& ec,
	size_t bytesTransferred) {
	
	if(ec) {
		err << "Failed to receive from UDP socket" << std::endl;
	}
	else {
		Packet p;

		try {
			//Parse the datagram into a packet
			p = Packet(std::vector<uint8_t>(std::begin(readBuffer),
				std::begin(readBuffer) + bytesTransferred));
		}
		catch(const Exception& e) {
			if(e.getErrorCode() == Packet::PACKET_INVALID_HEADER ||
					e.getErrorCode() == Packet::PACKET_INVALID_SIZE) {
				//Might be from some other application, we can ignore
				warning << "Invalid datagram "
					"received from " << receiveEndpoint.address() << std::endl;
			}
			else {
				//Weird!
				err << "Exception caught: "
					<< e.what() << std::endl;
			}
		}

		std::shared_ptr<LightNode> sendNode;

		//try to find the node associated with this packet
		try {
			sendNode = getNodeByAddress(receiveEndpoint.address());

			//Let the node handle the packet
			sendNode->receivePacket(p);
		}
		catch(const Exception& e) {
			if(e.getErrorCode() == LIGHT_HUB_NODE_NOT_FOUND) {
					//The sender is not in the list of connected nodes
					
					if(p.getID() == Packet::INFO) {
					//TODO: Give the new node a unique name
					//TODO: Have the node send its set name along with other parameters
					std::string name = receiveEndpoint.address().to_string();
					LightNode::Type_e type = static_cast<LightNode::Type_e>(p.getPayload()[0]);
					uint16_t ledCount = (p.getPayload()[1] << 8) | (p.getPayload()[2]);
					
					auto newNode = std::make_shared<LightNode>(name, type, ledCount,
						receiveEndpoint.address(), sendPort);

					//Store the new node
					nodes.push_back(newNode);

					//Send a signal
					sigNodeDiscover(newNode);
				}
				else {
					cout << "[Warning] Received packet from unconnected device with ID " << p.getID() << endl;
				}
			}
			else {
				//Some other error occurred
				err << "Exception thrown: "
					<< e.what() << std::endl;
			}
		}
	}

	startListening();
}

void LightHub::updateLights() {
	for(auto& node : nodes) {
		node->update();
	}
}
