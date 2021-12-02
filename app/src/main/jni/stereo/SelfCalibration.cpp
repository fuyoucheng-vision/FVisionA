//
// Created by fuyoucheng on 2021/3/25.
//

#include "SelfCalibration.h"
#include <numeric>
#include <thread>
#include <random>
#include "../FLog.h"
#include "Util.h"
#include "DisparityUtil.h"

using namespace std;
using namespace cv;

SelfCalibration::SelfCalibration(bool _isParallel, bool _isDebugInfo)
        : isParallel(_isParallel), isDebugInfo(_isDebugInfo) {

}

SelfCalibration::~SelfCalibration() {

}

bool SelfCalibration::calibration(const vector<StereoImage>& stereoImages,
                                  const vector<KeyPointListPair>& imgPtsPairs,
                                  const vector<IdxPair>& idxPairs,
                                  vector<Mat>& kMats, vector<Mat>& rMats, vector<Mat>& tVecs,
                                  vector<vector<int>>& inlierIdxsList) {
    for (int i=0; i<stereoImages.size(); i++) {
        const StereoImage &stereoImage = stereoImages[i];
        const int imageSize[] = {stereoImage.getImage().cols, stereoImage.getImage().rows};
        const double *physicalSize = &stereoImage.getCaptureInfo().physicalSize[0];
        double pixelSize[] = {physicalSize[0] / imageSize[0],
                              physicalSize[1] / imageSize[1]};
        if (imageSize[0] / (double)imageSize[1] >= physicalSize[0] / physicalSize[1]) {
            pixelSize[1] = physicalSize[0] / imageSize[0];
        } else {
            pixelSize[0] = physicalSize[1] / imageSize[1];
        }
        Mat kMat = (Mat_<double>(3, 3) <<
                stereoImage.getCaptureInfo().focal / pixelSize[0], 0, imageSize[0]/2,
                0, stereoImage.getCaptureInfo().focal / pixelSize[1], imageSize[1]/2,
                0, 0, 1);
        FLOGD("calibration : %d, (%d, %d), %f, (%f, %f)",
                i, imageSize[0], imageSize[1], stereoImage.getCaptureInfo().focal, physicalSize[0], physicalSize[1]);
        Util::printMat(kMat, "kMat");
        Util::printMat(stereoImage.getCaptureInfo().rMat, "rMat");
        kMats.emplace_back(kMat);
        rMats.emplace_back(stereoImage.getCaptureInfo().rMat);
    }
    FLOGD("----------------------------------- calibration : prepared");

    // from opengl coordinate (cardbord output) to calibration camera coordinate (rotate 180 around x-axis)
    Mat preRMat = (Mat_<double>(3, 3) <<
            1, 0, 0,
            0, -1, 0,
            0, 0, -1);
    for (int i=0; i<rMats.size(); i++) {
        rMats[i] = preRMat * rMats[i];
    }

    // adjust to camera0 coordinate
    Mat rMatBase = rMats[0].clone();
    for (int i=0; i<rMats.size(); i++) {
        rMats[i] = rMats[i] * rMatBase.t();
    }

    if (!calEssentialMat(kMats, imgPtsPairs, idxPairs, inlierIdxsList)) {
        return false;
    }

    vector<Mat> tTransList;
    calTTrans(kMats, rMats, tVecs, imgPtsPairs, idxPairs, inlierIdxsList, tTransList);

    if(!alignT(tTransList, rMats, idxPairs, tVecs)) {
        return false;
    }

    checkTOrder(kMats, rMats, tVecs, imgPtsPairs, idxPairs, inlierIdxsList);

    FLOGD("----------------------------------- calibration : cal t");

    filterByDisparity(kMats, rMats, tVecs, imgPtsPairs, idxPairs,
                      Size(stereoImages[0].getImage().cols, stereoImages[0].getImage().rows),
                      inlierIdxsList);

//    filterBySpread(imgPtsPairs, idxPairs, inlierIdxsList);

    FLOGD("----------------------------------- calibration : filtered");

    calTTrans(kMats, rMats, tVecs, imgPtsPairs, idxPairs, inlierIdxsList, tTransList);

    if (!alignT(tTransList, rMats, idxPairs, tVecs)) {
        return false;
    }

    checkTOrder(kMats, rMats, tVecs, imgPtsPairs, idxPairs, inlierIdxsList);

    return true;
}

