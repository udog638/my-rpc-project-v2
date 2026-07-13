#include "rpc_server_config.h"
#include "zk_conn_handler.h"
#include "load_balancer.h"
#include <fstream>

namespace myrpc
{

    bool RpcServerConfig::InitRpcServerConfig(const std::string &config_file)
    {
        try
        {
            std::ifstream file(config_file);
            if (!file.is_open())
            {
                LOG_ERROR("Failed to open config file: {}", config_file);
                return false;
            }

            nlohmann::json config = nlohmann::json::parse(file);

            // 基本配置
            SetServersNamePrefix(config.value("servers_name_prefix", "myrpc"));
            SetServersIp(config.value("servers_ip", "0.0.0.0"));
            SetServersPort(config.value("servers_port", 8989));
            SetTimeout(config.value("timeout", 3000));
            SetMaxConnections(config.value("max_connections", 1000));

            // 初始化注册配置
            if (config.contains("register_config"))
            {
                std::string register_config_file = config.value("register_config", "../config/servers.json");
                if (!service_registry_config_->InitRegistryConfig(register_config_file))
                {
                    LOG_ERROR("Failed to initialize registry config");
                    return false;
                }
            }

            // 初始化线程池配置
            if (config.contains("thread_pool"))
            {
                if (!thread_pool_config_->InitThreadPoolConfig(config["thread_pool"]))
                {
                    LOG_ERROR("Failed to initialize thread pool config");
                    return false;
                }
            }

            // 初始化 zk 连接(zookeeper 服务注册/发现)
            if (config.contains("scheduler"))
            {
                auto &zk_conn_handler = ZkConnHandler::GetInstance();
                if (!zk_conn_handler.initZkConnHandler(config["scheduler"]))
                {
                    LOG_ERROR("Failed to initialize zk conn handler");
                    return false;
                }
            }

            // 初始化负载均衡器(random/round/weight)
            if (config.contains("load_balance_strategy"))
            {
                std::string strategy = config["load_balance_strategy"];
                if (!LoadBalancer::initBalancer(strategy))
                {
                    LOG_ERROR("Failed to initialize load balancer with strategy: {}", strategy);
                    return false;
                }
            }

            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Error initializing config: {}", e.what());
            return false;
        }
    }

} // namespace myrpc
