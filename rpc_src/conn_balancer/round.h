#pragma once
#include "load_balancer.h"
#include <atomic>

namespace myrpc
{

    class RoundRobinLoadBalancer : public LoadBalancer
    {
    public:
        RoundRobinLoadBalancer() : current_index_(0) {}

        std::string select(const std::vector<std::string> &instances) override
        {
            if (instances.empty())
            {
                return "";
            }
            // 使用原子操作获取下一个索引，通过模运算确保索引始终在有效范围内
            // 这样避免了检查-然后-重置的竞态条件
            size_t index = current_index_.fetch_add(1) % instances.size();
            return instances[index];
        }

    private:
        std::atomic<size_t> current_index_;
    };

} // namespace myrpc