bool SelfCalibration::calEssentialMat(const vector<Mat>& kMats,
                                      const vector<KeyPointListPair>& imgPtsPairs,
                                      const vector<IdxPair>& idxPairs,
                                      vector<vector<int>>& inlierIdxsList) {
    inlierIdxsList.clear();
    inlierIdxsList.reserve(idxPairs.size());
    for (int i=0; i<idxPairs.size(); i++) {
        inlierIdxsList.emplace_back(vector<int>());
    }
    bool ret = true;

    mutex mt;
    auto funcEssentialMat = [&](int i) {
        mt.lock();
        vector<int> &inlierIdxs = inlierIdxsList[i];
        auto &[idx0, idx1] = idxPairs[i];
        auto &[imgKps0, imgKps1] = imgPtsPairs[i];
        if (imgKps0.empty() || imgKps0.size() != imgKps1.size()) {
            FLOGE("calibration : calEssentialMat : invalid key points");
            ret = false;
            mt.unlock();
            return;
        }
        double fx0 = kMats[idx0].at<double>(0, 0), fy0 = kMats[idx0].at<double>(1, 1),
               cx0 = kMats[idx0].at<double>(0, 2), cy0 = kMats[idx0].at<double>(1, 2);
        double fx1 = kMats[idx1].at<double>(0, 0), fy1 = kMats[idx1].at<double>(1, 1),
               cx1 = kMats[idx1].at<double>(0, 2), cy1 = kMats[idx1].at<double>(1, 2);
        mt.unlock();

        vector<Point2f> imgPts0, imgPts1;
        imgPts0.reserve(imgKps0.size());
        imgPts1.reserve(imgKps1.size());
        for (auto &imgKp : imgKps0) {
            imgPts0.emplace_back(Point2f((imgKp.pt.x - cx0) / fx0, (imgKp.pt.y - cy0) / fy0));  // k.inv * pt
        }
        for (auto &imgKp : imgKps1) {
            imgPts1.emplace_back(Point2f((imgKp.pt.x - cx1) / fx1, (imgKp.pt.y - cy1) / fy1));
        }
        double threshold = 4 / (fx0 + fy0 + fx1 + fy1);

        vector<double> sThreshold = {0.2, 0.4, 0.6, 0.8, 1.0};
        for (auto s : sThreshold) {
            FLOGD("eMat : [%d-%d] : sThreshold = %f", idx0, idx1, s);
            Mat mask;
            Mat eMat = findEssentialMat(imgPts0, imgPts1, Mat::eye(3, 3, CV_64FC1), RANSAC, 0.999999, threshold * s, mask);
            inlierIdxs.clear();
            for (int j=0; j<mask.rows; j++) {
                if (mask.at<unsigned char>(j, 0) != 0) {
                    inlierIdxs.push_back(j);
                }
            }
            if (inlierIdxs.size() >= 20) {
                Util::printMat(eMat, "eMat : [%d-%d] :\n", idx0, idx1);
                break;
            }
        }
    };

    if (isParallel) {
        vector<thread*> ths;
        for (size_t i=0; i<idxPairs.size(); i++) {
            ths.emplace_back(new thread(funcEssentialMat, i));
        }
        for (auto th : ths) {
            th->join();
        }
    } else {
        for (size_t i=0; i<idxPairs.size(); i++) {
            funcEssentialMat(i);
        }
    }
    return ret;
}

