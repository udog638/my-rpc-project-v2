#include "rpc_protocol.h"
#include "log_manager.h"
#include <cstring>

namespace myrpc
{
    const uint32_t RpcHeader::MAGIC = 0x12345678;

    bool RpcRequest::Serialize(std::string &out) const
    {
        try
        {
            size_t body_size = service_name_.size() + method_name_.size() + payload_.size() +
                                3 * sizeof(uint32_t);

            out.resize(sizeof(RpcHeader) + body_size);

            RpcHeader header;
            header.magic_number = RpcHeader::MAGIC;
            header.message_length = body_size;
            header.sequence_id = sequence_id_;
            std::memcpy(&out[0], &header, sizeof(header));

            size_t pos = sizeof(header);

            uint32_t len = service_name_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], service_name_.data(), len);
            pos += len;

            len = method_name_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], method_name_.data(), len);
            pos += len;

            len = payload_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], payload_.data(), len);

            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("RpcRequest serialize failed: {}", e.what());
            return false;
        }
    }

    bool RpcRequest::Deserialize(const std::string &in)
    {
        try
        {
            if (in.size() < sizeof(RpcHeader))
            {
                LOG_ERROR("Input too small for header, size: {}, need: {}", in.size(), sizeof(RpcHeader));
                return false;
            }

            RpcHeader header;
            std::memcpy(&header, in.data(), sizeof(header));

            if (header.magic_number != RpcHeader::MAGIC)
            {
                LOG_ERROR("Invalid magic number: {:#x}", header.magic_number);
                return false;
            }

            size_t pos = sizeof(RpcHeader);
            uint32_t len = 0;

            if (pos + sizeof(uint32_t) > in.size())
            {
                LOG_ERROR("Cannot read service_name length");
                return false;
            }
            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);
            if (len > in.size() - pos)
            {
                LOG_ERROR("service_name too long: len={}", len);
                return false;
            }
            service_name_ = in.substr(pos, len);
            pos += len;

            if (pos + sizeof(uint32_t) > in.size())
            {
                LOG_ERROR("Cannot read method_name length");
                return false;
            }
            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);
            if (len > in.size() - pos)
            {
                LOG_ERROR("method_name too long: len={}", len);
                return false;
            }
            method_name_ = in.substr(pos, len);
            pos += len;

            if (pos + sizeof(uint32_t) > in.size())
            {
                LOG_ERROR("Cannot read payload length");
                return false;
            }
            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);
            if (len > in.size() - pos)
            {
                LOG_ERROR("payload too long: len={}", len);
                return false;
            }
            payload_ = in.substr(pos, len);

            sequence_id_ = header.sequence_id;
            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("RpcRequest deserialize failed: {}", e.what());
            return false;
        }
    }

    bool RpcResponse::Serialize(std::string &out) const
    {
        try
        {
            size_t body_size = result_data_.size() + error_message_.size() +
                                2 * sizeof(uint32_t) +
                                sizeof(error_code_) +
                                sizeof(sequence_id_);

            out.resize(sizeof(RpcHeader) + body_size);

            RpcHeader header;
            header.magic_number = RpcHeader::MAGIC;
            header.message_length = body_size;
            header.sequence_id = sequence_id_;
            std::memcpy(&out[0], &header, sizeof(header));

            size_t pos = sizeof(header);

            uint32_t len = result_data_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], result_data_.data(), len);
            pos += len;

            len = error_message_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], error_message_.data(), len);
            pos += len;

            std::memcpy(&out[pos], &error_code_, sizeof(error_code_));
            pos += sizeof(error_code_);

            std::memcpy(&out[pos], &sequence_id_, sizeof(sequence_id_));

            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("RpcResponse serialize failed: {}", e.what());
            return false;
        }
    }

    bool RpcResponse::Deserialize(const std::string &in)
    {
        try
        {
            if (in.size() < sizeof(RpcHeader))
            {
                LOG_ERROR("Input too small for header, size: {}, need: {}", in.size(), sizeof(RpcHeader));
                return false;
            }

            RpcHeader header;
            std::memcpy(&header, in.data(), sizeof(header));

            if (header.magic_number != RpcHeader::MAGIC)
            {
                LOG_ERROR("Invalid magic number: {:#x}", header.magic_number);
                return false;
            }

            if (in.size() != sizeof(header) + header.message_length)
            {
                LOG_ERROR("Invalid message length: expected {}, got {}",
                          sizeof(header) + header.message_length, in.size());
                return false;
            }

            size_t pos = sizeof(header);
            uint32_t len = 0;

            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);
            if (pos + len > in.size())
            {
                LOG_ERROR("result_data too long: len={}", len);
                return false;
            }
            result_data_ = in.substr(pos, len);
            pos += len;

            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);
            if (pos + len > in.size())
            {
                LOG_ERROR("error_message too long: len={}", len);
                return false;
            }
            error_message_ = in.substr(pos, len);
            pos += len;

            std::memcpy(&error_code_, &in[pos], sizeof(error_code_));
            pos += sizeof(error_code_);

            std::memcpy(&sequence_id_, &in[pos], sizeof(sequence_id_));

            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("RpcResponse deserialize failed: {}", e.what());
            return false;
        }
    }

} // namespace myrpc
