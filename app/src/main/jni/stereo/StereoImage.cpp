//
// Created by fuyoucheng on 2021/3/22.
//

#include "StereoImage.h"
#include <fstream>
#include "json/json.h"
#include "opencv2/xfeatures2d.hpp"
#include "../FLog.h"
#include "Util.h"

using namespace std;
using namespace cv;

StereoImage::StereoImage() {

}

StereoImage::~StereoImage() {

}

void StereoImage::print() {
    Util::printMat(captureInfo.rMat,
            "StereoImage : %s, [%d, %d], %f, %f, [%f, %f], %d\n",
            path.c_str(), image.cols, image.rows,
            captureInfo.sensorOrientation, captureInfo.focal, captureInfo.physicalSize[0], captureInfo.physicalSize[1],
            keyPts.size());
}

bool StereoImage::load(const string& _path) {
    path = _path;
    bool isSucceed = false;
    do {
        // load image
        image = cv::imread(path);
        if (image.empty()) {
            break;
        }

        // load capture info
        auto dotIdx = path.rfind('.');
        if (dotIdx == string::npos) {
            break;
        }
        string infoPath = path.substr(0, dotIdx) + ".txt";
        ifstream ifs;
        ifs.open(infoPath.c_str());
        if (!ifs.is_open()) {
            break;
        }
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(ifs, root, false)) {
            break;
        }
        ifs.close();
        captureInfo.sensorOrientation = root["sensorOrientation"].asDouble();
        captureInfo.focal = atof(root["focal"].asString().c_str());
        captureInfo.physicalSize[0] = atof(root["physicalSize"][0].asString().c_str());
        captureInfo.physicalSize[1] = atof(root["physicalSize"][1].asString().c_str());
        captureInfo.rMat = Util::loadMat(root["rotationMatrix"], 3, 3);

        isSucceed = true;
    } while(false);
    if (!isSucceed) {
        image.release();
    }
    return isSucceed;
}

void StereoImage::detectFeaturePoints() {
//    auto Feature2D = xfeatures2d::SiftFeatureDetector::create();
    auto feature2D = xfeatures2d::SurfFeatureDetector::create(6000);
    // detect
    feature2D->detect(image, keyPts);
    // extractor
    feature2D->compute(image, keyPts, imageDesc);
}