void SelfCalibration::calTTrans(const vector<Mat>& kMats, const vector<Mat>& rMats, const vector<Mat>& tVecs,
                                const vector<KeyPointListPair>& imgPtsPairs,
                                const vector<IdxPair>& idxPairs,
                                const vector<vector<int>>& inlierIdxsList,
                                vector<Mat>& twTransList) {
    twTransList.clear();
    twTransList.reserve(idxPairs.size());
    for (int i=0; i<idxPairs.size(); i++) {
        auto [idx0, idx1] = idxPairs[i];
        KeyPointListPair imgPtsPairInlier;
        Util::getImgPtsInlier(imgPtsPairs[i], inlierIdxsList[i], imgPtsPairInlier);
        Mat twTrans = calTTransIter(imgPtsPairInlier,
                                    MAKE_MAT_PAIR(kMats[idx0], kMats[idx1]),
                                    MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]));
        twTransList.push_back(twTrans);

        double errEMat = 0;
        tuple<double, double> errRecon = make_tuple(0, 0);
        if (isDebugInfo) {
            errEMat = Util::checkErrEMat(imgPtsPairInlier,
                                         MAKE_MAT_PAIR(kMats[idx0], kMats[idx1]),
                                         MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]),
                                         twTrans);
            errRecon = Util::checkErrRecon(imgPtsPairInlier,
                                           MAKE_MAT_PAIR(kMats[idx0], kMats[idx1]),
                                           MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]),
                                           twTrans);
        }
        Util::printMat(twTrans.t(), "twTrans : [%d-%d] : total = %lu, inlier = %lu, errEMat = %f, errRecon = %f, errImgPt = %f\n",
                       idx0, idx1, KEY_POINT_LIST_PAIR_0(imgPtsPairs[i]).size(), inlierIdxsList[i].size(), errEMat, get<0>(errRecon), get<1>(errRecon));
    }
}

Mat SelfCalibration::calTTransIter(const KeyPointListPair& imgPtsPair,
                                   const MatPair& kMatPair,
                                   const MatPair& rMatPair) {
    const auto& [imgPts0, imgPts1] = imgPtsPair;
    Mat rTrans = MAT_PAIR_1(rMatPair) * MAT_PAIR_0(rMatPair).t();
    Mat A = Mat::zeros(imgPts0.size(), 3, CV_64FC1);
    Mat kMat0Inv = MAT_PAIR_0(kMatPair).inv();
    Mat kMat1Inv_t_rTrans = MAT_PAIR_1(kMatPair).inv().t() * rTrans;

    for (int i=0; i<A.rows; i++) {
        Mat p0 = kMat0Inv * (Mat_<double>(3, 1) << imgPts0[i].pt.x, imgPts0[i].pt.y, 1);
        Mat p1 = (Mat_<double>(1, 3) << imgPts1[i].pt.x, imgPts1[i].pt.y, 1) * kMat1Inv_t_rTrans;
        A.at<double>(i, 0) = Mat(p1 * Util::makeTx(1, 0, 0) * p0).at<double>(0, 0);
        A.at<double>(i, 1) = Mat(p1 * Util::makeTx(0, 1, 0) * p0).at<double>(0, 0);
        A.at<double>(i, 2) = Mat(p1 * Util::makeTx(0, 0, 1) * p0).at<double>(0, 0);
    }
    for (int i=0; i<A.rows; i++) {
        A.row(i) /= norm(A.row(i));
    }

    Mat tTrans = Util::solveZ(A);

    Mat twTrans = MAT_PAIR_0(rMatPair).t() * tTrans;
    twTrans = twTrans / norm(twTrans);
    return twTrans;
}

