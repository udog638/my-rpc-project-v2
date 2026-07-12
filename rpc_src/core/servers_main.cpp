#include "log_manager.h"
#include "rpc_server_config.h"
#include "create_socket.h"
#include "connection_manager.h"
#include "message_cycle.h"
#include "service_manager.h"
#include "rpc_service.h"
#include <csignal>

// server 入口：加载配置 -> 建监听socket -> 注册 RpcService -> 跑 epoll 事件循环
static myrpc::MessageCycle *g_message_cycle = nullptr;

static void SignalHandler(int sig)
{
    LOG_INFO("received signal: {}, stopping server", sig);
    if (g_message_cycle)
    {
        g_message_cycle->Stop();
    }
}

int main()
{
    Logger::GetInstance().Init();
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    auto &server_config = myrpc::RpcServerConfig::GetInstance();
    if (!server_config.InitRpcServerConfig("../config/rpc_server.json"))
    {
        LOG_ERROR("加载服务端配置失败");
        return 1;
    }

    auto socket_ptr = myrpc::CreateSocket::Create(server_config);
    if (!socket_ptr)
    {
        LOG_ERROR("创建监听socket失败");
        return 1;
    }
    LOG_INFO("监听socket创建成功, ip={}, port={}",
             server_config.GetServersIp(), server_config.GetServersPort());

    auto rpc_service = std::make_shared<myrpc::RpcService>();
    if (!myrpc::ServiceManager::GetInstance().RegisterService(rpc_service))
    {
        LOG_ERROR("注册 RpcService 失败");
        return 1;
    }

    myrpc::MessageCycle message_cycle(&myrpc::ConnectionManager::GetInstance());
    if (!message_cycle.AddListenFd(socket_ptr->GetFd()))
    {
        LOG_ERROR("添加监听fd到epoll失败");
        return 1;
    }

    g_message_cycle = &message_cycle;
    LOG_INFO("RPC server 启动完成，监听 {}:{}",
             server_config.GetServersIp(), server_config.GetServersPort());
    message_cycle.Loop();

    LOG_INFO("RPC server 已退出");
    return 0;
}
