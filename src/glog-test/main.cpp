#include <stdio.h>
#include "glog/logging.h"


int main(int argc, char* argv[])
{
    google::InitGoogleLogging((const char *)argv[0]);  // 参数为自己的可执行文件名
    google::SetLogDestination(google::GLOG_INFO, "./Log/");
    LOG(INFO) << "Glog test INFO";
    LOG(INFO) << "Glog test INFO 2";
    LOG(INFO) << "中文测试";
    LOG(INFO) << L"宽字符测试";

    for (int i = 0; i < 100; ++i) {
        LOG(INFO) << "test google glog" << i << " cookies";
    }

    LOG(INFO) << "This is a <Warn> log message...";
    google::ShutdownGoogleLogging();
}