void SelfCalibration::checkTOrder(const vector<Mat>& kMats, const vector<Mat>& rMats, vector<Mat>& tVecs,
                                  const vector<KeyPointListPair>& imgPtsPairs,
                                  const vector<IdxPair>& idxPairs,
                                  const vector<vector<int>>& inlierIdxsList) {
    int backwardCount = 0, totalCount = 0;
    Mat zStandard = (Mat_<double>(3, 1) << 0, 0, 1);

    for (int i=0; i<idxPairs.size(); i++) {
        auto [idx0, idx1] = idxPairs[i];
        KeyPointListPair imgPtsPairInlier;
        Util::getImgPtsInlier(imgPtsPairs[i], inlierIdxsList[i], imgPtsPairInlier);
        int totalCountSub = KEY_POINT_LIST_PAIR_0(imgPtsPairInlier).size();
        Mat twTrans = - rMats[idx1].t() * tVecs[idx1] + rMats[idx0].t() * tVecs[idx0];
        Mat pt3Ds;
        Util::reconstructPt3D(imgPtsPairInlier,
                              MAKE_MAT_PAIR(kMats[idx0], kMats[idx1]),
                              MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]),
                              twTrans,
                              &pt3Ds, nullptr, nullptr);
        Mat oPts[] = {-rMats[idx0].t() * tVecs[idx0], -rMats[idx1].t() * tVecs[idx1]};
        Mat zVecs[] = {rMats[idx0].t() * zStandard, rMats[idx1].t() * zStandard};
        int backwardCountSub = 0;
        for (int j=0; j<totalCountSub; j++) {
            Mat pt3D = pt3Ds.rowRange(j*3, (j+1)*3);
            double cos[2];
            for (int m=0; m<2; m++) {
                Mat pt3DVec = pt3D - oPts[m];
                cos[m] = Mat(zVecs[m].t() * pt3DVec).at<double>(0, 0) / (norm(zVecs[m]) * norm(pt3DVec));
            }
            if (cos[0] < 0 && cos[1] < 0) {
                backwardCountSub++;
            }
        }
        FLOGD("checkTOrder : [%d-%d] : %d / %d = %f", idx0, idx1, backwardCountSub, totalCountSub, backwardCountSub / (double)totalCountSub);
        backwardCount += backwardCountSub;
        totalCount += totalCountSub;
    }
    FLOGD("checkTOrder : total : %d / %d = %f", backwardCount, totalCount, backwardCount / (double)totalCount);
    if (backwardCount / (double)totalCount < 0.75) {
        for (Mat& tVec : tVecs) {
            tVec *= -1;
        }
    }
}

bool SelfCalibration::alignT(const vector<Mat>& tTransList, const vector<Mat>& rMats, const vector<IdxPair>& idxPairs,
                             vector<Mat>& tVecs) {
    vector<IdxTriple> idxTriples;
    vector<tuple<IdxPair, IdxPair, IdxPair>> idxPairsTriples;
    Util::makeTriple(idxPairs, idxTriples, idxPairsTriples);
    set<int> idxSet;
    for (auto &idxTriple : idxTriples) {
        idxSet.insert(get<0>(idxTriple));
        idxSet.insert(get<1>(idxTriple));
        idxSet.insert(get<2>(idxTriple));
    }
    if (idxSet.size() < idxPairs.size()) {
        FLOGE("calibration : alignT failed : cannot cover all idxPairs");
        return false;
    }

    Mat A = Mat::zeros(idxTriples.size() * 3, idxSet.size(), CV_64FC1);
    for (int i=0; i<idxTriples.size(); i++) {
        auto [i0, i1, i2] = idxTriples[i];
        auto &idxPair0 = idxPairs[i0], &idxPair1 = idxPairs[i1], &idxPair2 = idxPairs[i2];
        tTransList[i0].copyTo(A.rowRange(i*3, i*3+3).col(i0));
        if (Util::contains(idxPair1, IDX_PAIR_1(idxPair0))) {
            Mat(tTransList[i1] * (IDX_PAIR_1(idxPair0) == IDX_PAIR_0(idxPair1) ? 1 : -1)).copyTo(A.rowRange(i*3, i*3+3).col(i1));
            Mat(tTransList[i2] * (IDX_PAIR_0(idxPair0) == IDX_PAIR_0(idxPair2) ? -1 : 1)).copyTo(A.rowRange(i*3, i*3+3).col(i2));
        } else {
            Mat(tTransList[i1] * (IDX_PAIR_0(idxPair0) == IDX_PAIR_0(idxPair1) ? -1 : 1)).copyTo(A.rowRange(i*3, i*3+3).col(i1));
            Mat(tTransList[i2] * (IDX_PAIR_1(idxPair0) == IDX_PAIR_0(idxPair2) ? 1 : -1)).copyTo(A.rowRange(i*3, i*3+3).col(i2));
        }
    }
    Mat x = Util::solveZ(A);

    int iBase = 0;
    tVecs.clear();
    for (int i=0; i<rMats.size(); i++) {
        if (i == 0) {
            tVecs.emplace_back(Mat::zeros(3, 1, CV_64FC1));
        } else {
            for (int j=0; j<idxPairs.size(); j++) {
                auto &idxPair = idxPairs[j];
                if (Util::contains(idxPair, iBase) && Util::contains(idxPair, i)) {
                    tVecs.emplace_back(- rMats[i] * tTransList[j] * x.at<double>(j, 0) * (IDX_PAIR_0(idxPair) == iBase ? 1 : -1));
                    break;
                }
            }
        }
    }
    return true;
}

