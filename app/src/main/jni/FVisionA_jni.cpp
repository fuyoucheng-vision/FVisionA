//
// Created by FuYoucheng on 13/2/2021.
//

#include <android/log.h>
#include <jni.h>
#include <string>
#include <vector>
#include "FLog.h"
#include "cardboard/util.h"
#include "cardboard/CardboardWrapper.h"
#include "capture/Capture.h"
#include "stereo/Stereo.h"
#include "display/Renderer.h"

using namespace std;
using namespace cv;

namespace {

JavaVM *javaVm;

inline jlong jptr(fvision::CardboardWrapper *native_app) {
    return reinterpret_cast<intptr_t>(native_app);
}

inline fvision::CardboardWrapper *native(jlong ptr) {
    return reinterpret_cast<fvision::CardboardWrapper *>(ptr);
}

string getString(JNIEnv* env, jstring jstr) {
    const char* sz = env->GetStringUTFChars(jstr, nullptr);
    string str(sz);
    env->ReleaseStringUTFChars(jstr, sz);
    return str;
}

}

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM * vm , void *reserved) {
    FLOGD("JNI_OnLoad");
    javaVm = vm;
    return JNI_VERSION_1_6;
}

/******************************************************************/
// CardboardWrapper

JNIEXPORT jlong JNICALL Java_com_fyc_fvision_capture_sensor_CardboardWrapper_nativeCreate(JNIEnv* env, jobject obj) {
    return jptr(new fvision::CardboardWrapper(javaVm, obj));
}

JNIEXPORT void JNICALL Java_com_fyc_fvision_capture_sensor_CardboardWrapper_nativeDestory(JNIEnv* env, jobject obj, jlong handler) {
    delete native(handler);
}

JNIEXPORT void JNICALL Java_com_fyc_fvision_capture_sensor_CardboardWrapper_nativePause(JNIEnv* env, jobject obj, jlong handler) {
    native(handler)->pause();
}

JNIEXPORT void JNICALL Java_com_fyc_fvision_capture_sensor_CardboardWrapper_nativeResume(JNIEnv* env, jobject obj, jlong handler) {
    native(handler)->resume();
}

JNIEXPORT jfloatArray JNICALL Java_com_fyc_fvision_capture_sensor_CardboardWrapper_nativeGetRotationMatrix(JNIEnv* env, jobject obj, jlong handler) {
    ndk_hello_cardboard::Matrix4x4 rotationMatrix = native(handler)->getPos();
//    FLOGD("nativeGetRotationMatrix : %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f; %f, %f, %f, %f",
//          rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2], rotationMatrix.m[0][3],
//          rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2], rotationMatrix.m[1][3],
//          rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2], rotationMatrix.m[2][3],
//          rotationMatrix.m[3][0], rotationMatrix.m[3][1], rotationMatrix.m[3][2], rotationMatrix.m[3][3]);
    jfloatArray jarr = env->NewFloatArray(9);
    float *arr = new float[9];
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            arr[i*3+j] = rotationMatrix.m[j][i]; // 要转置一下，默认是列主序
        }
    }
    env->SetFloatArrayRegion(jarr, 0, 9, arr);
    delete []arr;
    return jarr;
}

/******************************************************************/
// Capture

JNIEXPORT void JNICALL Java_com_fyc_fvision_capture_CaptureUtil_00024ImageSaver_nativeSaveImageYUV420888(
        JNIEnv *env, jobject obj,
        jbyteArray jplaneY, jbyteArray jplaneU, jbyteArray jplaneV,
        jintArray jpixelStrides, jintArray jrowStrides, jint jwidth, jint jheight, jint jtargetHeight, jstring jpath) {
    const unsigned char* planes[] = {
            (unsigned char*) env->GetByteArrayElements(jplaneY, JNI_FALSE),
            (unsigned char*) env->GetByteArrayElements(jplaneU, JNI_FALSE),
            (unsigned char*) env->GetByteArrayElements(jplaneV, JNI_FALSE),
    };
    const int* pixelStrides = env->GetIntArrayElements(jpixelStrides, JNI_FALSE);
    const int* rowStrides = env->GetIntArrayElements(jrowStrides, JNI_FALSE);
    const string && path = getString(env, jpath);

    Capture::saveImageYUV420888(planes, pixelStrides, rowStrides, jwidth, jheight, jtargetHeight, path);
}


JNIEXPORT void JNICALL Java_com_fyc_fvision_capture_CaptureUtil_00024BufferSaver_nativeSaveImageBGRA(
        JNIEnv *env, jobject obj,
        jintArray jbuffer, jint jwidth, jint jheight, jstring jpath) {
    const int* buffer = env->GetIntArrayElements(jbuffer, JNI_FALSE);
    const string && path = getString(env, jpath);

    Capture::saveImageBGRA(buffer, jwidth, jheight, path);
}

/******************************************************************/
// Stereo

class StereoCallback : public Stereo::Callback {
public:
    StereoCallback(jobject _jCallback) {
        jCallback = getEnv()->NewGlobalRef(_jCallback);
    }
    ~StereoCallback() {
        getEnv()->DeleteGlobalRef(jCallback);
    }

