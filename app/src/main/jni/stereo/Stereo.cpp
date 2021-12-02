//
// Created by fuyoucheng on 2021/3/23.
//

#include "Stereo.h"
#include <thread>
#include "../FLog.h"
#include "Util.h"
#include "SelfCalibration.h"
#include "Optimization.h"
#include "DisparityUtil.h"

using namespace std;
using namespace cv;

const string Stereo::ErrorCode_InvalidImage         = "E00010";
const string Stereo::ErrorCode_CalibrationFailed    = "E00011";

Stereo::Stereo(bool _isDebugInfo, Callback* _callback)
        : callbackPtr(_callback), isParallel(true), isDebugInfo(_isDebugInfo), startTimestamp(Util::getTimestamp()) {
    printTag("start");
}

Stereo::~Stereo() {

}

void Stereo::load(const string& dir) {
    if (isDebugInfo) {
        Util::setupDebugDir(dir);
    }
    stereoImages.clear();
    vector<string> imageFiles = Util::getImageFiles(dir);
    for (auto &imageFile : imageFiles) {
        stereoImages.emplace_back(StereoImage());
    }
    mutex mt;
    auto funcLoad = [&](int i) {
        mt.lock();
        StereoImage &stereoImage = stereoImages[i];
        string &imageFile = imageFiles[i];
        mt.unlock();
        stereoImage.load(imageFile);
    };
    if (isParallel) {
        vector<thread*> ths;
        for (size_t i=0; i<imageFiles.size(); i++) {
            ths.emplace_back(new thread(funcLoad, i));
        }
        for (auto th : ths) {
            th->join();
        }
    } else {
        for (size_t i=0; i<imageFiles.size(); i++) {
            funcLoad(i);
        }
    }
    printTag("load image");
}

