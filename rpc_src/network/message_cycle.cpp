#include "message_cycle.h"
#include "service_manager.h"
#include "error_code.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace myrpc
{

    MessageCycle::MessageCycle(ConnectionManager *manager)
        : running_(false), connection_manager_(manager)
    {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1)
        {
            LOG_ERROR("Failed to create epoll: {}", strerror(errno));
            throw std::runtime_error("Failed to create epoll");
        }
    }

    MessageCycle::~MessageCycle()
    {
        running_ = false;
        if (epoll_fd_ >= 0)
        {
            ::close(epoll_fd_);
            epoll_fd_ = -1;
        }
    }

    bool MessageCycle::AddListenFd(int fd)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1)
        {
            LOG_ERROR("Failed to add fd {} to epoll: {}", fd, strerror(errno));
            return false;
        }

        listen_fds_.insert(fd);
        return true;
    }

    void MessageCycle::HandleNewConnection(int listen_fd)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = ::accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            LOG_ERROR("Accept failed, errno: {}", errno);
            return;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        LOG_INFO("New connection from {}:{}, fd: {}", client_ip, ntohs(client_addr.sin_port), client_fd);

        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags < 0 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            LOG_ERROR("Set nonblock failed, fd: {}", client_fd);
            ::close(client_fd);
            return;
        }

        int nodelay = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        auto conn = std::make_shared<Connection>(client_fd);
        connection_manager_->AddConnection(conn);

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET; // 边缘触发
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) == -1)
        {
            LOG_ERROR("Failed to add fd {} to epoll: {}", client_fd, strerror(errno));
            RemoveConnection(client_fd);
        }
    }

    void MessageCycle::HandleRpcRequest(const std::shared_ptr<Connection> &conn, const RpcRequest &request)
    {
        if (request.getServiceName().empty() || request.getMethodName().empty())
        {
            SendErrorResponse(conn, request.getSequenceId(),
                               static_cast<uint32_t>(ErrorCode::INVALID_REQUEST),
                               Error::getErrorMessage(ErrorCode::INVALID_REQUEST));
            return;
        }

        std::string result;
        if (!ServiceManager::GetInstance().HandleRpcRequest(
                request.getServiceName(), request.getMethodName(), request.getPayload(), result))
        {
            LOG_ERROR("handle rpc request failed: service={}, method={}",
                      request.getServiceName(), request.getMethodName());
            SendErrorResponse(conn, request.getSequenceId(),
                               static_cast<uint32_t>(ErrorCode::SERVICE_NOT_FOUND),
                               Error::getErrorMessage(ErrorCode::SERVICE_NOT_FOUND));
            return;
        }

        RpcResponse response;
        response.setSequenceId(request.getSequenceId());
        response.setErrorCode(static_cast<uint32_t>(ErrorCode::SUCCESS));
        response.setErrorMessage(Error::getErrorMessage(ErrorCode::SUCCESS));
        response.setResultData(result);

        if (!conn->Write(response))
        {
            LOG_ERROR("Failed to send response, sequence_id: {}", request.getSequenceId());
        }
    }

    void MessageCycle::SendErrorResponse(const std::shared_ptr<Connection> &conn, uint32_t sequence_id,
                                          uint32_t error_code, const std::string &error_message)
    {
        RpcResponse response;
        response.setSequenceId(sequence_id);
        response.setErrorCode(error_code);
        response.setErrorMessage(error_message);

        if (!conn->Write(response))
        {
            LOG_ERROR("Failed to send error response, sequence_id: {}", sequence_id);
        }
    }

    void MessageCycle::HandleClientData(int fd)
    {
        auto conn = connection_manager_->GetConnection(fd);
        if (!conn || !conn->Read())
        {
            RemoveConnection(fd);
            return;
        }

        // 一次 Read() 可能带回多条消息，也可能只带回半条，循环切帧直到不够一条为止
        while (true)
        {
            std::string frame;
            auto status = conn->ExtractFrame(frame);
            if (status == Connection::FrameStatus::kIncomplete)
            {
                break;
            }
            if (status == Connection::FrameStatus::kError)
            {
                RemoveConnection(fd);
                return;
            }

            RpcRequest request;
            if (!request.Deserialize(frame))
            {
                LOG_ERROR("Failed to deserialize request, fd: {}", fd);
                RemoveConnection(fd);
                return;
            }

            HandleRpcRequest(conn, request);
        }
    }

    void MessageCycle::RemoveConnection(int fd)
    {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        connection_manager_->RemoveConnection(fd);
    }

    void MessageCycle::HandleEpollEvents(struct epoll_event *events, int nfds)
    {
        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                LOG_ERROR("event error on fd: {}", fd);
                RemoveConnection(fd);
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (listen_fds_.find(fd) != listen_fds_.end())
                {
                    HandleNewConnection(fd);
                    continue;
                }
            }

            if (events[i].events & EPOLLIN)
            {
                HandleClientData(fd);
            }
        }
    }

    void MessageCycle::Loop()
    {
        LOG_INFO("Message cycle loop started");
        running_ = true;

        struct epoll_event events[MAX_EVENTS];
        while (running_)
        {
            int nev = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000); // 1秒超时，便于检查 running_
            if (nev < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                LOG_ERROR("epoll_wait error: {}", strerror(errno));
                break;
            }

            if (nev > 0)
            {
                HandleEpollEvents(events, nev);
            }
        }

        LOG_INFO("Message cycle loop ended");
    }

    void MessageCycle::Stop()
    {
        running_ = false;
    }

} // namespace myrpc
