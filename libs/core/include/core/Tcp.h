#pragma once

#include "core/Base.h"

//@TODO: get rid of this public dependency on SDL (pimpl or polymorphic type)
#include <SDL_net.h>

class TcpServer {
public:
    ~TcpServer() { Close(); }

    void Open(uint16_t port) {
        m_port = port;
        if (SDLNet_ResolveHost(&m_ip, NULL, port) == -1)
            FAIL();
        m_server = SDLNet_TCP_Open(&m_ip);
        if (!m_server)
            FAIL();
    }

    void Close() {
        if (m_server) {
            SDLNet_TCP_Close(m_server);
            m_server = {};
        }
        if (m_client) {
            SDLNet_TCP_Close(m_client);
            m_client = {};
        }
        m_ip = {};
        m_port = {};
    }

    bool TryAccept() {
        m_client = SDLNet_TCP_Accept(m_server);
        return m_client != nullptr;
    }

    template <typename T>
    bool Send(const T& value) {
        auto bytesSent = SDLNet_TCP_Send(m_client, &value, sizeof(T));
        return bytesSent <= sizeof(T);
    }

    template <typename T>
    bool Receive(T& value) {
        auto size = SDLNet_TCP_Recv(m_client, &value, sizeof(T));
        return size == sizeof(T);
    }

private:
    TCPsocket m_server{};
    TCPsocket m_client{};
    IPaddress m_ip{};
    uint16_t m_port{};
};

class TcpClient {
public:
    ~TcpClient() { Close(); }

    void Open(const char* ipAddress, uint16_t port) {
        if (SDLNet_ResolveHost(&m_ip, ipAddress, port) == -1)
            FAIL();
        m_socket = SDLNet_TCP_Open(&m_ip);
        if (!m_socket)
            FAIL();
    }

    void Close() {
        if (m_socket) {
            SDLNet_TCP_Close(m_socket);
            m_socket = {};
        }
        m_ip = {};
    }

    template <typename T>
    bool Send(const T& value) {
        auto bytesSent = SDLNet_TCP_Send(m_socket, &value, sizeof(T));
        return bytesSent <= sizeof(T);
    }

    template <typename T>
    bool Receive(T& value) {
        auto size = SDLNet_TCP_Recv(m_socket, &value, sizeof(T));
        return size == sizeof(T);
    }

private:
    TCPsocket m_socket{};
    IPaddress m_ip{};
};
