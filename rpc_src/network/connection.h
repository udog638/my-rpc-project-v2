#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include "rpc_protocol.h"
#include "log_manager.h"

namespace myrpc
{

    // 连接类：负责收发字节流，并把"协议对象(RpcRequest/RpcResponse)"和
    // "网络上跑的字节"之间的转换全部封装起来：
    //   发送方向: Serialize -> 压缩(zstd) -> 加密(AES) -> 加 4 字节长度前缀 -> 写 socket
    //   接收方向: 读 socket -> 按长度前缀切出一帧密文 -> 解密 -> 解压 -> 得到可 Deserialize 的字节
    // 调用方(server 的 MessageCycle / client 的 RpcClient)只管 Write(RpcRequest/RpcResponse)
    // 和 ExtractFrame()，不用关心压缩加密细节，因为同一个 Connection 类要同时服务两端。
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
            kComplete,   // 切出了一帧完整数据(已解密解压，可直接 Deserialize)
            kError       // 数据损坏(解密/解压失败，或魔数不对)，连接应该被关闭
        };

        explicit Connection(int fd);
        ~Connection();

        // 把 socket 上能读到的字节都读进 read_buffer_，不做任何协议解析
        bool Read();
        // 阻塞等待 timeout_ms 毫秒，一旦可读就调用 Read()；用于客户端同步等响应
        bool ReadWithTimeout(int timeout_ms);

        // 从 read_buffer_ 中尝试切出一帧完整消息：解密、解压后返回给调用方
        FrameStatus ExtractFrame(std::string &frame_out);

        bool Write(const std::string &data);
        bool Write(const RpcResponse &response);
        bool Write(const RpcRequest &request);
        void Close();
        bool IsValid() const { return fd_ > 0 && state_ == State::CONNECTED; }

        int GetFd() const { return fd_; }
        State GetState() const { return state_; }

    private:
        // 压缩 + 加密 + 加长度前缀，写到 socket 上
        bool WriteFrame(const std::string &data);

        int fd_ = -1;
        std::vector<char> read_buffer_;
        static const size_t MAX_BUFFER_SIZE = 1024 * 1024;
        static const uint32_t MAX_FRAME_SIZE = 16 * 1024 * 1024; // 16MB，防止恶意长度前缀撑爆内存

        State state_ = State::DISCONNECTED;
    };

} // namespace myrpc
