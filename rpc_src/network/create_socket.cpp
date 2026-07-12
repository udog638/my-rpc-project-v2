#include "create_socket.h"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace myrpc
{

    std::shared_ptr<CreateSocket> CreateSocket::Create(const RpcServerConfig &config)
    {
        auto socket_ptr = std::make_shared<CreateSocket>(config);
        if (!socket_ptr->Init())
        {
            return nullptr;
        }
        return socket_ptr;
    }

    CreateSocket::CreateSocket(const RpcServerConfig &config)
        : fd_(-1),
          servers_ip_(config.GetServersIp()),
          servers_port_(config.GetServersPort()),
          servers_max_connections_(config.GetMaxConnections()),
          socket_timeout_ms_(config.GetTimeout())
    {
    }

    CreateSocket::~CreateSocket()
    {
        if (fd_ > 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool CreateSocket::Init()
    {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0)
        {
            LOG_ERROR("create socket error, errno: {}, error: {}", errno, strerror(errno));
            return false;
        }

        if (!SetSocketOpt())
        {
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(servers_port_);

        if (servers_ip_.empty() || servers_ip_ == "0.0.0.0")
        {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else if (inet_pton(AF_INET, servers_ip_.c_str(), &addr.sin_addr) <= 0)
        {
            LOG_ERROR("invalid ip address: {}", servers_ip_);
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        if (::bind(fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            LOG_ERROR("bind error, ip: {}, port: {}, errno: {}, error: {}",
                      servers_ip_, servers_port_, errno, strerror(errno));
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        if (::listen(fd_, servers_max_connections_) < 0)
        {
            LOG_ERROR("listen error, errno: {}, error: {}", errno, strerror(errno));
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        return true;
    }

    bool CreateSocket::SetSocketOpt()
    {
        int reuse = 1;
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        {
            LOG_ERROR("set SO_REUSEADDR error, errno: {}, error: {}", errno, strerror(errno));
            return false;
        }

        int nodelay = 1;
        if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0)
        {
            LOG_ERROR("set TCP_NODELAY error, errno: {}, error: {}", errno, strerror(errno));
            return false;
        }

        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags < 0 || ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            LOG_ERROR("set socket nonblock error, errno: {}, error: {}", errno, strerror(errno));
            return false;
        }

        return true;
    }

    int CreateSocket::Accept()
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = ::accept(fd_, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                LOG_ERROR("accept error, errno: {}, error: {}", errno, strerror(errno));
            }
            return -1;
        }

        int flags = ::fcntl(client_fd, F_GETFL, 0);
        if (flags < 0 || ::fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            LOG_ERROR("set nonblock failed, fd: {}", client_fd);
            ::close(client_fd);
            return -1;
        }

        return client_fd;
    }

} // namespace myrpc
