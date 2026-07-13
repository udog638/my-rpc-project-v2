#include "service_registry.h"
#include <iostream>
#include <thread>
#include <chrono>

const std::string ServiceRegistry::ROOT_PATH = "/myrpc";

ServiceRegistry::ServiceRegistry(const std::string &zk_hosts)
    : zk_handle_(nullptr), is_connected_(false)
{

    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);

    zk_handle_ = zookeeper_init(zk_hosts.c_str(), globalWatcher,
                                30000, 0, this, 0);
    if (!zk_handle_)
    {
        throw std::runtime_error("Failed to connect to ZooKeeper");
    }

    // 等待连接建立
    int retry = 0;
    while (!is_connected_ && retry < 10)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        retry++;
    }
}

ServiceRegistry::~ServiceRegistry()
{
    if (zk_handle_)
    {
        try {
            // LOG_INFO("Closing ZooKeeper handle in ServiceRegistry destructor");
            zookeeper_close(zk_handle_);
            zk_handle_ = nullptr;
            // LOG_INFO("ZooKeeper handle closed successfully");
        } catch (const std::exception& e) {
            std::cerr << "Exception closing ZooKeeper handle in ServiceRegistry destructor: " 
                      << e.what() << std::endl;
            zk_handle_ = nullptr;
        } catch (...) {
            std::cerr << "Unknown exception closing ZooKeeper handle in ServiceRegistry destructor" 
                      << std::endl;
            zk_handle_ = nullptr;
        }
    }
}

void ServiceRegistry::globalWatcher(zhandle_t *zh, int type,
                                    int state, const char *path,
                                    void *watcherCtx)
{
    if (!watcherCtx)
        return;

    ServiceRegistry *registry = static_cast<ServiceRegistry *>(watcherCtx);
    if (type == ZOO_SESSION_EVENT)
    {
        if (state == ZOO_CONNECTED_STATE)
        {
            registry->is_connected_ = true;
            std::cout << "Connected to ZooKeeper" << std::endl;
        }
        else
        {
            registry->is_connected_ = false;
            std::cout << "Disconnected from ZooKeeper" << std::endl;
        }
    }
}

// 回调函数
void string_completion_cb(int rc, const char *value, const void *data)
{
    if (rc == ZOK)
    {
        std::cout << "Operation completed successfully" << std::endl;
    }
    else
    {
        std::cerr << "Operation failed: " << zerror(rc) << std::endl;
    }
}

bool ServiceRegistry::createNode(const std::string &path,
                                 const std::string &data,
                                 int flags)
{
    if (!zk_handle_)
        return false;

    // 创建服务实例节点（通常是临时节点）
    int ret = zoo_create(zk_handle_, path.c_str(), data.c_str(),
                         data.length(), &ZOO_OPEN_ACL_UNSAFE,
                         flags, NULL, 0);

    if (ret == ZOK)
    {
        // LOG_INFO("Successfully created service instance node: {}", path);
        return true;
    }
    else if (ret == ZNODEEXISTS)
    {
        LOG_WARN("Service instance node already exists: {}", path);
        return true; // 节点已存在也算成功
    }
    else
    {
        LOG_ERROR("Failed to create service instance node: {}, error: {}", path, zerror(ret));
        return false;
    }
}

bool ServiceRegistry::ensurePath(const std::string &path)
{
    if (path.empty())
    {
        LOG_ERROR("Path is empty");
        return true;
    }

    if (!zk_handle_)
    {
        LOG_ERROR("ZooKeeper handle is null");
        return false;
    }

    // LOG_INFO("Ensuring persistent path: {}", path);

    // 先确保父路径存在
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0)
    {
        std::string parent = path.substr(0, pos);
        if (!parent.empty() && !ensurePath(parent))
        {
            LOG_ERROR("Failed to create parent path: {}", parent);
            return false;
        }
    }

    // 使用同步API检查节点是否存在
    struct Stat stat;
    int ret = zoo_exists(zk_handle_, path.c_str(), 0, &stat);

    if (ret == ZOK)
    {
        // LOG_INFO("Persistent path exists: {}", path);
        return true;
    }
    else if (ret == ZNONODE)
    {
        // LOG_INFO("Persistent path does not exist, creating: {}", path);
        
        // 创建持久节点 - 只用于路径结构
        ret = zoo_create(zk_handle_, path.c_str(), "", 0, &ZOO_OPEN_ACL_UNSAFE,
                         0, NULL, 0); // flags=0 表示持久节点

        if (ret == ZOK)
        {
            // LOG_INFO("Successfully created persistent path: {}", path);
            return true;
        }
        else if (ret == ZNODEEXISTS)
        {
            // LOG_INFO("Persistent path already exists (created by another process): {}", path);
            return true;
        }
        else
        {
            LOG_ERROR("Failed to create persistent path: {}, error: {}", path, zerror(ret));
            return false;
        }
    }
    else
    {
        LOG_ERROR("Failed to check persistent path: {}, error: {}", path, zerror(ret));
        return false;
    }
}

bool ServiceRegistry::registerService(const std::string &service_name,
                                      const std::string &service_address)
{
    if (!isConnected() || !zk_handle_)
    {
        LOG_ERROR("Not connected to ZooKeeper");
        return false;
    }
    
    // 确保服务类型的持久路径存在 (如: /myrpc/myrpc_service)
    std::string service_path = ROOT_PATH + "/" + service_name;
    // // LOG_INFO("Ensuring service type path: {}", service_path);
    if (!ensurePath(service_path))
    {
        LOG_ERROR("Failed to ensure service type path: {}", service_path);
        return false;
    }
    // // LOG_INFO("Service type path ready: {}", service_path);

    // 创建服务实例的临时节点 (如: /myrpc/myrpc_service/124.221.19.77:8989)
    std::string instance_path = service_path + "/" + service_address;
    // // LOG_INFO("Creating service instance: {}", instance_path);
    return createNode(instance_path, service_address, ZOO_EPHEMERAL);
}

// 回调函数
void strings_completion_cb(int rc, const struct String_vector *strings, const void *data)
{
    if (rc == ZOK && strings)
    {
        // LOG_INFO("Found {} services", strings->count);
    }
}



bool ServiceRegistry::isConnected() const
{
    return is_connected_ && zk_handle_ != nullptr;
}