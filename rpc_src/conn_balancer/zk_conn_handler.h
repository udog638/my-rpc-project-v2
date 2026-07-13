#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <zookeeper/zookeeper.h>
#include <zookeeper/zookeeper_version.h>
#include <zookeeper/proto.h>

#ifdef __cplusplus
}
#endif

#include <atomic>
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <vector>
#include <nlohmann/json.hpp>
#include <log_manager.h>
#include "service_registry.h"  // 包含完整定义

class LoadBalancer;

namespace myrpc
{
    class ServiceRegistryConfig;
    
    class ZkConnHandler
    {
    public:
        // 单例访问
        static ZkConnHandler &GetInstance()
        {
            static ZkConnHandler instance;
            return instance;
        }
        // 初始化方法
        bool initZkConnHandler(const nlohmann::json &config);

        // 获取服务器列表
        std::vector<std::string> getAllServers(const std::string &zk_namespace);

        // 获取单个服务器
        std::string getServer(const std::string &zk_namespace);

        // 服务注册方法
        bool registerService(const std::string &service_name, const std::string &service_address);
        bool registerServicesFromConfig(const myrpc::ServiceRegistryConfig* registry_config);

        // ServiceRegistry 访问方法
        ServiceRegistry* getServiceRegistry() {
            return service_registry_.get();
        }
        
        const ServiceRegistry* getServiceRegistry() const {
            return service_registry_.get();
        }
        
        bool hasServiceRegistry() const {
            return service_registry_ != nullptr;
        }
        
        // 安全的 ServiceRegistry 访问方法，自动初始化
        ServiceRegistry* getOrCreateServiceRegistry();

        // 设置方法
        void setZkHost(const std::string &host);
        void setZkPort(int port);
        void setZkRetryInterval(int seconds);
        void setZkNamespace(const std::string &zk_namespace);
        // void setZkClient(zhandle_t *zk_client);

        ~ZkConnHandler();

        // 清理资源
        void cleanup();

        // 禁止拷贝和移动
        ZkConnHandler(const ZkConnHandler &) = delete;
        ZkConnHandler &operator=(const ZkConnHandler &) = delete;
        ZkConnHandler(ZkConnHandler &&) = delete;
        ZkConnHandler &operator=(ZkConnHandler &&) = delete;

        void updateServersFromZk(const std::string &zk_namespace);
        bool ensureZkConnection();

    private:
        // Zookeeper 回调
        ZkConnHandler()
            : zk_client_(nullptr), running_(false), service_registry_(nullptr)
        {
            // initZkClient();
        }
        static void global_watcher(zhandle_t *zh, int type, int state,
                                   const char *path, void *watcherCtx);

        // 成员变量
        std::atomic<bool> running_;
        std::chrono::seconds retry_interval_;

        std::mutex mutex_; // 保护成员变量访问
        std::mutex servers_mutex_;
        std::vector<std::string> servers_;
        zhandle_t *zk_client_;

        // 配置相关
        std::string zk_host_;
        int zk_port_;
        std::string zk_namespace_;

        // 使用智能指针管理ServiceRegistry实例，自动内存管理
        std::unique_ptr<ServiceRegistry> service_registry_;

        // 内部创建 ServiceRegistry 的辅助方法
        bool createServiceRegistryIfNeeded();

        // ZooKeeper客户端初始化
        bool initZkClient();
    };

} // namespace myrpc