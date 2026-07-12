#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>

namespace myrpc
{

    // 简化版错误码：够用就行，后面遇到新错误场景再补充
    enum class ErrorCode : uint32_t
    {
        SUCCESS = 0,
        SERVICE_NOT_FOUND = 3000,
        METHOD_NOT_FOUND = 3001,
        INVALID_REQUEST = 6000,
        INTERNAL_ERROR = 6002,
    };

    class Error
    {
    public:
        static std::string getErrorMessage(ErrorCode code)
        {
            static const std::unordered_map<ErrorCode, std::string> messages = {
                {ErrorCode::SUCCESS, "Success"},
                {ErrorCode::SERVICE_NOT_FOUND, "Service not found"},
                {ErrorCode::METHOD_NOT_FOUND, "Method not found"},
                {ErrorCode::INVALID_REQUEST, "Invalid request"},
                {ErrorCode::INTERNAL_ERROR, "Internal error"},
            };
            auto it = messages.find(code);
            return it != messages.end() ? it->second : "Unknown error code";
        }
    };

} // namespace myrpc
