#pragma once

#include "core/ConsoleOutput.h"
#include "core/Tcp.h"
#include "emulator/EngineTypes.h"
#include <chrono>
#include <memory>
#include <thread>

namespace SyncMsg {

    enum class Type { FrameStart, FrameEnd };

    struct FrameStart {
        double frameTime{};
        Input input{};
    };

} // namespace SyncMsg

enum class ConnectionType { Server, Client };

class SyncProtocol {
public:
    void InitServer() {
        m_server = std::make_unique<TcpServer>();
        Errorf("Server: about to accept connection...\n");
        m_server->Open(9123);
        while (!m_server->TryAccept()) {
            Errorf("Server: no connection, retrying...\n");
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
        Errorf("Server: Connected!\n");
    }

    void ShutdownServer() {
        if (m_server) {
            m_server->Close();
        }
        m_server.release();
    }

    void InitClient() {
        m_client = std::make_unique<TcpClient>();
        Errorf("Client: about to connect...\n");
        m_client->Open("127.0.0.1", 9123);
        Errorf("Client: Connected!\n");
    }

    void ShutdownClient() {
        if (m_client) {
            m_client->Close();
        }
        m_client.release();
    }

    bool IsServer() const { return m_server != nullptr; }
    bool IsClient() const { return m_client != nullptr; }
    bool IsStandalone() const { return !IsServer() && !IsClient(); }

    void Server_SendFrameStart(double frameTime, const Input& input) {
        auto message = SyncMsg::FrameStart{frameTime, input};
        m_server->Send(SyncMsg::Type::FrameStart);
        m_server->Send(message);
    }

    void Client_RecvFrameStart(double& frameTime, Input& input) {
        RecvType(m_client, SyncMsg::Type::FrameStart);
        SyncMsg::FrameStart message;
        m_client->Receive(message);

        frameTime = message.frameTime;
        input = message.input;
    }

    void Client_SendFrameEnd() { m_client->Send(SyncMsg::Type::FrameEnd); }

    void Server_RecvFrameEnd() { RecvType(m_server, SyncMsg::Type::FrameEnd); }

    // Generic
    template <typename T>
    void SendValue(ConnectionType connType, const T& value) {
        if (connType == ConnectionType::Server) {
            m_server->Send(value);
        } else if (connType == ConnectionType::Client) {
            m_client->Send(value);
        }
    }

    template <typename T>
    void RecvValue(ConnectionType connType, T& value) {
        if (connType == ConnectionType::Server) {
            m_server->Receive(value);
        } else if (connType == ConnectionType::Client) {
            m_client->Receive(value);
        }
    }

    void SendType(ConnectionType connType, SyncMsg::Type type) { SendValue(connType, type); }

    void RecvType(ConnectionType connType, SyncMsg::Type expectedType) {
        SyncMsg::Type type{};
        RecvValue(connType, type);
        ASSERT(type == expectedType);
    }

private:
    template <typename T>
    void RecvType(const T& connection, SyncMsg::Type expectedType) {
        SyncMsg::Type type;
        connection->Receive(type);
        ASSERT(type == expectedType);
    }

    std::unique_ptr<TcpServer> m_server;
    std::unique_ptr<TcpClient> m_client;
};
