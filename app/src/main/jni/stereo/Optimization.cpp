//
// Created by fuyoucheng on 2021/3/26.
//

#include "Optimization.h"
#include <thread>
#include "../FLog.h"
#include "Util.h"

using namespace std;
using namespace cv;

Optimization::Optimization(bool _isParallel) :
        isParallel(_isParallel), paramsCount(9),
        maxIter(30), epsilon1(0.01), epsilon2(0.000000000000000000000000001), muScale(0.01), nuInit(2), nuScale(2), theta(0.01),
        idxPairsPtr(nullptr), imgPtsPairsPtr(nullptr), cameraCount(0) {

}

Optimization::~Optimization() {

}

void Optimization::optimize(vector<Mat>& kMats, vector<Mat>& rMats, vector<Mat>& tVecs,
                            const vector<IdxPair>& idxPairs,
                            const vector<KeyPointListPair>& imgPtsPairs,
                            const vector<vector<int>>& inlierIdxsList) {
    cameraCount = kMats.size();
    Mat params = onStart(kMats, rMats, tVecs, idxPairs, imgPtsPairs, inlierIdxsList);
    optimize(params);
    onFinish(params, kMats, rMats, tVecs);
}

Mat Optimization::onStart(const vector<Mat>& kMats, const vector<Mat>& rMats, const vector<Mat>& tVecs,
                          const vector<IdxPair>& idxPairs,
                          const vector<KeyPointListPair>& imgPtsPairs,
                          const vector<vector<int>>& inlierIdxsList) {
    idxPairsPtr = &idxPairs;
    imgPtsPairsPtr = &imgPtsPairs;
    Mat params = Mat::zeros(cameraCount * paramsCount, 1, CV_64FC1);
    for (int i=0; i<cameraCount; i++) {
        int iBase = i * paramsCount;
        params.at<double>(iBase, 0) = (kMats[i].at<double>(0, 0) + kMats[i].at<double>(1, 1)) / 2;
        params.at<double>(iBase+1, 0) = kMats[i].at<double>(0, 2) ;
        params.at<double>(iBase+2, 0) = kMats[i].at<double>(1, 2) ;
        Rodrigues(rMats[i], params.rowRange(iBase+3, iBase+6));
        tVecs[i].copyTo(params.rowRange(iBase+6, iBase+9));
    }
    calHLists(imgPtsPairs);
    return params;
}

void Optimization::onFinish(const Mat& params, vector<Mat>& kMats, vector<Mat>& rMats, vector<Mat>& tVecs) {
    kMats.clear();
    rMats.clear();
    tVecs.clear();
    for (int i=0; i<cameraCount; i++) {
        int iBase = i * paramsCount;
        kMats.emplace_back((Mat_<double>(3, 3) <<
                params.at<double>(iBase, 0), 0, params.at<double>(iBase + 1, 0),
                0, params.at<double>(iBase, 0), params.at<double>(iBase + 2, 0),
                0, 0, 1));
        Mat rMat = Mat::zeros(3, 3, CV_64FC1);
        Rodrigues(params.rowRange(iBase + 3, iBase + 6), rMat);
        rMats.emplace_back(rMat);
        tVecs.emplace_back(params.rowRange(iBase + 6, iBase + 9).clone());
    }
    for (int i=0; i<cameraCount; i++) {
        Util::printMat(kMats[i], "Optimization : kMat[%d]\n", i);
        Util::printMat(rMats[i], "Optimization : rMat[%d]\n", i);
        Util::printMat(tVecs[i], "Optimization : tVec[%d]\n", i);
    }
}

