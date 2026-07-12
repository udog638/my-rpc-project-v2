#include "rpc_client.h"
#include "rpc_client_config.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace myrpc
{

    RpcClient::RpcClient(const std::string &config_path)
    {
        auto &config = RpcClientConfig::GetInstance();
        if (!config.InitRpcClientConfig(config_path))
        {
            throw std::runtime_error("Failed to load RPC client config: " + config_path);
        }

        // 简化版：直接从配置里读 server 地址，不接 zk 服务发现
        server_ip_ = "127.0.0.1";
        server_port_ = config.GetServerPort();
        timeout_ms_ = config.GetTimeout();
        retry_times_ = config.GetRetryTimes();

        if (!Connect())
        {
            throw std::runtime_error("Failed to connect to RPC server");
        }
    }

    RpcClient::~RpcClient()
    {
        Disconnect();
    }

    bool RpcClient::initSocket()
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            LOG_ERROR("Create socket failed: {}", strerror(errno));
            return false;
        }

        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0)
        {
            LOG_ERROR("Invalid server ip: {}", server_ip_);
            ::close(fd);
            return false;
        }

        if (::connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            LOG_ERROR("Connect failed: {}:{}, errno: {}", server_ip_, server_port_, errno);
            ::close(fd);
            return false;
        }

        connection_ = std::make_shared<Connection>(fd);
        return true;
    }

    bool RpcClient::Connect()
    {
        if (is_connected_)
        {
            return true;
        }

        for (int retry = 0; retry < retry_times_; ++retry)
        {
            if (initSocket())
            {
                is_connected_ = true;
                return true;
            }
            LOG_WARN("Connect attempt {}/{} failed", retry + 1, retry_times_);
        }

        return false;
    }

    void RpcClient::Disconnect()
    {
        if (connection_)
        {
            connection_->Close();
            connection_.reset();
        }
        is_connected_ = false;
    }

} // namespace myrpc
