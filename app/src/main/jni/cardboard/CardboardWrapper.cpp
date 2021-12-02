//
// Created by FuYoucheng on 13/2/2021.
//

#include "CardboardWrapper.h"
#include <array>
#include <cmath>
#include "../FLog.h"

using namespace ndk_hello_cardboard;

namespace fvision {

constexpr uint64_t kPredictionTimeWithoutVsyncNanos = 50000000;

CardboardWrapper::CardboardWrapper(JavaVM* vm, jobject obj)
        : headTrackerPtr(nullptr) {
    FLOGD("CardboardWrapper::CardboardWrapper");
    Cardboard_initializeAndroid(vm, obj);
    headTrackerPtr = CardboardHeadTracker_create();
}

CardboardWrapper::~CardboardWrapper() {
    FLOGD("CardboardWrapper::~CardboardWrapper");
    CardboardHeadTracker_destroy(headTrackerPtr);
    headTrackerPtr = nullptr;
}

void CardboardWrapper::pause() {
    FLOGD("CardboardWrapper::pause");
    CardboardHeadTracker_pause(headTrackerPtr);
}

void CardboardWrapper::resume() {
    FLOGD("CardboardWrapper::resume");
    CardboardHeadTracker_resume(headTrackerPtr);
}

Matrix4x4 CardboardWrapper::getPos() {
    std::array<float, 4> out_orientation;
    std::array<float, 3> out_position;
    long monotonic_time_nano = GetMonotonicTimeNano();
    monotonic_time_nano += kPredictionTimeWithoutVsyncNanos;
    CardboardHeadTracker_getPose(headTrackerPtr, monotonic_time_nano,
                                 &out_position[0], &out_orientation[0]);
//    return GetTranslationMatrix(out_position) *
//           Quatf::FromXYZW(&out_orientation[0]).ToMatrix();
    return Quatf::FromXYZW(&out_orientation[0]).ToMatrix(); // 返回的是旋转矩阵的转置
}

}