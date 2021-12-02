//
// Created by fuyoucheng on 2021/3/23.
//

#ifndef FVISIONA_STEREO_STEREO_H
#define FVISIONA_STEREO_STEREO_H

#include <memory>
#include "Define.h"
#include "StereoImage.h"

class Stereo {
public:

    static const std::string ErrorCode_InvalidImage;
    static const std::string ErrorCode_CalibrationFailed;

    class Callback {
    public:
        virtual void onSucceed(const std::vector<std::string>& keys) = 0;
        virtual void onError(const std::string& code, const std::string& msg) = 0;
        virtual void onInfo(const std::string& info) = 0;
    };

public:
    Stereo(bool isDebugInfo, Callback* callback);
    virtual ~Stereo();

    void load(const std::string& dir);

    void process();

    const std::vector<IdxPair>& getIdxPair() {
        return idxPairs;
    }
    const std::vector<WordPtList>& getWorldPtsList() {
        return worldPtsList;
    }
    const std::vector<ColorList>& getColorsList() {
        return colorsList;
    }
    const std::vector<cv::Mat>& getKMats() {
        return kMats;
    }
    const std::vector<cv::Mat>& getRMats() {
        return rMats;
    }
    const std::vector<cv::Mat>& getTVecs() {
        return tVecs;
    }

protected:
    void printTag(const char *infoFormat, ...);

    void matchFeatures(const StereoImage& image0, const StereoImage& image1, const IdxPair& idxPair,
                       int maxFeatureCount, double distanceThreshold,
                       KeyPointList& imgPts0, KeyPointList& imgPts1, MatchList& matchList);

    void normalizeTVecs();

    void updateInliers(const std::vector<KeyPointListPair>& imgPtsPairs,
                       const std::vector<MatchList>& matchLists,
                       const std::vector<std::vector<int>> inlierIdxsList,
                       std::vector<KeyPointListPair>& imgPtsPairsInlier,
                       std::vector<MatchList>& matchListsInlier);

    void checkErr(const std::vector<KeyPointListPair>& imgPtsPairsInlier);

protected:
    std::unique_ptr<Callback> callbackPtr;
    const bool isParallel;
    const bool isDebugInfo;
    int64_t startTimestamp;

    std::vector<StereoImage> stereoImages;
    std::vector<IdxPair> idxPairs;

    std::vector<cv::Mat> kMats, rMats, tVecs;
    std::vector<WordPtList> worldPtsList;
    std::vector<ColorList> colorsList;

};


#endif //FVISIONA_STEREO_STEREO_H
