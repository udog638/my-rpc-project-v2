// config/server_config.h

#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <log_manager.h>
#include "thread_pool_config.h"
#include "registry_config.h"
// zk_conn_handler.h / load_balancer.h 属于后面 zk 注册和负载均衡模块，
// 讲到那两个模块时再把 scheduler / load_balance_strategy 的解析补回来

/**
 * 加载 rpc_server_config.json 文件，服务端的配置
 **/
namespace myrpc
{

    class RpcServerConfig
    {
    public:
        static RpcServerConfig &GetInstance()
        {
            static RpcServerConfig instance;
            return instance;
        }

        bool InitRpcServerConfig(const std::string &config_file);
        const std::string &GetServersNamePrefix() const { return servers_name_prefix_; }
        const std::string &GetServersIp() const { return servers_ip_; }
        uint16_t GetServersPort() const { return servers_port_; }
        int GetTimeout() const { return timeout_; }
        uint32_t GetMaxConnections() const { return max_connections_; }

        const ServiceRegistryConfig *GetServiceRegistryConfig() const { return service_registry_config_.get(); }
        const ThreadPoolConfig *GetThreadPoolConfig() const { return thread_pool_config_.get(); }

        void SetServersNamePrefix(const std::string &servers_name_prefix) { servers_name_prefix_ = servers_name_prefix; }
        void SetServersIp(const std::string &servers_ip) { servers_ip_ = servers_ip; }
        void SetServersPort(uint16_t servers_port) { servers_port_ = servers_port; }
        void SetTimeout(uint32_t timeout) { timeout_ = timeout; }
        void SetMaxConnections(uint32_t max_connections) { max_connections_ = max_connections; }
        RpcServerConfig(const RpcServerConfig &) = delete;
        RpcServerConfig &operator=(const RpcServerConfig &) = delete;

        RpcServerConfig(RpcServerConfig &&) = delete;
        RpcServerConfig &operator=(RpcServerConfig &&) = delete;

    private:
        uint32_t max_connections_ = 0;
        std::string servers_ip_;
        uint16_t servers_port_ = 0;
        uint32_t timeout_ = 0;
        std::string servers_name_prefix_;
        RpcServerConfig()
            : thread_pool_config_(std::make_unique<ThreadPoolConfig>()), service_registry_config_(std::make_unique<ServiceRegistryConfig>()) {}

        std::unique_ptr<ThreadPoolConfig> thread_pool_config_;
        std::unique_ptr<ServiceRegistryConfig> service_registry_config_;
    };

} // namespace myrpc
