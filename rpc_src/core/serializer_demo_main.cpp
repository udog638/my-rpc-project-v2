#include "log_manager.h"
#include "serializer_manager.h"
#include "message.pb.h"

int main() {
    Logger::GetInstance().Init();

    // JSON 序列化演示
    nlohmann::json req_json;
    req_json["name"] = "Alice";
    std::string json_buf = myrpc::SerializerManager::serialize(req_json, myrpc::SerializeType::JSON);
    LOG_INFO("JSON 序列化结果: {}", json_buf);

    nlohmann::json req_json_out;
    if (myrpc::SerializerManager::deserialize(json_buf, req_json_out, myrpc::SerializeType::JSON)) {
        LOG_INFO("JSON 反序列化结果: name={}", req_json_out["name"].get<std::string>());
    }

    // Protobuf 序列化演示
    myrpc::HelloRequest pb_req;
    pb_req.set_name("Bob");
    std::string pb_buf = myrpc::SerializerManager::serialize(pb_req, myrpc::SerializeType::PROTOBUF);
    LOG_INFO("Protobuf 序列化结果长度: {}", pb_buf.size());

    myrpc::HelloRequest pb_req_out;
    if (myrpc::SerializerManager::deserialize(pb_buf, pb_req_out, myrpc::SerializeType::PROTOBUF)) {
        LOG_INFO("Protobuf 反序列化结果: name={}", pb_req_out.name());
    }

    return 0;
}