    void onSucceed(const vector<string>& keys) {
        JNIEnv *env = getEnv();
        jclass clz = env->GetObjectClass(jCallback);
        jmethodID method = env->GetMethodID(clz, "onSucceed", "([Ljava/lang/String;)V");
        jclass objClass = env->FindClass("java/lang/String");
        jobjectArray keyArray = env->NewObjectArray(keys.size(), objClass, nullptr);
        for (int i=0; i<keys.size(); i++) {
            jstring jstr = env->NewStringUTF(keys[i].c_str());
            env->SetObjectArrayElement(keyArray, i, jstr);
        }
        env->CallVoidMethod(jCallback, method, keyArray);
    }

    void onError(const std::string& code, const std::string& msg) {
        JNIEnv *env = getEnv();
        jclass clz = env->GetObjectClass(jCallback);
        jmethodID method = env->GetMethodID(clz, "onError", "(Ljava/lang/String;Ljava/lang/String;)V");
        jstring jcode = env->NewStringUTF(code.c_str());
        jstring jmsg = env->NewStringUTF(msg.c_str());
        env->CallVoidMethod(jCallback, method, jcode, jmsg);
        env->DeleteLocalRef(jcode);
        env->DeleteLocalRef(jmsg);
    }

    void onInfo(const std::string& info) {
        JNIEnv *env = getEnv();
        jclass clz = env->GetObjectClass(jCallback);
        jmethodID method = env->GetMethodID(clz, "onInfo", "(Ljava/lang/String;)V");
        jstring jinfo = env->NewStringUTF(info.c_str());
        env->CallVoidMethod(jCallback, method, jinfo);
        env->DeleteLocalRef(jinfo);
    }

private:
    JNIEnv* getEnv() {
        JNIEnv *jniEnv;
        int state = javaVm->GetEnv((void**)&jniEnv, JNI_VERSION_1_6);
        if (state != JNI_OK) {
            javaVm->AttachCurrentThread(&jniEnv, NULL);
        }
        return jniEnv;
    }

    jobject jCallback;
};

unique_ptr<Stereo> stereoPtr;

JNIEXPORT void JNICALL Java_com_fyc_fvision_stereo_Stereo_nativeProcess(JNIEnv* env, jobject obj, jstring jdir, jboolean jdebuginfo, jobject jCallback) {
    string && dir = getString(env, jdir);
    FLOGD("nativeProcess : dir : %s", dir.c_str());

    stereoPtr = make_unique<Stereo>(jdebuginfo == JNI_TRUE, new StereoCallback(jCallback));
    stereoPtr->load(dir);
    stereoPtr->process();
}

/******************************************************************/
// FRender

unique_ptr<Renderer> rendererPtr;

JNIEXPORT void JNICALL Java_com_fyc_fvision_display_FRenderer_nativeOnSurfaceCreated(JNIEnv* env, jobject obj, jstring jvsh, jstring jfsh) {
    string && vsh = getString(env, jvsh);
    const string & fsh = getString(env, jfsh);
//    FLOGD("nativeOnSurfaceCreated : vsh : \n%s", vsh.c_str());
//    FLOGD("nativeOnSurfaceCreated : fsh : \n%s", fsh.c_str());

    rendererPtr = make_unique<Renderer>();
    rendererPtr->initGL(vsh, fsh);
}

JNIEXPORT void JNICALL Java_com_fyc_fvision_display_FRenderer_nativeOnSurfaceChanged(JNIEnv* env, jobject obj, jint width, jint height) {
    FLOGD("nativeOnSurfaceChanged : %d, %d", width, height);
    rendererPtr->updateView(width, height);
}

JNIEXPORT void JNICALL Java_com_fyc_fvision_display_FRenderer_nativeOnDrawFramed(JNIEnv* env, jobject obj, jfloat offsetX, jfloat offsetY, jfloat scale) {
//    FLOGD("nativeOnDrawFramed : %f, %f, %f", offsetX, offsetY, scale);
    rendererPtr->draw(offsetX, offsetY, scale);
}

JNIEXPORT void JNICALL Java_com_fyc_fvision_display_FRenderer_nativeLoadVertex(JNIEnv* env, jobject obj) {
    FLOGD("nativeLoadVertex");
    if (stereoPtr != nullptr) {
        rendererPtr->loadVertex(stereoPtr->getIdxPair(), stereoPtr->getWorldPtsList(), stereoPtr->getColorsList());
    } else {
        FLOGE("nativeLoadVertex : stereoPtr == nullptr");
    }
}

JNIEXPORT void JNICALL Java_com_fyc_fvision_display_FRenderer_nativeUpdateVertex(JNIEnv* env, jobject obj, jobjectArray keyArray) {
    vector<string> keys;
    jsize n = env->GetArrayLength(keyArray);
    for (int i=0; i<n; i++) {
        auto key = (jstring)env->GetObjectArrayElement(keyArray, i);
        keys.emplace_back(env->GetStringUTFChars(key, nullptr));
    }
    stringstream ss;
    for (auto &key : keys) {
        ss<<key<<", ";
    }
    FLOGD("nativeUpdateVertex : %s", ss.str().c_str());
    rendererPtr->updateVertex(keys);
}

}
