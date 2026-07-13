#include "log_manager.h"
#include "zstd_compress.h"
#include "aes_encrypt.h"
#include <iostream>

// compress + encrypt 模块演示：压缩 -> 解压，加密 -> 解密，分别验证正确性
int main()
{
    Logger::GetInstance().Init();

    std::string original = "hello myrpc, this is a test string for compress and encrypt, "
                            "hello myrpc, this is a test string for compress and encrypt.";

    // --- zstd 压缩 ---
    std::string compressed;
    if (!myrpc::ZstdCompress::getInstance().CompressString(original, compressed))
    {
        LOG_ERROR("压缩失败");
        return 1;
    }
    LOG_INFO("压缩: {} 字节 -> {} 字节", original.size(), compressed.size());

    std::string decompressed;
    if (!myrpc::ZstdCompress::getInstance().DecompressString(compressed, decompressed))
    {
        LOG_ERROR("解压失败");
        return 1;
    }
    LOG_INFO("解压结果与原文一致: {}", decompressed == original);

    // --- AES(自定义移位) 加密 ---
    std::string encrypted;
    if (!myrpc::AesEncrypt::getInstance().Encrypt(original, encrypted))
    {
        LOG_ERROR("加密失败");
        return 1;
    }
    LOG_INFO("加密: {} 字节 -> {} 字节(base64), 密文预览: {}",
             original.size(), encrypted.size(), encrypted.substr(0, 32));

    std::string decrypted;
    if (!myrpc::AesEncrypt::getInstance().Decrypt(encrypted, decrypted))
    {
        LOG_ERROR("解密失败");
        return 1;
    }
    LOG_INFO("解密结果与原文一致: {}", decrypted == original);

    return 0;
}
