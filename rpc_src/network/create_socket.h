#pragma once
#include <memory>
#include <string>
#include <netinet/tcp.h>
#include "rpc_server_config.h"

namespace myrpc
{

    class CreateSocket
    {
    public:
        static std::shared_ptr<CreateSocket> Create(const RpcServerConfig &config);
        explicit CreateSocket(const RpcServerConfig &config);
        ~CreateSocket();

        int GetFd() const { return fd_; }
        int Accept();

    private:
        bool Init();
        bool SetSocketOpt();

        int fd_;
        std::string servers_ip_;
        uint16_t servers_port_;
        int servers_max_connections_;
        int socket_timeout_ms_;
    };

} // namespace myrpc
