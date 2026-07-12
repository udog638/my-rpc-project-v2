#include "log_manager.h"
#include "rpc_server_config.h"
#include "rpc_client_config.h"

int main() {
    Logger::GetInstance().Init();

    auto &server_config = myrpc::RpcServerConfig::GetInstance();
    if (!server_config.InitRpcServerConfig("../config/rpc_server.json")) {
        LOG_ERROR("加载服务端配置失败");
        return 1;
    }
    LOG_INFO("服务端配置加载成功: ip={}, port={}, max_connections={}",
             server_config.GetServersIp(), server_config.GetServersPort(),
             server_config.GetMaxConnections());
    LOG_INFO("线程池配置: max_threads={}, core_threads={}",
             server_config.GetThreadPoolConfig()->GetMaxThreads(),
             server_config.GetThreadPoolConfig()->GetCoreThreads());
    LOG_INFO("注册中心配置: service_name={}, 节点数={}",
             server_config.GetServiceRegistryConfig()->GetServiceName(),
             server_config.GetServiceRegistryConfig()->GetRegistryNodesSize());

    auto &client_config = myrpc::RpcClientConfig::GetInstance();
    if (!client_config.InitRpcClientConfig("../config/rpc_client.json")) {
        LOG_ERROR("加载客户端配置失败");
        return 1;
    }
    LOG_INFO("客户端配置加载成功: zk_host={}, zk_port={}, server_port={}",
             client_config.GetZkHost(), client_config.GetZkPort(),
             client_config.GetServerPort());

    return 0;
}
