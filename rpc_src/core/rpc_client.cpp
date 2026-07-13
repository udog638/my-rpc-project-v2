#include "rpc_client.h"
#include "rpc_client_config.h"
#include "zk_conn_handler.h"
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

        timeout_ms_ = config.GetTimeout();
        retry_times_ = config.GetRetryTimes();
        zk_namespace_ = config.GetZkNamespace();

        // 向 zk 注册中心报到，之后 Connect() 会用它查可用地址
        nlohmann::json zk_config;
        zk_config["zk_host"] = config.GetZkHost();
        zk_config["zk_port"] = config.GetZkPort();
        zk_config["zk_namespace"] = zk_namespace_;
        if (!ZkConnHandler::GetInstance().initZkConnHandler(zk_config))
        {
            throw std::runtime_error("Failed to initialize ZkConnHandler");
        }

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
            // 每次重连都重新向 zk 查一次，拿到的是当前存活的服务实例列表，
            // 具体选哪一台由负载均衡器决定(server 端配置的 load_balance_strategy)
            std::string server_address = ZkConnHandler::GetInstance().getServer(zk_namespace_);
            if (server_address.empty())
            {
                LOG_ERROR("No available server found in zk namespace: {}", zk_namespace_);
                LOG_WARN("Connect attempt {}/{} failed", retry + 1, retry_times_);
                continue;
            }

            size_t colon_pos = server_address.find(':');
            if (colon_pos == std::string::npos)
            {
                LOG_ERROR("Invalid server address from zk: {}", server_address);
                continue;
            }
            server_ip_ = server_address.substr(0, colon_pos);
            server_port_ = std::stoi(server_address.substr(colon_pos + 1));

            if (initSocket())
            {
                is_connected_ = true;
                LOG_INFO("Connected to server: {}:{}", server_ip_, server_port_);
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
