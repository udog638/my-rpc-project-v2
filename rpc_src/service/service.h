#pragma once
#include <string>

namespace myrpc
{

    // 服务基类：所有 RPC 服务都要实现这两个接口
    class Service
    {
    public:
        virtual ~Service() = default;
        virtual std::string GetServiceName() const = 0;
        virtual bool HandleRequest(const std::string &method_name,
                                    const std::string &args,
                                    std::string &result) = 0;
    };

} // namespace myrpc
