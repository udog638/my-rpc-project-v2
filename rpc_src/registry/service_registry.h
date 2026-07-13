#pragma once
#include <string>
#include <vector>
#include <memory>
#include <zookeeper/zookeeper.h>
#include "log_manager.h"

class ServiceRegistry
{
public:
    ServiceRegistry(const std::string &zk_hosts);
    ~ServiceRegistry();

    // 注册服务
    bool registerService(const std::string &service_name,
                         const std::string &service_address);

    // 发现服务


    // 检查连接状态
    bool isConnected() const;

private:
    static void globalWatcher(zhandle_t *zh, int type,
                              int state, const char *path, void *watcherCtx);

    bool createNode(const std::string &path, const std::string &data,
                    int flags = 0);
    bool ensurePath(const std::string &path);

    zhandle_t *zk_handle_;
    static const std::string ROOT_PATH;
    bool is_connected_;
};