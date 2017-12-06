#ifndef UDP_COMMUNICATOR_H_
#define UDP_COMMUNICATOR_H_

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <deque>
#include <boost/asio/deadline_timer.hpp>

#define SIZE_OF_IP 16
#define SIZE_OF_PORT 8
#define ENVELOPE_DATA_SIZE 256
#define MAX_ENVELOPE_SIZE 512

class UDPCommunicator {

private:
    char m_localAddress[SIZE_OF_IP];
    char m_localPort[SIZE_OF_PORT];

    char m_remoteAddress[SIZE_OF_IP];
    char m_remotePort[SIZE_OF_PORT];

	boost::asio::io_service& m_ioService;
	boost::asio::ip::udp::socket m_socket;	
    boost::asio::ip::udp::endpoint m_localEndpoint;
    boost::asio::ip::udp::endpoint m_remoteSendEndpoint;
    boost::asio::ip::udp::endpoint m_remoteReceiveEndpoint;
    boost::array<char, MAX_ENVELOPE_SIZE> m_readBuffer;
    const char* m_writeBuffer;
    size_t m_sizeOfData;
private:

    void handleRead(const boost::system::error_code& error, std::size_t bytes_transferred);
    void handleWrite(const boost::system::error_code& error, std::size_t bytes_transferred);
    void read();

public:

    void Uinit();
    void write(const char* m_WillBeWritten,int size);
    UDPCommunicator(boost::asio::io_service& io_service, const char *localAddress,
            const char *localPort, const char *remoteAddress,	const char *remotePort);

    virtual ~UDPCommunicator();
};

#endif /* UDP_GROUND_RC_COMMUNICATOR_H_ */