void Optimization::optimize(Mat& x) {
    Mat f, jacobian;
    calculate(x, f, jacobian);
    FLOGD("Optimization : x (%d, %d)", x.rows, x.cols);
//    Util::printMat("", x);
    FLOGD("Optimization : f (%d, %d)", f.rows, f.cols);
//    Util::printMat("", f);
    FLOGD("Optimization : jacobian (%d, %d)", jacobian.rows, jacobian.cols);
//    Util::printMat("", jacobian);

    Mat A = jacobian.t() * jacobian;
    Mat g = jacobian.t() * f;
    FLOGD("Optimization : A (%d, %d)", A.rows, A.cols);
//    Util::printMat("", A);
    FLOGD("Optimization : g (%d, %d)", g.rows, g.cols);
//    Util::printMat("", g);

    int iter = 0;
    double _maxADiag;
    minMaxLoc(A.diag(), nullptr, &_maxADiag);
    double mu = _maxADiag * muScale;
    double nu = nuInit;
    while (iter < maxIter) {
        iter++;
        FLOGD("Optimization : ========== iter = %d, epsilon1 = %f, mu = %f, nu = %f", iter, epsilon1, mu, nu);
        Mat Ap = A + mu * Mat::eye(A.rows, A.cols, CV_64FC1);
        Mat dx;
        if (!Util::solve(Ap, -g, dx)) {
            FLOGE("Optimization : solve dx failed!");
            break;
        }
        double normDx = norm(dx);
        double normX = norm(x);
        FLOGD("Optimization : norm(dx) = %f, norm(x) = %f", normDx, normX);
        if (normDx <= epsilon2 * (normX + epsilon2)) {
            FLOGD("Optimization : found by epsilon2 : %d", iter);
            break;
        }
        Mat xNew = x + dx, fNew, jacobianNew;
        calculate(xNew, fNew, jacobianNew);
        double F = 0.5 * norm(f, NORM_L2SQR);
        double FNew = 0.5 * norm(fNew, NORM_L2SQR);
        Mat L = 0.5 * dx.t() * (mu * dx - g);
        double rho = (F - FNew) / L.at<double>(0, 0);
        FLOGD("Optimization : F = %f, FNew = %f, L = %f, rho = %f", F, FNew, L.at<double>(0, 0), rho);
        if (rho > 0) {
            x = xNew;
            f = fNew;
            jacobian = jacobianNew;
            A = jacobian.t() * jacobian;
            g = jacobian.t() * f;
            double normG = norm(g, NORM_INF);
            FLOGD("Optimization : norm(g) = %f", normG);
            if (normG <= epsilon1) {
                FLOGD("Optimization : found by epsilon1 : %d", iter);
                break;
            } else {
                FLOGD("Optimization : accept : %d", iter);
                mu = mu * max(1./3., 1. - pow(2.*rho-1., 3));
                nu = nuInit;
            }
        } else {
            FLOGD("Optimization : not accept : %d", iter);
            mu *= nu;
            nu *= nuScale;
        }
    }
    FLOGD("Optimization : finish : %d", iter);
}

void Optimization::calculate(const Mat& params, Mat& f, Mat& jacobian) {
    vector<Mat> KRs, KTs, dKRs, dKTs;
    calKRKTdKRdKT(params, KRs, KTs, dKRs, dKTs);

    int ptCountAll = 0;
    for (auto &hList : hLists) {
        ptCountAll += hList.size();
    }
//    FLOGD("Optimization : calculate : ptCountAll = %d", ptCountAll);
    f = Mat::zeros(ptCountAll * 4, 1, CV_64FC1);
    jacobian = Mat::zeros(ptCountAll * 4, paramsCount * cameraCount, CV_64FC1);

    auto func = [&](const int hListBeg, const int hListEnd) {
        int ptCountCur = 0;
        for (int i = 0; i < hListBeg; i++) {
            ptCountCur += hLists[i].size();
        }
        for (int iH = hListBeg; iH < hListEnd; iH++) {
            auto &idxPair = (*idxPairsPtr)[iH];
            auto &hList = hLists[iH];

            Mat KRFull, KTFull;
            vector<Mat> dKRFulls, dKTFulls;
            calKRKTdKRdKTFull(KRs, KTs, dKRs, dKTs, idxPair, KRFull, KTFull, dKRFulls, dKTFulls);

            Mat fSub = f.rowRange(ptCountCur * 4, (ptCountCur + hList.size()) * 4);
            Mat jacobianSub = jacobian.rowRange(ptCountCur * 4, (ptCountCur + hList.size()) * 4);
            ptCountCur += hList.size();

            for (size_t i=0; i<hList.size(); i++) {
                int iBase = i * 4;
                auto &h = hList[i];
                Mat A = h * KRFull;
                Mat b = - h * KTFull;
                Mat ATA = A.t() * A;
                Mat ATAInv = ATA.inv();
                Mat x = ATAInv * A.t() * b;

                Mat(A * x - b).copyTo(fSub.rowRange(iBase, iBase + 4));

                int idxs[] = {IDX_PAIR_0(idxPair), IDX_PAIR_1(idxPair)};
                for (int iIdx=0; iIdx<2; iIdx++) {
                    int idx = idxs[iIdx];
                    for (int j=0; j<paramsCount; j++) {
                        Mat dA = h * dKRFulls[iIdx * paramsCount + j];
                        Mat db = - h * dKTFulls[iIdx * paramsCount + j];
                        Mat dATAInv = - ATAInv * (dA.t() * A + A.t() * dA) * ATAInv;
                        Mat dx = dATAInv * A.t() * b + ATAInv * dA.t() * b + ATAInv * A.t() * db;
                        Mat(dA * x + A * dx - db).copyTo(
                                jacobianSub.rowRange(iBase, iBase + 4)
                                               .colRange(idx * paramsCount + j, idx * paramsCount + j + 1));
                    }
                }
            }
        }
    };
    if (isParallel) {
        int parallelCount = 3;
        vector<thread*> ths;
        for (int i=0; i<parallelCount; i++) {
            int eachCount = hLists.size() / parallelCount;
            int hListBeg = eachCount * i;
            int hListEnd = i < (parallelCount-1) ? eachCount * (i+1) : hLists.size();
            ths.emplace_back(new thread(func, hListBeg, hListEnd));
        }
        for (auto th : ths) {
            th->join();
        }
    } else {
        auto hListRange = make_tuple(0, hLists.size());
        func(0, hLists.size());
    }
}

