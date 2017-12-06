#include <cstdlib>
#include <deque>
#include <iostream>
#include <sstream>
#include <boost/thread/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/crc.hpp>
#include <vector>
#include <string.h>

#include "UDPCommunicator.h"
#include "Utility.h"

//netcat -u -l 15001

UDPCommunicator::UDPCommunicator(boost::asio::io_service& io_service,
         const char *localAddress,
         const char *localPort,
         const char *remoteAddress,
         const char *remotePort
         ) :
		m_ioService(io_service),
        m_socket(io_service)

{
    strcpy(m_localAddress, localAddress);
    strcpy(m_localPort, localPort);
    strcpy(m_remoteAddress,remoteAddress);
    strcpy(m_remotePort,remotePort);

    boost::system::error_code error;
    boost::asio::ip::udp::resolver resolveLocal(m_ioService);
    boost::asio::ip::udp::resolver::query queryLocal(boost::asio::ip::udp::v4(), m_localAddress, m_localPort);
    m_localEndpoint = *resolveLocal.resolve(queryLocal);
    m_socket.open(m_localEndpoint.protocol());
    m_socket.set_option(boost::asio::socket_base::reuse_address(true), error);
    m_socket.bind(m_localEndpoint);

    boost::asio::ip::udp::resolver resolver(m_ioService);
    boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), m_remoteAddress, m_remotePort);
    m_remoteSendEndpoint = *resolver.resolve(query);

    read();

    boost::thread thread(boost::bind(&boost::asio::io_service::run, &m_ioService));
    printf("UDPCommunicator has been start.\nlocalAddress: %s/%s  remoteAddress: %s/%s\n",
           m_localAddress,m_localPort,m_remoteAddress,m_remotePort);

}

UDPCommunicator::~UDPCommunicator()
{
}

void UDPCommunicator::Uinit() {

    m_socket.cancel();
    m_socket.close();
    m_ioService.stop();

    printf("UDPCommunicator has been stop...\n");
}

void UDPCommunicator::read()
{
    m_readBuffer.assign(0);
    m_socket.async_receive(
            boost::asio::buffer(m_readBuffer),
            boost::bind(&UDPCommunicator::handleRead, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
    );
}

void UDPCommunicator::handleRead(const boost::system::error_code& error, std::size_t bytes_transferred) {

    if (!error || error == boost::asio::error::message_size)
    {
        unsigned char *receivedData = (unsigned char *)m_readBuffer.data();

        printf("UDPCommunicator: Received bytes size(%d) :",(int)bytes_transferred);
        for (unsigned int i = 0; i < bytes_transferred; i++)
        {
            printf(" %02X", receivedData[i]);
        }
        printf("\n");

        printf("UDPCommunicator: Received char  size(%d) : %s\n",(int)bytes_transferred,receivedData);
        read();

    }
    else
    {
        printf("handleRead() Error: %s\n",error.message().c_str());
    }
}

void UDPCommunicator::handleWrite(const boost::system::error_code& error, std::size_t bytes_transferred) {

    if (!error)
    {
        printf("UDPCommunicator: Write bytes size(%d) :\n",(int)bytes_transferred);
        for (unsigned int i = 0; i < bytes_transferred; i++)
        {
            if(i>0)printf(":");
            printf("%02X", m_writeBuffer[i]);
        }
        printf("\n");

        //printf("UDPCommunicator: Write char  size(%d) : %s\n" ,(int)bytes_transferred,m_writeBuffer);
    }
    else
    {
        printf("handleWrite() Packet Error: %s\n",error.message().c_str());
    }
}

void UDPCommunicator::write(const char* m_WillBeWritten,int size)
{
    if(size == 0)
        m_sizeOfData = strlen((char*)m_WillBeWritten);
    else
        m_sizeOfData = size;

    m_writeBuffer = m_WillBeWritten;
    m_socket.async_send_to(
            boost::asio::buffer(m_writeBuffer,m_sizeOfData),
            m_remoteSendEndpoint,
            boost::bind(&UDPCommunicator::handleWrite, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
    );
}
