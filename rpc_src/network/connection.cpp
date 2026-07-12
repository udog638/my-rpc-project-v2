#include "connection.h"
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <cstring>

namespace myrpc
{

    Connection::Connection(int fd) : fd_(fd)
    {
        state_ = (fd_ >= 0) ? State::CONNECTED : State::DISCONNECTED;
    }

    Connection::~Connection()
    {
        Close();
    }

    bool Connection::Read()
    {
        if (state_ != State::CONNECTED)
        {
            return false;
        }

        char buf[4096];
        while (true)
        {
            ssize_t n = ::read(fd_, buf, sizeof(buf));
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break; // 边缘触发模式下，数据已读完
                }
                LOG_ERROR("read error, fd: {}, errno: {}, error: {}", fd_, errno, strerror(errno));
                state_ = State::DISCONNECTED;
                return false;
            }

            if (n == 0)
            {
                state_ = State::DISCONNECTED;
                return false; // 对端关闭连接
            }

            if (read_buffer_.size() + n > MAX_BUFFER_SIZE)
            {
                LOG_ERROR("read buffer overflow, fd: {}", fd_);
                state_ = State::DISCONNECTED;
                return false;
            }

            read_buffer_.insert(read_buffer_.end(), buf, buf + n);

            if (static_cast<size_t>(n) < sizeof(buf))
            {
                break;
            }
        }

        return true;
    }

    bool Connection::ReadWithTimeout(int timeout_ms)
    {
        while (true)
        {
            fd_set read_set;
            struct timeval timeout;
            timeout.tv_sec = timeout_ms / 1000;
            timeout.tv_usec = (timeout_ms % 1000) * 1000;

            FD_ZERO(&read_set);
            FD_SET(fd_, &read_set);

            int ret = select(fd_ + 1, &read_set, nullptr, nullptr, &timeout);
            if (ret < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                LOG_ERROR("select error, fd: {}, errno: {}", fd_, errno);
                return false;
            }

            if (ret == 0)
            {
                LOG_ERROR("read timeout after {} ms, fd: {}", timeout_ms, fd_);
                return false;
            }

            return Read();
        }
    }

    Connection::FrameStatus Connection::ExtractFrame(std::string &frame_out)
    {
        if (read_buffer_.size() < sizeof(RpcHeader))
        {
            return FrameStatus::kIncomplete;
        }

        RpcHeader header;
        std::memcpy(&header, read_buffer_.data(), sizeof(header));

        if (header.magic_number != RpcHeader::MAGIC)
        {
            LOG_ERROR("Invalid magic number: {:#x}, fd: {}", header.magic_number, fd_);
            return FrameStatus::kError;
        }

        size_t frame_size = sizeof(RpcHeader) + header.message_length;
        if (read_buffer_.size() < frame_size)
        {
            return FrameStatus::kIncomplete; // 半包，等待更多数据
        }

        frame_out.assign(read_buffer_.begin(), read_buffer_.begin() + frame_size);
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + frame_size);

        return FrameStatus::kComplete;
    }

    bool Connection::Write(const std::string &data)
    {
        if (state_ != State::CONNECTED || data.empty())
        {
            return false;
        }

        size_t sent = 0;
        while (sent < data.size())
        {
            ssize_t n = ::write(fd_, data.data() + sent, data.size() - sent);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    continue; // 简化处理：忙等直到写完（生产环境应改为注册可写事件）
                }
                LOG_ERROR("write error, fd: {}, errno: {}", fd_, errno);
                state_ = State::DISCONNECTED;
                return false;
            }
            sent += n;
        }
        return true;
    }

    bool Connection::Write(const RpcResponse &response)
    {
        std::string serialized;
        if (!response.Serialize(serialized))
        {
            LOG_ERROR("Failed to serialize response, fd: {}", fd_);
            return false;
        }
        return Write(serialized);
    }

    bool Connection::Write(const RpcRequest &request)
    {
        std::string serialized;
        if (!request.Serialize(serialized))
        {
            LOG_ERROR("Failed to serialize request, fd: {}", fd_);
            return false;
        }
        return Write(serialized);
    }

    void Connection::Close()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
        state_ = State::DISCONNECTED;
    }

} // namespace myrpc
