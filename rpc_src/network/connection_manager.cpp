#include "connection_manager.h"

namespace myrpc
{

    void ConnectionManager::AddConnection(std::shared_ptr<Connection> conn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_[conn->GetFd()] = conn;
    }

    void ConnectionManager::RemoveConnection(int fd)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.erase(fd);
    }

    std::shared_ptr<Connection> ConnectionManager::GetConnection(int fd)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(fd);
        return it == connections_.end() ? nullptr : it->second;
    }

    size_t ConnectionManager::GetConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }

} // namespace myrpc
