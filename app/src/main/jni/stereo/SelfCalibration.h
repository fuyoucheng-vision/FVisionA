//
// Created by fuyoucheng on 2021/3/25.
//

#ifndef FVISIONA_STEREO_SELFCALIBRATION_H
#define FVISIONA_STEREO_SELFCALIBRATION_H

#include <vector>
#include "Define.h"
#include "StereoImage.h"

class SelfCalibration {
public:
    SelfCalibration(bool isParallel, bool isDebugInfo);
    virtual ~SelfCalibration();

    bool calibration(const std::vector<StereoImage>& stereoImages,
                     const std::vector<KeyPointListPair>& imgPtsPairs,
                     const std::vector<IdxPair>& idxPairs,
                     std::vector<cv::Mat>& kMats, std::vector<cv::Mat>& rMats, std::vector<cv::Mat>& tVecs,
                     std::vector<std::vector<int>>& inlierIdxsList);

protected:
    bool calEssentialMat(const std::vector<cv::Mat>& kMats,
                         const std::vector<KeyPointListPair>& imgPtsPairs,
                         const std::vector<IdxPair>& idxPairs,
                         std::vector<std::vector<int>>& inlierIdxsList);

    void calTTrans(const std::vector<cv::Mat>& kMats, const std::vector<cv::Mat>& rMats, const std::vector<cv::Mat>& tVecs,
                   const std::vector<KeyPointListPair>& imgPtsPairs,
                   const std::vector<IdxPair>& idxPairs,
                   const std::vector<std::vector<int>>& inlierIdxsList,
                   std::vector<cv::Mat>& twTransList);

    cv::Mat calTTransIter(const KeyPointListPair& imgPtsPair,
                          const MatPair& kMatPair,
                          const MatPair& rMatPair);

    void checkTOrder(const std::vector<cv::Mat>& kMats, const std::vector<cv::Mat>& rMats, std::vector<cv::Mat>& tVecs,
                     const std::vector<KeyPointListPair>& imgPtsPairs,
                     const std::vector<IdxPair>& idxPairs,
                     const std::vector<std::vector<int>>& inlierIdxsList);

    bool alignT(const std::vector<cv::Mat>& tTransList, const std::vector<cv::Mat>& rMats, const std::vector<IdxPair>& idxPairs,
                std::vector<cv::Mat>& tVecs);

    void filterByDisparity(const std::vector<cv::Mat>& kMats, const std::vector<cv::Mat>& rMats, const std::vector<cv::Mat>& tVecs,
                           const std::vector<KeyPointListPair>& imgPtsPairs,
                           const std::vector<IdxPair>& idxPairs,
                           const cv::Size& imageSize,
                           std::vector<std::vector<int>>& inlierIdxsList);
    void filterBySpread(const std::vector<KeyPointListPair>& imgPtsPairs,
                        const std::vector<IdxPair>& idxPairs,
                        std::vector<std::vector<int>>& inlierIdxsList);

private:
    bool isParallel;
    bool isDebugInfo;

};


#endif //FVISIONA_STEREO_SELFCALIBRATION_H
