#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "service.h"
#include "log_manager.h"

namespace myrpc
{

    // 服务注册表(单例)：server 启动时把 Service 实现注册进来，
    // 收到请求时按 service_name 找到对应实现并转发调用
    class ServiceManager
    {
    public:
        static ServiceManager &GetInstance()
        {
            static ServiceManager instance;
            return instance;
        }

        bool RegisterService(std::shared_ptr<Service> service)
        {
            if (!service)
            {
                LOG_ERROR("register null service");
                return false;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            std::string service_name = service->GetServiceName();
            if (services_.find(service_name) != services_.end())
            {
                LOG_WARN("service already exists: {}", service_name);
                return false;
            }

            services_[service_name] = service;
            return true;
        }

        bool HandleRpcRequest(const std::string &service_name,
                              const std::string &method_name,
                              const std::string &args,
                              std::string &result)
        {
            std::shared_ptr<Service> service;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = services_.find(service_name);
                if (it == services_.end())
                {
                    LOG_ERROR("service not found: {}", service_name);
                    return false;
                }
                service = it->second;
            }

            if (!service->HandleRequest(method_name, args, result))
            {
                LOG_ERROR("handle request failed: service={}, method={}", service_name, method_name);
                return false;
            }
            return true;
        }

        ServiceManager(const ServiceManager &) = delete;
        ServiceManager &operator=(const ServiceManager &) = delete;

    private:
        ServiceManager() = default;
        std::unordered_map<std::string, std::shared_ptr<Service>> services_;
        mutable std::mutex mutex_;
    };

} // namespace myrpc
