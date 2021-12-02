//
// Created by fuyoucheng on 2021/3/29.
//

#ifndef FVISIONA_STEREO_DISPARITYUTIL_H
#define FVISIONA_STEREO_DISPARITYUTIL_H

#include "Define.h"
#include "StereoImage.h"

class DisparityUtil {
public:
    static void calDisparity(const std::tuple<const StereoImage&, const StereoImage&>& imagePair,
                             const IdxPair& idxPair,
                             const KeyPointListPair& imgPtsPair,
                             const MatPair& kMatPair, const MatPair& rMatPair, const MatPair& tVecPair,
                             bool isDebugInfo,
                             std::tuple<cv::Mat, cv::Mat>& imgRectifyPair, cv::Mat& disparity, cv::Mat& disparityMask,
                             bool& isVertical, bool& isReverse, std::tuple<int, int>& disparityRange,
                             MatPair& rMatPairNew, MatPair& kMatPairNew, cv::Mat& Q);

    static void rectify(const KeyPointListPair& imgPtsPair,
                        const IdxPair& idxPair,
                        const MatPair& kMatPair, const MatPair& rMatPair, const MatPair& tVecPair,
                        const cv::Size& imageSize,
                        std::vector<cv::Mat>& r, std::vector<cv::Mat>& p, cv::Mat& Q,
                        std::vector<double>& imgPtsDisparity, std::vector<double>& imgPtsDisparityErr);

    static void reconstruct(const std::tuple<cv::Mat, cv::Mat>& imgRectifyPair,
                            const IdxPair& idxPair,
                            const cv::Mat& disparity, const cv::Mat& disparityMask,
                            const bool& isVertical, const bool& isReverse, const std::tuple<int, int>& disparityRange,
                            const MatPair& rRectifyPair,
                            const MatPair& kMatPair, const MatPair& rMatPair, const MatPair& tVecPair,
                            WordPtList& pts, ColorList& colors);

protected:
    static cv::Mat remapPt(const cv::KeyPoint& kp, const cv::Mat& T);

    static void generateDisparityMask(const cv::Mat& imgDisparity, cv::Mat& disparityMask);

    static cv::Mat reconstruct(double x, double y, double disparity, bool isVertical, bool isReverse,
                               cv::Mat& h, const cv::Mat& kr, const cv::Mat& kt);
    static cv::Mat reconstruct(double x, double y, double disparity, bool isVertical, bool isReverse,
                               cv::Mat& krh, const cv::Mat& kt);

};


#endif //FVISIONA_STEREO_DISPARITYUTIL_H
