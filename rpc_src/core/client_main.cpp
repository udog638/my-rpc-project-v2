#include "log_manager.h"
#include "rpc_client.h"
#include "serializer_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>

// client 入口：连接 server，调用 RpcService.Echo，打印结果
int main()
{
    Logger::GetInstance().Init();

    try
    {
        myrpc::RpcClient client("../config/rpc_client.json");
        LOG_INFO("已连接到 RPC server");

        nlohmann::json request;
        request["message"] = "hello, i am myrpc client";

        nlohmann::json response;
        bool success = client.Call<nlohmann::json, nlohmann::json>(
            "RpcService", "Echo", request, myrpc::SerializeType::JSON, response);

        if (success)
        {
            LOG_INFO("调用成功, response: {}", response.dump());
        }
        else
        {
            LOG_ERROR("调用失败");
            return 1;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("client 异常: {}", e.what());
        return 1;
    }

    return 0;
}
