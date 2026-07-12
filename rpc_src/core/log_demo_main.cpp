#include "log_manager.h"

int main() {
    Logger::GetInstance().Init();
    LOG_INFO("你好，日志模块跑通了");
    LOG_WARN("这是一条警告日志");
    LOG_ERROR("这是一条错误日志");
    CLIENT_INFO("这是客户端专用日志，会带 [CLIENT] 前缀");
    return 0;
}