void Stereo::process() {
    kMats.clear();
    rMats.clear();
    tVecs.clear();
    worldPtsList.clear();
    colorsList.clear();

    idxPairs.clear();
    Util::makePairs(stereoImages.size(), idxPairs);
    if (idxPairs.empty()) {
        FLOGE("makePairs : no pairs");
        callbackPtr->onError(ErrorCode_InvalidImage, "Invalid input image");
        return;
    }

    // detect feature points
    for (auto &stereoImage : stereoImages) {
        stereoImage.detectFeaturePoints();
        stereoImage.print();
    }
    printTag("detect feature points");

    // match key points
    int maxFeatureCount = 200;
    double distanceThreshold = 0.3;
    vector<KeyPointListPair> imgPtsPairs;
    vector<MatchList> matchLists;
    for (auto &idxPair : idxPairs) {
        imgPtsPairs.emplace_back(MAKE_KEY_POINT_LIST_PAIR(KeyPointList(), KeyPointList()));
        auto& [imgPts0, imgPts1] = *imgPtsPairs.rbegin();
        matchLists.emplace_back(MatchList());
        MatchList& matchVec = *matchLists.rbegin();
        matchFeatures(stereoImages[IDX_PAIR_0(idxPair)], stereoImages[IDX_PAIR_1(idxPair)], idxPair,
                      maxFeatureCount, distanceThreshold, imgPts0, imgPts1, matchVec);
    }
    printTag("matching key points");

    // self calibration
    SelfCalibration selfCali(isParallel, isDebugInfo);
    vector<vector<int>> inlierIdxsList;
    if (!selfCali.calibration(stereoImages, imgPtsPairs, idxPairs, kMats, rMats, tVecs, inlierIdxsList)) {
        FLOGE("calibration : failed");
        callbackPtr->onError(ErrorCode_CalibrationFailed, "Calibration failed");
        return;
    }

    normalizeTVecs();

    vector<KeyPointListPair> imgPtsPairsInlier;
    vector<MatchList> matchListsInlier;
    updateInliers(imgPtsPairs, matchLists, inlierIdxsList, imgPtsPairsInlier, matchListsInlier);
    printTag("self calibration");

    // check error (after self calibration)
    if (isDebugInfo) {
        checkErr(imgPtsPairsInlier);
        printTag("check error (after self calibration)");
    }

    // optimization
    Optimization optimization(isParallel);
    optimization.optimize(kMats, rMats, tVecs, idxPairs, imgPtsPairsInlier, inlierIdxsList);

    normalizeTVecs();
    printTag("optimization");

    // check error (after optimization)
    if (isDebugInfo) {
        checkErr(imgPtsPairsInlier);
        printTag("check error (after optimization)");
    }

    // adjust r
    for (auto& rMat : rMats) {
        rMat *= (Mat_<double>(3, 3) << 1,0,0, 0,-1,0, 0,0,-1);
    }

    // rectify & disparity
    auto funcDisparity = [&](const int i) {
        auto &idxPair = idxPairs[i];
        auto [idx0, idx1] = idxPair;

        // stereo rectify, stereo matching
        tuple<Mat, Mat> imgRectifyPair;
        Mat disparity, disparityMask;
        bool isVertical, isReverse;
        tuple<int, int> disparityRange;
        MatPair rMatPair, kMatPair;
        Mat Q;
        DisparityUtil::calDisparity(make_tuple(stereoImages[idx0], stereoImages[idx1]),
                                    idxPair, imgPtsPairsInlier[i],
                                    MAKE_MAT_PAIR(kMats[idx0], kMats[idx1]),
                                    MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]),
                                    MAKE_MAT_PAIR(tVecs[idx0], tVecs[idx1]),
                                    isDebugInfo,
                                    imgRectifyPair, disparity, disparityMask,
                                    isVertical, isReverse, disparityRange,
                                    rMatPair, kMatPair, Q);
        printTag("stereo rectify & stereo matching : [%d-%d]", idx0, idx1);

        // reconstruction
        WordPtList pts;
        ColorList colors;
        DisparityUtil::reconstruct(imgRectifyPair, idxPair, disparity, disparityMask, isVertical, isReverse, disparityRange, rMatPair,
                                   kMatPair, MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]), MAKE_MAT_PAIR(tVecs[idx0], tVecs[idx1]),
                                   pts, colors);
        worldPtsList.emplace_back(pts);
        colorsList.emplace_back(colors);
        if (isDebugInfo) {
            Util::saveDebugPly(pts, colors, "reconstruction_%d-%d", idx0, idx1);
        }
        printTag("reconstruction : [%d-%d]", idx0, idx1);
    };

    if (isParallel) {
        vector<thread*> ths;
        for (int i=0; i<idxPairs.size(); i++) {
            ths.emplace_back(new thread(funcDisparity, i));
        }
        for (auto th : ths) {
            th->join();
        }
    } else {
        for (int i=0; i<idxPairs.size(); i++) {
            funcDisparity(i);
        }
    }
    printTag("finish");

    //
    vector<string> keys;
    keys.reserve(idxPairs.size());
    char idxStr[100] = {0};
    for (auto & idxPair : idxPairs) {
        snprintf(idxStr, 100, "%d-%d", IDX_PAIR_0(idxPair), IDX_PAIR_1(idxPair));
        keys.emplace_back(idxStr);
    }
    callbackPtr->onSucceed(keys);
}

void Stereo::printTag(const char *infoFormat, ...) {
    char info[1024] = {0};
    va_list argList;
    va_start(argList, infoFormat);
    vsnprintf(info, 1024, infoFormat, argList);
    va_end(argList);

    FLOGD("=================================================== %s %ld", info, Util::getTimestamp() - startTimestamp);
    callbackPtr->onInfo(info);
}

