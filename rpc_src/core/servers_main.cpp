#include "log_manager.h"
#include "rpc_server_config.h"
#include "create_socket.h"
#include "connection_manager.h"
#include "message_cycle.h"
#include "service_manager.h"
#include "rpc_service.h"
#include "thread_pool_singleton.h"
#include "zk_conn_handler.h"
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

    // 向 zk 注册中心注册本机地址，client 端通过 ZkConnHandler::getServer() 发现它
    auto registry_config = server_config.GetServiceRegistryConfig();
    if (!myrpc::ZkConnHandler::GetInstance().registerServicesFromConfig(registry_config))
    {
        LOG_ERROR("向 zk 注册服务失败");
        return 1;
    }

    auto thread_pool_config = server_config.GetThreadPoolConfig();
    meeting_ctrl::ThreadPoolStruct pool_config;
    pool_config.core_threads = thread_pool_config->GetCoreThreads();
    pool_config.max_threads = thread_pool_config->GetMaxThreads();
    pool_config.max_queue_size = thread_pool_config->GetQueueSize();
    pool_config.keep_alive_time = std::chrono::seconds(thread_pool_config->GetKeepAliveTime());
    meeting_ctrl::ThreadPoolSingleton::init(pool_config);
    LOG_INFO("线程池初始化完成: core_threads={}, max_threads={}",
             pool_config.core_threads, pool_config.max_threads);

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

    LOG_INFO("正在关闭线程池...");
    meeting_ctrl::ThreadPoolSingleton::shutdown(std::chrono::seconds(10));

    LOG_INFO("正在清理 zk 连接...");
    myrpc::ZkConnHandler::GetInstance().cleanup();

    LOG_INFO("RPC server 已退出");
    return 0;
}
