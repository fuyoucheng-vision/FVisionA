//
// Created by fuyoucheng on 2021/3/22.
//

#ifndef FVISIONA_STEREO_UTIL_H
#define FVISIONA_STEREO_UTIL_H

#include <string>
#include <vector>
#include "json/json.h"
#include "Define.h"
#include "StereoImage.h"

class Util {
public:
    static void setupDebugDir(const std::string& dir);

    static std::vector<std::string> getImageFiles(const std::string& dir);

    static int64_t getTimestamp();

    static cv::Mat loadMat(const Json::Value& node, int rows, int cols);
    static void printMat(const cv::Mat& mat, const char *titleFormat, ...);

    static void makePairs(int range, std::vector<IdxPair>& pairs);
    static void makeTriple(const std::vector<IdxPair>& idxPairs,
                           std::vector<IdxTriple>& idxTriples,
                           std::vector<std::tuple<IdxPair, IdxPair, IdxPair>>& idxPairsTriples);

    template <typename T>
    static bool contains(const std::tuple<T, T>& data, const T& d) {
        return d == std::get<0>(data) || d == std::get<1>(data);
    }
    template <typename T>
    static bool contains(const std::tuple<T, T, T>& data, const T& d) {
        return d == std::get<0>(data) || d == std::get<1>(data) || d == std::get<2>(data);
    }

    static void saveDebugImage(const cv::Mat& image, const char *fileNameFormat, ...);
    static void saveDebugMat(const cv::Mat& mat, const char *fileNameFormat, ...);
    static void saveDebugPly(const std::vector<cv::Mat>& pts, const std::vector<cv::Vec3b>& colors, const char *fileNameFormat, ...);

    static inline double degrees(double radius) {
        return radius / 3.14159265359 * 180.0;
    }
    static inline double radians(double degree) {
        return degree / 180.0 * 3.14159265359;
    }

    static std::vector<double> rotationMatrixToEuler(const cv::Mat& rMat);
    static cv::Mat eulerToRotationMatrix(const std::vector<double>& eular);

    static bool solve(const cv::Mat& A, const cv::Mat& b, cv::Mat& x);
    static cv::Mat solveZ(const cv::Mat& A);

    static cv::Mat toGray(const cv::Mat& image);

    static cv::Mat makeTx(double t0, double t1, double t2);

    static double checkErrEMat(const KeyPointListPair& imgPtsPair,
                               const MatPair& kMatPair,
                               const MatPair& rMatPair,
                               const cv::Mat& twTrans);
    static std::tuple<double, double> checkErrRecon(const KeyPointListPair& imgPtsPair,
                                                    const MatPair& kMatPair,
                                                    const MatPair& rMatPair,
                                                    const cv::Mat& twTrans);

    static void reconstructPt3D(const KeyPointListPair& imgPtsPair,
                                const MatPair& kMatPair, const MatPair& rMatPair, const cv::Mat& twTrans,
                                cv::Mat* pt3D, cv::Mat* errRecon, cv::Mat* errImgPt);

    static void normalizeTVecs(const std::vector<cv::Mat>& rMats, const std::vector<cv::Mat>& tVecs,
                               const std::vector<IdxPair>& idxPairs, const double twTransStd,
                               std::vector<cv::Mat>& tVecsNormalized);

    static void getImgPtsInlier(const KeyPointListPair& imgPtsPair, const std::vector<int>& inlierIdxs,
                                KeyPointListPair& imgPtsPairInlier);

    static double cosTZ(const MatPair& rMatPair, const MatPair& tVecPair);

    static cv::Mat generateMaskRect(const cv::Size& maskSize, const cv::Size& center, const cv::Size& size);
    static cv::Mat generateMaskEllipse(const cv::Size& maskSize, const cv::Size& center, const cv::Size& size);

private:
    static std::string debugDir;
};


#endif //FVISIONA_STEREO_UTIL_H
