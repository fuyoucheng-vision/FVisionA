//
// Created by fuyoucheng on 2021/3/26.
//

#ifndef FVISIONA_STEREO_OPTIMIZATION_H
#define FVISIONA_STEREO_OPTIMIZATION_H

#include "Define.h"

class Optimization {
public:
    Optimization(bool isParallel);
    virtual ~Optimization();

    void optimize(std::vector<cv::Mat>& kMats, std::vector<cv::Mat>& rMats, std::vector<cv::Mat>& tVecs,
                  const std::vector<IdxPair>& idxPairs,
                  const std::vector<KeyPointListPair>& imgPtsPairs,
                  const std::vector<std::vector<int>>& inlierIdxsList);

protected:
    virtual cv::Mat onStart(const std::vector<cv::Mat>& kMats, const std::vector<cv::Mat>& rMats, const std::vector<cv::Mat>& tVecs,
                            const std::vector<IdxPair>& idxPairs,
                            const std::vector<KeyPointListPair>& imgPtsPairs,
                            const std::vector<std::vector<int>>& inlierIdxsList);

    virtual void onFinish(const cv::Mat& params,
                          std::vector<cv::Mat>& kMats, std::vector<cv::Mat>& rMats, std::vector<cv::Mat>& tVecs);

    virtual void calculate(const cv::Mat& params, cv::Mat& f, cv::Mat& jacobian);

    void calHLists(const std::vector<KeyPointListPair>& imgPtsPairs);

    void calKRKTdKRdKT(const cv::Mat& params,
                       std::vector<cv::Mat>& KRs, std::vector<cv::Mat>& KTs,
                       std::vector<cv::Mat>& dKRs, std::vector<cv::Mat>& dKTs);

    void calKRKTdKRdKTFull(const std::vector<cv::Mat>& KRs, const std::vector<cv::Mat>& KTs,
                           const std::vector<cv::Mat>& dKRs, const std::vector<cv::Mat>& dKTs,
                           const IdxPair& idxPair,
                           cv::Mat& KRFull, cv::Mat& KTFull,
                           std::vector<cv::Mat>& dKRFulls, std::vector<cv::Mat>& dKTFulls);

private:
    void optimize(cv::Mat& params);

protected:
    bool isParallel;

    int paramsCount;
    int maxIter;
    double epsilon1;
    double epsilon2;
    double muScale;
    double nuInit;
    double nuScale;

    double theta;

    const std::vector<IdxPair>* idxPairsPtr;
    const std::vector<KeyPointListPair>* imgPtsPairsPtr;
    int cameraCount;

    std::vector<std::vector<cv::Mat>> hLists;

};


#endif //FVISIONA_STEREO_OPTIMIZATION_H
