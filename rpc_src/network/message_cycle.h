#pragma once

#include <set>
#include <mutex>
#include <atomic>
#include <sys/epoll.h>
#include "connection_manager.h"
#include "connection.h"
#include "rpc_protocol.h"
#include "log_manager.h"

namespace myrpc
{

    // 事件循环：接入新连接、收数据、按 service/method 分发 RPC 请求并回写响应
    class MessageCycle
    {
    public:
        explicit MessageCycle(ConnectionManager *manager);
        ~MessageCycle();

        bool AddListenFd(int fd);
        void Loop();
        void Stop();

    private:
        void HandleEpollEvents(struct epoll_event *events, int nfds);
        void HandleNewConnection(int listen_fd);
        void HandleClientData(int fd);
        void RemoveConnection(int fd);

        // 收到一条完整的 RpcRequest 后，交给线程池异步处理(避免阻塞 epoll 主循环)，
        // 处理完在线程池的工作线程里把 RpcResponse 写回去
        void HandleRpcRequest(const std::shared_ptr<Connection> &conn, const RpcRequest &request);
        void HandleRpcRequestSync(const std::shared_ptr<Connection> &conn, const RpcRequest &request);
        void SendErrorResponse(const std::shared_ptr<Connection> &conn, uint32_t sequence_id,
                                uint32_t error_code, const std::string &error_message);

        std::set<int> listen_fds_;
        std::atomic<bool> running_;
        mutable std::mutex mutex_;

        int epoll_fd_;
        static const int MAX_EVENTS = 1024;

        ConnectionManager *connection_manager_;
    };

} // namespace myrpc