void SelfCalibration::filterByDisparity(const vector<Mat>& kMats, const vector<Mat>& rMats, const vector<Mat>& tVecs,
                                        const vector<KeyPointListPair>& imgPtsPairs,
                                        const vector<IdxPair>& idxPairs,
                                        const Size& imageSize,
                                        vector<vector<int>>& inlierIdxsList) {
    for (int i=0; i<idxPairs.size(); i++) {
        auto& [imgPts0, imgPts1] = imgPtsPairs[i];
        auto [idx0, idx1] = idxPairs[i];
        const vector<int>& inlierIdxs = inlierIdxsList[i];
        KeyPointListPair imgPtsPairInlier;
        Util::getImgPtsInlier(imgPtsPairs[i], inlierIdxsList[i], imgPtsPairInlier);

        vector<Mat> r, p;
        Mat Q;
        vector<double> imgPtsDisparity, imgPtsDisparityErr;
        DisparityUtil::rectify(imgPtsPairInlier,
                               idxPairs[i],
                               MAKE_MAT_PAIR(kMats[idx0], kMats[idx1]),
                               MAKE_MAT_PAIR(rMats[idx0], rMats[idx1]),
                               MAKE_MAT_PAIR(tVecs[idx0], tVecs[idx1]),
                               imageSize,
                               r, p, Q, imgPtsDisparity, imgPtsDisparityErr);

        double imgPtsDisparityAvg = accumulate(imgPtsDisparity.begin(), imgPtsDisparity.end(), 0.) / imgPtsDisparity.size();

        vector<double> imgPtsDisparitySe;
        imgPtsDisparitySe.reserve(imgPtsDisparity.size());
        for (auto d : imgPtsDisparity) {
            imgPtsDisparitySe.push_back(pow(d - imgPtsDisparityAvg, 2));
        }
        double imgPtsDisparitySeAvg = accumulate(imgPtsDisparitySe.begin(), imgPtsDisparitySe.end(), 0.) / imgPtsDisparitySe.size();

        double imgPtsDisparityErrAvg = accumulate(imgPtsDisparityErr.begin(), imgPtsDisparityErr.end(), 0.) / imgPtsDisparityErr.size();

        vector<int> inlierIdxsNew;
        for (int j=0; j<imgPtsDisparitySe.size(); j++) {
            if (imgPtsDisparitySe[j] / imgPtsDisparitySeAvg < 3. && imgPtsDisparityErr[j] / imgPtsDisparityErrAvg < 3.) {
                inlierIdxsNew.push_back(inlierIdxs[j]);
            }
        }
        inlierIdxsList[i] = inlierIdxsNew;
    }
}