void Stereo::matchFeatures(const StereoImage& image0, const StereoImage& image1, const IdxPair& idxPair,
                           int maxFeatureCount, double distanceThreshold,
                           KeyPointList& imgPts0, KeyPointList& imgPts1, MatchList& matchList) {
//    FlannBasedMatcher matcher;
    BFMatcher matcher;
    MatchList matches;
    matcher.match(image0.getImageDesc(), image1.getImageDesc(), matches);
    sort(matches.begin(), matches.end(), [](const DMatch& m0, const DMatch& m1) {
        return m0.distance < m1.distance;
    });
    if (isDebugInfo) {
        Mat matchImage;
        drawMatches(image0.getImage(), image0.getKeyPts(), image1.getImage(), image1.getKeyPts(),
                    MatchList(matches.begin(), matches.begin() + min((int)matches.size(), maxFeatureCount)), matchImage);
        Util::saveDebugImage(matchImage, "match_%d-%d", IDX_PAIR_0(idxPair), IDX_PAIR_1(idxPair));
    }

    int count = min((int)matches.size(), maxFeatureCount);
    for (int i=0; i<count; i++) {
        DMatch& match = matches[i];
        if (match.distance >= distanceThreshold) {
            break;
        }
        imgPts0.push_back(image0.getKeyPts()[match.queryIdx]);
        imgPts1.push_back(image1.getKeyPts()[match.trainIdx]);
        matchList.push_back(match);
    }
    FLOGD("matchFeatures : [%d-%d] : %d", IDX_PAIR_0(idxPair), IDX_PAIR_1(idxPair), imgPts0.size());
}

void Stereo::normalizeTVecs() {
    vector<Mat> tVecNormalized;
    Util::normalizeTVecs(rMats, tVecs, idxPairs, 0.1, tVecNormalized);
    tVecs = tVecNormalized;
}

void Stereo::updateInliers(const vector<KeyPointListPair>& imgPtsPairs,
                           const vector<MatchList>& matchLists,
                           vector<vector<int>> inlierIdxsList,
                           vector<KeyPointListPair>& imgPtsPairsInlier,
                           vector<MatchList>& matchListsInlier) {
    for (size_t i=0; i<idxPairs.size(); i++) {
        const auto& [imgPts0, imgPts1] = imgPtsPairs[i];
        const MatchList &matchVec = matchLists[i];
        KeyPointList imgPts0Inlier, imgPts1Inlier;
        MatchList matchListInlier;
        auto inlierIdxs = inlierIdxsList[i];
        for (int idx : inlierIdxs) {
            imgPts0Inlier.emplace_back(imgPts0[idx]);
            imgPts1Inlier.emplace_back(imgPts1[idx]);
            matchListInlier.emplace_back(matchVec[idx]);
        }
        imgPtsPairsInlier.emplace_back(MAKE_KEY_POINT_LIST_PAIR(imgPts0Inlier, imgPts1Inlier));
        matchListsInlier.push_back(matchListInlier);
        if (isDebugInfo) {
            StereoImage &image0 = stereoImages[IDX_PAIR_0(idxPairs[i])], &image1 = stereoImages[IDX_PAIR_1(idxPairs[i])];
            Mat matchImage;
            drawMatches(image0.getImage(), image0.getKeyPts(), image1.getImage(), image1.getKeyPts(), matchListInlier, matchImage);
            Util::saveDebugImage(matchImage, "inlier_%d-%d", IDX_PAIR_0(idxPairs[i]), IDX_PAIR_1(idxPairs[i]));
        }
    }
}

void Stereo::checkErr(const vector<KeyPointListPair>& imgPtsPairsInlier) {
    vector<Mat> tVecsNormalized;
    Util::normalizeTVecs(rMats, tVecs, idxPairs, 1, tVecsNormalized);

    for (int i=0; i<idxPairs.size(); i++) {
        auto [idx0, idx1] = idxPairs[i];
        Mat twTrans = - rMats[idx1].t() * tVecsNormalized[idx1] + rMats[idx0].t() * tVecsNormalized[idx0];
        double errEMat = Util::checkErrEMat(imgPtsPairsInlier[i],
                                            MAKE_MAT_PAIR(kMats[idx0], kMats[idx1]),
                                            MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]),
                                            twTrans);
        tuple<double, double> errRecon = Util::checkErrRecon(imgPtsPairsInlier[i],
                                                             MAKE_MAT_PAIR(kMats[idx0], kMats[idx1]),
                                                             MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]),
                                                             twTrans);
        FLOGD("check err : (%d, %d) : errEMat = %f, errRecon = %f, errImgPt = %f", idx0, idx1, errEMat, get<0>(errRecon), get<1>(errRecon));
    }
}

