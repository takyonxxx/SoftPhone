
#ifndef TCP_SERVER_COMMUNICATOR_H_
#define TCP_SERVER_COMMUNICATOR_H_

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <deque>
#include <boost/asio/deadline_timer.hpp>
#include <boost/scoped_ptr.hpp>

#define SIZE_OF_IP 16
#define SIZE_OF_PORT 8
#define ENVELOPE_DATA_SIZE 256
#define MAX_ENVELOPE_SIZE 512

class TCPCommunicator
{

private:
    typedef std::deque<std::string> Outbox;

private:

    char m_localAddress[SIZE_OF_IP];
    char m_localPort[SIZE_OF_PORT];

    char m_remoteAddress[SIZE_OF_IP];
    char m_remotePort[SIZE_OF_PORT];

    boost::asio::io_service& m_ioService;
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::io_service::strand _strand;

    /*boost::asio::io_service&  m_ioService;
    boost::scoped_ptr<boost::asio::ip::tcp::socket> m_socket;*/
    boost::scoped_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;

    boost::array<char, MAX_ENVELOPE_SIZE> m_readBuffer;
    size_t m_sizeOfData;
    const char* m_writeBuffer;
    Outbox _outbox;

private:
    void handleListen(const boost::system::error_code& error);
    void handleRead(const boost::system::error_code& error);

    void handleConnect(const boost::system::error_code& error);

    void startListen();
    void read();

    void handleWrite(const boost::system::error_code& error,const size_t bytes_transferred);
    void write();
    //void writeImpl(const std::string& message);
    void writeImpl(const char* message);

public:

    TCPCommunicator(boost::asio::io_service& io_service, const char *localAddress,
            const char *localPort, const char *remoteAddress,	const char *remotePort);

    virtual ~TCPCommunicator(); 
    void Uinit();
    void write(const char* message);
};

#endif /* TCP_SERVER_COMMUNICATOR_H_ */
