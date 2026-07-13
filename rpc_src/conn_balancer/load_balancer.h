#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>
#include "log_manager.h"

namespace myrpc
{

    // 前向声明
    class RandomLoadBalancer;
    class RoundRobinLoadBalancer;
    class WeightedRoundRobinLoadBalancer;

    class LoadBalancer
    {
    public:
        virtual ~LoadBalancer() = default;

        // 纯虚函数 - 由子类实现具体的负载均衡算法
        virtual std::string select(const std::vector<std::string> &instances) = 0;

        // 单例接口
        static std::shared_ptr<LoadBalancer> getInstance();
        static bool initBalancer(const std::string &type = "random");

        // 静态方法 - 对外提供的服务器选择接口
        static std::string selectServer(const std::vector<std::string> &servers);

    protected:
        // 改为protected，允许子类访问
        LoadBalancer() noexcept = default;

    private:
        static std::shared_ptr<LoadBalancer> instance_;
        static std::mutex mutex_;

        // 禁止拷贝和赋值
        LoadBalancer(const LoadBalancer &) = delete;
        LoadBalancer &operator=(const LoadBalancer &) = delete;
    };

    // 工厂函数声明
    std::shared_ptr<LoadBalancer> createRandomBalancer();
    std::shared_ptr<LoadBalancer> createRoundRobinBalancer();
    std::shared_ptr<LoadBalancer> createWeightedBalancer();

}