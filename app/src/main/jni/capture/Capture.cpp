//
// Created by FuYoucheng on 3/4/2021.
//

#include "Capture.h"
#include "opencv2/opencv.hpp"
#include "../FLog.h"

using namespace std;
using namespace cv;

Capture::Capture() {

}

Capture::~Capture() {

}

void Capture::saveImageYUV420888(const unsigned char* planes[], const int* pixelStrides, const int* rowStrides,
                                 int width, int height, int targetHeight, const string& path) {
    FLOGD("Capture::saveImageYUV420888 : pixelStrides = [%d, %d, %d], rowStrides = [%d, %d, %d], size = (%d, %d), targetHeight = %d, path = %s",
          pixelStrides[0], pixelStrides[1], pixelStrides[2], rowStrides[0], rowStrides[1], rowStrides[2], width, height, targetHeight, path.c_str());

    bool is420sp = pixelStrides[1] > 1;
    const unsigned char* planeY = planes[0];
    const unsigned char* planeU = planes[1];
    const unsigned char* planeV = planes[2];
    Mat img = Mat::zeros(height, width, CV_8UC4);
    int *imgPtr = (int*)img.ptr();

    int *uOffsets = new int[width], *vOffsets = new int[width];
    for (int j=0; j<width; j++) {
        uOffsets[j] = is420sp ? j/pixelStrides[1]*pixelStrides[1] : j/2;
        vOffsets[j] = is420sp ? j/pixelStrides[2]*pixelStrides[2] : j/2;
    }

    for (int i=0; i<height; i++) {
        int yStart = i * rowStrides[0];
        int uStart = i/2 * rowStrides[1];
        int vStart = i/2 * rowStrides[2];
        int imgStart = i * width;
        for (int j=0; j<width; j++) {
            yuv2bgra(planeY[yStart + j], planeU[uStart + uOffsets[j]], planeV[vStart + vOffsets[j]], imgPtr[imgStart + j]);
        }
    }

    Mat imgResize;
    if (height > targetHeight) {
        int targetWidth = int(width / (float)height * targetHeight + 0.5f);
        FLOGD("saveImageYUV420888 : resize (%d, %d) to (%d, %d)", width, height, targetWidth, targetHeight);
        cv::resize(img, imgResize, Size(targetWidth, targetHeight), cv::INTER_CUBIC);
    } else {
        imgResize = img;
    }

    imwrite(path, imgResize);
}

void Capture::saveImageBGRA(const int* buffer, int width, int height, const std::string& path) {
    FLOGD("Capture::saveImageBGRA : size = (%d, %d), path = %s", width, height, path.c_str());

    Mat img = Mat::zeros(height, width, CV_8UC4);
    int *imgPtr = (int*)img.ptr();
    memcpy(imgPtr, buffer, width * height * 4);

    imwrite(path, img);
}

void Capture::yuv2bgra(const unsigned char& y, const unsigned char& u, const unsigned char& v, int& pixel) {
    int r = (int)((y&0xff) + 1.4075 * ((v&0xff)-128));
    int g = (int)((y&0xff) - 0.3455 * ((u&0xff)-128) - 0.7169*((v&0xff)-128));
    int b = (int)((y&0xff) + 1.779 * ((u&0xff)-128));
    r =(r<0? 0: r>255? 255 : r);
    g =(g<0? 0: g>255? 255 : g);
    b =(b<0? 0: b>255? 255 : b);
    pixel = ((0xff000000 | r<<16) | g<<8) | b;
}
