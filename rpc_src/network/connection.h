#pragma once
#include <memory>
#include <string>
#include <vector>
#include "rpc_protocol.h"
#include "log_manager.h"

namespace myrpc
{

    // 连接类：只负责收发字节流 + 按 RpcHeader 切帧(处理粘包/半包)。
    // 具体一帧是 RpcRequest 还是 RpcResponse，由调用方(server 的 MessageCycle /
    // client 的 RpcClient)决定，因为同一个 Connection 类要同时服务两端。
    // 压缩/加密留给后面的模块打通后再接入(直接在 ExtractFrame 前后插入一层即可)
    class Connection : public std::enable_shared_from_this<Connection>
    {
    public:
        enum class State
        {
            CONNECTED,
            DISCONNECTED
        };

        enum class FrameStatus
        {
            kIncomplete, // 数据不够一帧，等待下一次 Read()
            kComplete,   // 切出了一帧完整数据
            kError       // 数据损坏(比如魔数不对)，连接应该被关闭
        };

        explicit Connection(int fd);
        ~Connection();

        // 把 socket 上能读到的字节都读进 read_buffer_，不做任何协议解析
        bool Read();
        // 阻塞等待 timeout_ms 毫秒，一旦可读就调用 Read()；用于客户端同步等响应
        bool ReadWithTimeout(int timeout_ms);

        // 从 read_buffer_ 中尝试切出一帧完整消息(header+body)
        FrameStatus ExtractFrame(std::string &frame_out);

        bool Write(const std::string &data);
        bool Write(const RpcResponse &response);
        bool Write(const RpcRequest &request);
        void Close();
        bool IsValid() const { return fd_ > 0 && state_ == State::CONNECTED; }

        int GetFd() const { return fd_; }
        State GetState() const { return state_; }

    private:
        int fd_ = -1;
        std::vector<char> read_buffer_;
        static const size_t MAX_BUFFER_SIZE = 1024 * 1024;

        State state_ = State::DISCONNECTED;
    };

} // namespace myrpc