void Optimization::calHLists(const vector<KeyPointListPair>& imgPtsPairs) {
    hLists.clear();
    hLists.reserve(imgPtsPairs.size());
    for (auto &imgPtsPair : imgPtsPairs) {
        auto &[imgPts0, imgPts1] = imgPtsPair;
        vector<Mat> hList;
        hList.reserve(imgPts0.size());
        for (int i=0; i<imgPts0.size(); i++) {
            hList.emplace_back((Mat_<double>(4, 6) <<
                    1, 0, -imgPts0[i].pt.x, 0, 0, 0,
                    0, 1, -imgPts0[i].pt.y, 0, 0, 0,
                    0, 0, 0, 1, 0, -imgPts1[i].pt.x,
                    0, 0, 0, 0, 1, -imgPts1[i].pt.y));
        }
        hLists.emplace_back(hList);
    }
}

void Optimization::calKRKTdKRdKT(const Mat& params,
                                 vector<Mat>& KRs, vector<Mat>& KTs,
                                 vector<Mat>& dKRs, vector<Mat>& dKTs) {
    for (int i=0; i<cameraCount; i++) {
        int iBase = i * paramsCount;
        Mat kMat = (Mat_<double>(3, 3) <<
                params.at<double>(iBase, 0), 0, params.at<double>(iBase + 1, 0),
                0, params.at<double>(iBase, 0), params.at<double>(iBase + 2, 0),
                0, 0, 1);
        Mat rMat = Mat::zeros(3, 9, CV_64FC1);
        Mat rMatJacobian = Mat::zeros(3, 3, CV_64FC1);
        Rodrigues(params.rowRange(iBase + 3, iBase + 6), rMat, rMatJacobian);
        Mat tVec = params.rowRange(iBase + 6, iBase + 9);

        KRs.emplace_back(kMat * rMat);
        KTs.emplace_back(kMat * tVec);

        Mat dK[] = {
                (Mat_<double>(3, 3) << 1,0,0, 0,1,0, 0,0,0),
                (Mat_<double>(3, 3) << 0,0,1, 0,0,0, 0,0,0),
                (Mat_<double>(3, 3) << 0,0,0, 0,0,1, 0,0,0),
        };
        Mat dT[] = {
                (Mat_<double>(3, 1) << 1,0,0),
                (Mat_<double>(3, 1) << 0,1,0),
                (Mat_<double>(3, 1) << 0,0,1),
        };

        for (auto &_dK : dK) {
            dKRs.emplace_back(_dK * rMat);
        }
        for (int j=0; j<3; j++) {
            dKRs.emplace_back(kMat * rMatJacobian.row(j).reshape(1, 3));
        }
        for (int j=0; j<3; j++) {
            dKRs.emplace_back(Mat::zeros(3, 3, CV_64FC1));
        }

        for (auto &_dK : dK) {
            dKTs.emplace_back(_dK * tVec);
        }
        for (int j=0; j<3; j++) {
            dKTs.emplace_back(Mat::zeros(3, 1, CV_64FC1));
        }
        for (auto &_dT : dT) {
            dKTs.emplace_back(kMat * _dT);
        }
    }
}

void Optimization::calKRKTdKRdKTFull(const vector<Mat>& KRs, const vector<Mat>& KTs,
                                     const vector<Mat>& dKRs, const vector<Mat>& dKTs,
                                     const IdxPair& idxPair,
                                     Mat& KRFull, Mat& KTFull,
                                     vector<Mat>& dKRFulls, vector<Mat>& dKTFulls) {
    auto [idx0, idx1] = idxPair;
    KRFull = Mat::zeros(6, 3, CV_64FC1);
    KRs[idx0].copyTo(KRFull.rowRange(0, 3));
    KRs[idx1].copyTo(KRFull.rowRange(3, 6));
    KTFull = Mat::zeros(6, 1, CV_64FC1);
    KTs[idx0].copyTo(KTFull.rowRange(0, 3));
    KTs[idx1].copyTo(KTFull.rowRange(3, 6));

    for (int i=0; i<paramsCount; i++) {
        Mat dKRFull = Mat::zeros(6, 3, CV_64FC1);
        dKRs[idx0 * paramsCount + i].copyTo(dKRFull.rowRange(0, 3));
        dKRFulls.emplace_back(dKRFull);

        Mat dKTFull = Mat::zeros(6, 1, CV_64FC1);
        dKTs[idx0 * paramsCount + i].copyTo(dKTFull.rowRange(0, 3));
        dKTFulls.emplace_back(dKTFull);
    }
    for (int i=0; i<paramsCount; i++) {
        Mat dKRFull = Mat::zeros(6, 3, CV_64FC1);
        dKRs[idx1 * paramsCount + i].copyTo(dKRFull.rowRange(3, 6));
        dKRFulls.emplace_back(dKRFull);

        Mat dKTFull = Mat::zeros(6, 1, CV_64FC1);
        dKTs[idx1 * paramsCount + i].copyTo(dKTFull.rowRange(3, 6));
        dKTFulls.emplace_back(dKTFull);
    }
}