void SelfCalibration::filterBySpread(const vector<KeyPointListPair>& imgPtsPairs,
                                     const vector<IdxPair>& idxPairs,
                                     vector<vector<int>>& inlierIdxsList) {
    for (int i=0; i<idxPairs.size(); i++) {
        const KeyPointList& imgPts = KEY_POINT_LIST_PAIR_0(imgPtsPairs[i]);
        vector<int>& inlierIdxs = inlierIdxsList[i];
        int n = MIN(80, inlierIdxs.size());
        if (n == inlierIdxs.size()) {
            continue;
        }

        uniform_int_distribution<unsigned> u(0, inlierIdxs.size()-1);
        default_random_engine e;

        map<long, double> mapDis;
        auto funcKey = [&](int a, int b) {
            return a < b ? a * 100000 + b : b * 100000 + a;
        };
        auto funcDis = [&](const KeyPoint&a, const KeyPoint& b) {
            return sqrt(pow(a.pt.x - b.pt.x, 2) + pow(a.pt.y - b.pt.y, 2));
        };
        auto funcGetDis = [&](int a, int b) {
            long key = funcKey(a, b);
            double dis = 0;
            auto it = mapDis.find(key);
            if (it != mapDis.end()) {
                dis = it->second;
            } else {
                dis = funcDis(imgPts[a], imgPts[b]);
                mapDis.insert(make_pair(key, dis));
            }
            return dis;
        };
        map<int, double> mapInLinerIdxs;
        int minIdx = -1;
        //
        for (int j=0; j<n; j++) {
            mapInLinerIdxs.insert(make_pair(inlierIdxs[j], 0));
        }
        for (auto it0 = mapInLinerIdxs.begin(); it0 != mapInLinerIdxs.end(); it0++) {
            int j0 = it0->first;
            double disTotal = 0;
            for (auto it1 = mapInLinerIdxs.begin(); it1 != mapInLinerIdxs.end(); it1++) {
                int j1 = it1->first;
                if (j0 == j1)
                    continue;
                disTotal += funcGetDis(j0, j1);
            }
            it0->second = disTotal;
            if (minIdx < 0 || disTotal < mapInLinerIdxs.find(minIdx)->second) {
                minIdx = j0;
            }
        }
        //
        int maxIter = (inlierIdxs.size() - n) * 2;
        for (int nIter=0; nIter<maxIter; nIter++) {
            int j0 = -1;
            for (int nIterRandom=0; nIterRandom<100; nIterRandom++) {
                j0 = inlierIdxs[u(e)];
                if (mapInLinerIdxs.find(j0) == mapInLinerIdxs.end()) {
                    break;
                }
            }
            if (j0 < 0) {
                FLOGD("filterBySpread : %d-%d : skip iter %d", IDX_PAIR_0(idxPairs[i]), IDX_PAIR_1(idxPairs[i]), nIter);
                continue;
            }
            double disTotal = 0;
            for (auto it=mapInLinerIdxs.begin(); it!=mapInLinerIdxs.end(); it++) {
                int j1 = it->first;
                if (j1 == minIdx)
                    continue;
                disTotal += funcGetDis(j0, j1);
            }
            if (disTotal > mapInLinerIdxs.find(minIdx)->second) {
                mapInLinerIdxs.erase(minIdx);
                mapInLinerIdxs.insert(make_pair(j0, disTotal));
                int minIdxNew = j0;
                for (auto it = mapInLinerIdxs.begin(); it != mapInLinerIdxs.end(); it++) {
                    int j1 = it->first;
                    if (j0 == j1)
                        continue;
                    auto iterJ1 = mapInLinerIdxs.find(j1);
                    iterJ1->second -= mapDis.find(funcKey(minIdx, j1))->second;
                    iterJ1->second += mapDis.find(funcKey(j0, j1))->second;
                    if (iterJ1->second < mapInLinerIdxs.find(minIdxNew)->second) {
                        minIdxNew = j1;
                    }
                }
                minIdx = minIdxNew;
            }
        }
        inlierIdxs.clear();
        for (auto it = mapInLinerIdxs.begin(); it != mapInLinerIdxs.end(); it++) {
            inlierIdxs.emplace_back(it->first);
        }
    }
}