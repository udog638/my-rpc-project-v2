#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <netinet/in.h>
#include "connection.h"
#include "rpc_protocol.h"
#include "serializer_manager.h"
#include "error_code.h"
#include "log_manager.h"

namespace myrpc
{

    // RPC 客户端：启动时向 zk 注册中心查询 RpcService 的可用地址列表，
    // 用负载均衡器(random/round/weight，由 server 端配置的 load_balance_strategy 决定)选一台连接
    class RpcClient
    {
    public:
        explicit RpcClient(const std::string &config_path = "../config/rpc_client.json");
        ~RpcClient();

        RpcClient(const RpcClient &) = delete;
        RpcClient &operator=(const RpcClient &) = delete;

        bool Connect();
        void Disconnect();
        bool IsConnected() const { return is_connected_; }

        template <typename Response, typename Request>
        bool Call(const std::string &service_name,
                  const std::string &method_name,
                  const Request &request,
                  SerializeType serialize_type,
                  Response &response)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!IsConnected() && !Connect())
            {
                LOG_ERROR("Not connected to server");
                return false;
            }

            std::string payload = SerializerManager::serialize(request, serialize_type);
            if (payload.empty())
            {
                LOG_ERROR("Failed to serialize request");
                return false;
            }

            RpcRequest rpc_request;
            rpc_request.setServiceName(service_name);
            rpc_request.setMethodName(method_name);
            rpc_request.setPayload(payload);
            rpc_request.setSequenceId(GenerateSequenceId());

            if (!connection_->Write(rpc_request))
            {
                LOG_ERROR("Failed to send request");
                is_connected_ = false;
                return false;
            }

            return ReadResponse(response, serialize_type);
        }

    private:
        bool initSocket();

        template <typename Response>
        bool ReadResponse(Response &response, SerializeType serialize_type)
        {
            std::string frame;
            Connection::FrameStatus status = Connection::FrameStatus::kIncomplete;

            // 服务端一次响应通常就是一帧，但也可能被拆成多次 TCP 包送达，
            // 所以要在 buffer 里没有完整帧时反复 ReadWithTimeout()
            while (status == Connection::FrameStatus::kIncomplete)
            {
                if (!connection_->ReadWithTimeout(timeout_ms_))
                {
                    LOG_ERROR("Failed to read response within timeout");
                    is_connected_ = false;
                    return false;
                }
                status = connection_->ExtractFrame(frame);
            }

            if (status == Connection::FrameStatus::kError)
            {
                LOG_ERROR("Invalid response frame");
                is_connected_ = false;
                return false;
            }

            RpcResponse rpc_response;
            if (!rpc_response.Deserialize(frame))
            {
                LOG_ERROR("Failed to deserialize response");
                return false;
            }

            if (rpc_response.getErrorCode() != static_cast<uint32_t>(ErrorCode::SUCCESS))
            {
                LOG_ERROR("RPC call failed: {} (code: {})",
                          rpc_response.getErrorMessage(), rpc_response.getErrorCode());
                return false;
            }

            std::string result_data = rpc_response.getResultData();
            if (!SerializerManager::deserialize(result_data, response, serialize_type))
            {
                LOG_ERROR("Failed to deserialize response data");
                return false;
            }
            return true;
        }

        uint32_t GenerateSequenceId() { return ++sequence_id_; }

        std::string server_ip_;
        int server_port_ = 0;
        int timeout_ms_ = 3000;
        int retry_times_ = 3;
        std::string zk_namespace_;

        std::shared_ptr<Connection> connection_;
        std::mutex mutex_;
        std::atomic<uint32_t> sequence_id_{0};
        std::atomic<bool> is_connected_{false};
    };

} // namespace myrpc
