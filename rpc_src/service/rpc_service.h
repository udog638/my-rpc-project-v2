#pragma once
#include "service.h"
#include "log_manager.h"
#include <nlohmann/json.hpp>

namespace myrpc
{

    // 示例服务：Echo，用来验证 server/client 整条链路跑通
    class RpcService : public Service
    {
    public:
        std::string GetServiceName() const override
        {
            return "RpcService";
        }

        bool HandleRequest(const std::string &method_name,
                            const std::string &args,
                            std::string &result) override
        {
            if (method_name != "Echo")
            {
                LOG_ERROR("Unknown method: {}", method_name);
                return false;
            }

            try
            {
                nlohmann::json request = nlohmann::json::parse(args);
                nlohmann::json response;
                response["echo"] = "hello from myrpc server";
                response["received_message"] = request.value("message", "");
                result = response.dump();
                return true;
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to process Echo request: {}", e.what());
                return false;
            }
        }
    };

} // namespace myrpc
