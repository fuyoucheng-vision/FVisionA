//
// Created by fuyoucheng on 2021/3/22.
//

#ifndef FVISIONA_STEREO_STEREOIMAGE_H
#define FVISIONA_STEREO_STEREOIMAGE_H

#include <string>
#include "Define.h"

class StereoImage {
public:

    class CaptureInfo {
    public:
        double sensorOrientation;
        double focal;
        double physicalSize[2];
        cv::Mat rMat;
    };

    /******************************************************************/

    StereoImage();
    virtual ~StereoImage();

    void print();

    bool load(const std::string& path);

    void detectFeaturePoints();

    /******************************************************************/

    const cv::Mat& getImage() const {
        return image;
    }

    const CaptureInfo& getCaptureInfo() const {
        return captureInfo;
    }

    const KeyPointList& getKeyPts() const {
        return keyPts;
    }

    const cv::Mat& getImageDesc() const {
        return imageDesc;
    }

private:
    std::string path;
    cv::Mat image;
    CaptureInfo captureInfo;

    KeyPointList keyPts;
    cv::Mat imageDesc;

};


#endif //FVISIONA_STEREO_STEREOIMAGE_H
