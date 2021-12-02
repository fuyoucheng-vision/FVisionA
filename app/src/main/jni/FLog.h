//
// Created by FuYoucheng on 13/2/2021.
//

#ifndef FVISIONA_FLOG_H
#define FVISIONA_FLOG_H

#include <android/log.h>

#define FLOGI(...) __android_log_print(ANDROID_LOG_INFO, "flog", __VA_ARGS__)

#define FLOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "flog", __VA_ARGS__)

#define FLOGW(...) __android_log_print(ANDROID_LOG_WARN, "flog", __VA_ARGS__)

#define FLOGE(...) __android_log_print(ANDROID_LOG_ERROR, "flog", __VA_ARGS__)

#endif //FVISIONA_FLOG_H
