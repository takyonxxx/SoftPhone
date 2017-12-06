#include <cstdlib>
#include <iostream>
#include <sstream>
#include <boost/thread/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include "TCPCommunicator.h"



//nc 127.0.0.1 15000
TCPCommunicator::TCPCommunicator(boost::asio::io_service& io_service, const char *localAddress,
                                 const char *localPort,
                                 const char *remoteAddress,
                                 const char *remotePort)
: m_ioService(io_service),
  m_socket(io_service),
  _strand(io_service)
{
    strcpy(m_localAddress, localAddress);
    strcpy(m_localPort, localPort);
    strcpy(m_remoteAddress,remoteAddress);
    strcpy(m_remotePort,remotePort);   

    startListen();

    ///client////
    /*try {
        boost::asio::ip::tcp::resolver resolver(m_ioService);
        boost::asio::ip::tcp::resolver::query query(m_remoteAddress, m_remotePort);
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

        boost::asio::async_connect(m_socket, endpoint_iterator,
                boost::bind(&TCPCommunicator::handleConnect, this,
                        boost::asio::placeholders::error));
    } catch (std::exception &ex) {
    }*/

    printf("TCPCommunicator Server has been start.\nListening on Port: %s\n",m_remotePort);

}

TCPCommunicator::~TCPCommunicator()
{
}

void TCPCommunicator::startListen()
{
   // m_socket.reset(new boost::asio::ip::tcp::socket(m_ioService));
    m_acceptor.reset(new boost::asio::ip::tcp::acceptor(m_ioService,
            boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::tcp::v4(),
                    atoi(m_remotePort)
    )
    )
    );

    try
    {        
          m_acceptor->async_accept(m_socket, boost::bind(&TCPCommunicator::handleListen, this, boost::asio::placeholders::error));
    } catch (std::exception &ex) {
    }

    boost::thread thread(boost::bind(&boost::asio::io_service::run, &m_ioService));

}

void TCPCommunicator::Uinit()
{   
    m_socket.cancel();
    m_socket.close();
    m_ioService.stop();

    printf("TCPCommunicator has been stop...\n");
}

void TCPCommunicator::handleConnect(const boost::system::error_code& error)
{
    if (!error)
    {

    }
    else
    {
        std::cerr << "could not connect: " << boost::system::system_error(error).what() << std::endl;
    }

}

void TCPCommunicator::handleListen(const boost::system::error_code& error)
{
    if (!error)
    {
        boost::asio::ip::tcp::no_delay option(true);
        m_socket.set_option(option);
        read();
    }
    else
    {
         std::cerr << "could not listen: " << boost::system::system_error(error).what() << std::endl;
    }

}

// Reading messages from the server
void TCPCommunicator::read()
{
    m_readBuffer.assign(0);
    boost::asio::async_read(m_socket,boost::asio::buffer(m_readBuffer),
        boost::asio::transfer_at_least(1),
        boost::bind(&TCPCommunicator::handleRead, this,
        boost::asio::placeholders::error));
}

void TCPCommunicator::write()
{
    const std::string& message = _outbox[0];

    boost::asio::async_write(
            m_socket,
            boost::asio::buffer( message.c_str(), message.size() ),
            _strand.wrap(
                boost::bind(
                    &TCPCommunicator::handleWrite,
                    this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred
                    )
                )
            );
}

void TCPCommunicator::write(const char* message)
{
    m_writeBuffer = message;
    m_readBuffer.assign(0);
    _strand.post(
            boost::bind(
                &TCPCommunicator::writeImpl,
                this,
                message
                )
            );
}


void TCPCommunicator::writeImpl(const char* message)
{
    _outbox.push_back( message );
    if ( _outbox.size() > 1 ) {
        // outstanding async_write
        return;
    }

    this->write();
}


void TCPCommunicator::handleRead(const boost::system::error_code& error)
{
    if (error)
    {
        std::cerr << "could not read: " << boost::system::system_error(error).what() << std::endl;
        return;
    }

    unsigned char *receivedData = (unsigned char *)m_readBuffer.data();
     m_sizeOfData = strlen((char*)receivedData);

    printf("TCPCommunicator: Received bytes size(%d) :",(int)m_sizeOfData);
    for (unsigned int i = 0; i < m_sizeOfData; i++)
    {
        printf(" %02X", receivedData[i]);
    }
    printf("\n");
    printf("TCPCommunicator: Received char  size(%d) : %s\n",(int)m_sizeOfData,receivedData);

    read();
    //write("hello");

}

void TCPCommunicator::handleWrite(const boost::system::error_code& error,const size_t bytes_transferred)
{
    _outbox.pop_front();

    if (error)
    {
        std::cerr << "could not write: " << boost::system::system_error(error).what() << std::endl;
        return;
    }

    if ( !_outbox.empty() ) {
        // more messages to send
        this->write();
    }

    printf("TCPCommunicator: Write bytes size(%d) :",(int)bytes_transferred);
    for (unsigned int i = 0; i < bytes_transferred; i++)
    {
        printf(" %02X", m_writeBuffer[i]);
    }
    printf("\n");

    printf("TCPCommunicator: Write char  size(%d) : %s\n" ,(int)bytes_transferred,m_writeBuffer);
}

