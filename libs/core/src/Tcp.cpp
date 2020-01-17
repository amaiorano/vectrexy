#include "core/Tcp.h"

#if defined(ENGINE_NULL)

class TcpServerImpl {};
TcpServer::TcpServer() = default;
TcpServer::~TcpServer() = default;
void TcpServer::Open(uint16_t /*port*/) {}
void TcpServer::Close() {}
bool TcpServer::TryAccept() {
    return false;
}
bool TcpServer::Connected() const {
    return false;
}
bool TcpServer::ReceiveDataAvailable(uint32_t /*timeoutMS*/) const {
    return false;
}
int TcpServer::Send(const void* /*data*/, int /*len*/) {
    return 0;
}
int TcpServer::Receive(void* /*data*/, int /*maxlen*/) {
    return 0;
    ;
}

class TcpClientImpl {};
TcpClient::TcpClient() = default;
TcpClient::~TcpClient() = default;
void TcpClient::Open(const char* /*ipAddress*/, uint16_t /*port*/) {}
void TcpClient::Close() {}
int TcpClient::Send(const void* /*data*/, int /*len*/) {
    return 0;
}
int TcpClient::Receive(void* /*data*/, int /*maxlen*/) {
    return 0;
}

#elif defined(ENGINE_SDL)

#include <SDL_net.h>

class TcpServerImpl {
public:
    ~TcpServerImpl() { Close(); }

    void Open(uint16_t port) {
        m_port = port;
        if (SDLNet_ResolveHost(&m_ip, nullptr, port) == -1)
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
        if (m_client) {
            m_socketSet = SDLNet_AllocSocketSet(1);
            SDLNet_TCP_AddSocket(m_socketSet, m_client);
        }
        return m_client != nullptr;
    }

    bool Connected() const {
        return m_client != nullptr;
    }

    bool ReceiveDataAvailable(uint32_t timeoutMS) const {
        int numSocketsReadyForReading = SDLNet_CheckSockets(m_socketSet, timeoutMS);
        return numSocketsReadyForReading == 1;
    }

    int Send(const void* data, int len) { return SDLNet_TCP_Send(m_client, data, len); }

    int Receive(void* data, int maxlen) { return SDLNet_TCP_Recv(m_client, data, maxlen); }

private:
    TCPsocket m_server{};
    TCPsocket m_client{};
    SDLNet_SocketSet m_socketSet{};
    IPaddress m_ip{};
    uint16_t m_port{};
};

TcpServer::TcpServer() = default;
TcpServer::~TcpServer() = default;

void TcpServer::Open(uint16_t port) {
    m_impl->Open(port);
}

void TcpServer::Close() {
    m_impl->Close();
}

bool TcpServer::TryAccept() {
    return m_impl->TryAccept();
}

bool TcpServer::Connected() const {
    return m_impl->Connected();
}

bool TcpServer::ReceiveDataAvailable(uint32_t timeoutMS) const {
    return m_impl->ReceiveDataAvailable(timeoutMS);
}

int TcpServer::Send(const void* data, int len) {
    return m_impl->Send(data, len);
}

int TcpServer::Receive(void* data, int maxlen) {
    return m_impl->Receive(data, maxlen);
}

class TcpClientImpl {
public:
    ~TcpClientImpl() { Close(); }

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

    int Send(const void* data, int len) { return SDLNet_TCP_Send(m_socket, data, len); }

    int Receive(void* data, int maxlen) { return SDLNet_TCP_Recv(m_socket, data, maxlen); }

private:
    TCPsocket m_socket{};
    IPaddress m_ip{};
};

TcpClient::TcpClient() = default;
TcpClient::~TcpClient() = default;

void TcpClient::Open(const char* ipAddress, uint16_t port) {
    m_impl->Open(ipAddress, port);
}

void TcpClient::Close() {
    m_impl->Close();
}

int TcpClient::Send(const void* data, int len) {
    return m_impl->Send(data, len);
}

int TcpClient::Receive(void* data, int maxlen) {
    return m_impl->Receive(data, maxlen);
}

#else

#error Implement me for current platform

#endif
