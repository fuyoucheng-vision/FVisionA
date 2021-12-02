//
// Created by fuyoucheng on 2021/3/29.
//

#include "DisparityUtil.h"
#include <numeric>
#include "../FLog.h"
#include "Util.h"

using namespace std;
using namespace cv;

void DisparityUtil::calDisparity(const tuple<const StereoImage&, const StereoImage&>& imagePair,
                                 const IdxPair& idxPair,
                                 const KeyPointListPair& imgPtsPair,
                                 const MatPair& kMatPair, const MatPair& rMatPair, const MatPair& tVecPair,
                                 bool isDebugInfo,
                                 tuple<Mat, Mat>& imgRectifyPair, Mat& disparity, Mat& disparityMask,
                                 bool& isVertical, bool& isReverse, tuple<int, int>& disparityRange,
                                 MatPair& rMatPairRectify, MatPair& kMatPairNew, Mat& Q) {

    const reference_wrapper<const StereoImage> images[] = {ref(get<0>(imagePair)), ref(get<1>(imagePair))};
    int idxs[] = {IDX_PAIR_0(idxPair), IDX_PAIR_1(idxPair)};
    Size imageSize = Size(images[0].get().getImage().cols, images[0].get().getImage().rows);

    // stereo rectify
    vector<Mat> r, p;
    vector<double> imgPtsDisparity, imgPtsDisparityErr;
    DisparityUtil::rectify(imgPtsPair, idxPair, kMatPair, rMatPair, tVecPair, imageSize,
                           r, p, Q, imgPtsDisparity, imgPtsDisparityErr);
    isVertical = abs(p[1].at<double>(1, 3)) > abs(p[1].at<double>(0, 3));
    double imgPtsDisparityAvg = accumulate(imgPtsDisparity.begin(), imgPtsDisparity.end(), 0.) / imgPtsDisparity.size();
    isReverse = imgPtsDisparityAvg < 0;
    double imgPtsDisparityErrAvg = accumulate(imgPtsDisparityErr.begin(), imgPtsDisparityErr.end(), 0.) / imgPtsDisparityErr.size();

    Mat mapMats[2][2];
    Mat kMats[] = {MAT_PAIR_0(kMatPair), MAT_PAIR_1(kMatPair)};
    for (int i=0; i<2; i++) {
        cv::initUndistortRectifyMap(kMats[i], Mat(), r[i], p[i], imageSize, CV_32FC1, mapMats[i][0], mapMats[i][1]);
    }

    Mat imgRectify[2];
    for (int i=0; i<2; i++) {
        cv::remap(images[i].get().getImage(), imgRectify[i], mapMats[i][0], mapMats[i][1], INTER_CUBIC);
    }
    get<0>(imgRectifyPair) = imgRectify[0].clone();
    get<1>(imgRectifyPair) = imgRectify[1].clone();
    if (isDebugInfo) {
        for (int i=0; i<2; i++) {
//            stringstream ss;
//            ss<<"rectify_"<<idxs[0]<<"-"<<idxs[1]<<"."<<idxs[i];
//            Util::saveDebugImage(imgRectify[i], ss.str());
            Util::saveDebugImage(imgRectify[i], "rectify_%d-%d.%d", idxs[0], idxs[1], idxs[i]);
        }
    }
    for (auto &img : imgRectify) {
        img = Util::toGray(img);
        if (isVertical) {
            img = img.t();
        }
        if (isReverse) {
            Mat imgFlipped;
            cv::flip(img, imgFlipped, 1);
            img = imgFlipped;
        }
    }
    FLOGD("Disparity : [%d, %d] : %f, %f, %s%s", idxs[0], idxs[1], abs(imgPtsDisparityAvg), imgPtsDisparityErrAvg,
            isVertical ? "vertical" : "horizontal", isReverse ? ", reverse" : "");

    // stereo matching
    int disparitySize = 16 * 10;

    auto stereo = StereoSGBM::create();
    stereo->setBlockSize(11);
    stereo->setP1(8 * pow(stereo->getBlockSize(), 2));
    stereo->setP2(32 * pow(stereo->getBlockSize(), 2) * 6);
    stereo->setDisp12MaxDiff(1);
    stereo->setPreFilterCap(31);
    stereo->setSpeckleRange(32);
    stereo->setSpeckleWindowSize(100);
    stereo->setMinDisparity(int(abs(imgPtsDisparityAvg) - disparitySize / 2.));
    stereo->setNumDisparities(disparitySize);

    Mat disparity16U;
    stereo->compute(imgRectify[0], imgRectify[1], disparity16U);

    if (isReverse) {
        Mat disparity8UFlipped;
        flip(disparity16U, disparity8UFlipped, 1);
        disparity16U = disparity8UFlipped;
    }
    if (isVertical) {
        disparity16U = disparity16U.t();
    }

    Mat imgDisparity;
    cv::normalize(disparity16U, imgDisparity, 0, 255, CV_MINMAX, CV_8U);
    generateDisparityMask(imgDisparity, disparityMask);
    if (isDebugInfo) {
        Util::saveDebugImage(imgDisparity, "disparity_%d-%d", idxs[0], idxs[1]);
        Util::saveDebugImage(disparityMask, "disparity_mask_%d-%d", idxs[0], idxs[1]);
    }

    disparity16U.convertTo(disparity, CV_64F);
    disparity = disparity / 16;

    get<0>(disparityRange) = stereo->getMinDisparity();
    get<1>(disparityRange) = stereo->getMinDisparity() + stereo->getNumDisparities();
    MAT_PAIR_0(rMatPairRectify) = r[0];
    MAT_PAIR_1(rMatPairRectify) = r[1];
    MAT_PAIR_0(kMatPairNew) = p[0].rowRange(0, 3).colRange(0, 3);
    MAT_PAIR_1(kMatPairNew) = p[1].rowRange(0, 3).colRange(0, 3);
}

void DisparityUtil::rectify(const KeyPointListPair& imgPtsPair,
                            const IdxPair& idxPair,
                            const MatPair& kMatPair, const MatPair& rMatPair, const MatPair& tVecPair,
                            const Size& imageSize,
                            vector<Mat>& r, vector<Mat>& p, Mat& Q,
                            vector<double>& imgPtsDisparity, vector<double>& imgPtsDisparityErr) {
    const reference_wrapper<const KeyPointList> imgPts[2] = {ref(KEY_POINT_LIST_PAIR_0(imgPtsPair)), ref(KEY_POINT_LIST_PAIR_1(imgPtsPair))};
    Mat kMats[] = {MAT_PAIR_0(kMatPair), MAT_PAIR_1(kMatPair)};

    Mat rTrans = MAT_PAIR_1(rMatPair) * MAT_PAIR_0(rMatPair).t();
    Mat tTrans = MAT_PAIR_1(tVecPair) - rTrans * MAT_PAIR_0(tVecPair);
    r.clear();
    p.clear();
    for (int i=0; i<2; i++) {
        r.emplace_back(Mat());
        p.emplace_back(Mat());
    }
    cv::stereoRectify(kMats[0], Mat(), kMats[1], Mat(), imageSize, rTrans, tTrans,
                      r[0], r[1], p[0], p[1], Q, 0);

    bool isVertical = abs(p[1].at<double>(1, 3)) > abs(p[1].at<double>(0, 3));

    double cosTZ = Util::cosTZ(rMatPair, tVecPair);
    double angleTZ = Util::degrees(acos(cosTZ));
    FLOGD("rectify : [%d-%d] : cosTZ = %f, angle = %f", IDX_PAIR_0(idxPair), IDX_PAIR_1(idxPair), angleTZ, abs(angleTZ - 90));

    vector<Mat> imgPtsRemapped[2];
    for (int i=0; i<2; i++) {
        Mat _T = p[i].rowRange(0, 3).colRange(0, 3) * r[i] * kMats[i].inv();
        for (auto &imgPt : imgPts[i].get()) {
            imgPtsRemapped[i].emplace_back(remapPt(imgPt, _T));
        }
    }

    imgPtsDisparity.clear();
    imgPtsDisparity.reserve(imgPtsRemapped[0].size());
    imgPtsDisparityErr.clear();
    imgPtsDisparityErr.reserve(imgPtsRemapped[0].size());
    for (int i=0; i<imgPtsRemapped[0].size(); i++) {
        imgPtsDisparity.emplace_back(isVertical
                                     ? imgPtsRemapped[0][i].at<double>(1, 0) - imgPtsRemapped[1][i].at<double>(1, 0)
                                     : imgPtsRemapped[0][i].at<double>(0, 0) - imgPtsRemapped[1][i].at<double>(0, 0));
        imgPtsDisparityErr.emplace_back(isVertical
                                        ? abs(imgPtsRemapped[0][i].at<double>(0, 0) - imgPtsRemapped[1][i].at<double>(0, 0))
                                        : abs(imgPtsRemapped[0][i].at<double>(1, 0) - imgPtsRemapped[1][i].at<double>(1, 0)));
    }
}

Mat DisparityUtil::remapPt(const KeyPoint& kp, const Mat& T) {
    Mat pt = T * (Mat_<double>(3, 1) << kp.pt.x, kp.pt.y, 1);
    return pt / pt.at<double>(2, 0);
}

void DisparityUtil::generateDisparityMask(const Mat& imgDisparity, Mat& disparityMask) {
    Mat imgDisparityThreshold;
    cv::threshold(imgDisparity, imgDisparityThreshold, 20, 255, THRESH_BINARY);

    Mat maskRect = Util::generateMaskRect(Size(imgDisparity.cols, imgDisparity.rows),
                                          Size(imgDisparity.cols / 2, imgDisparity.rows / 2),
                                          Size(imgDisparity.cols * 0.8, imgDisparity.rows * 0.7));
    cv::Mat imgDisparityMaskedRect;
    cv::add(imgDisparityThreshold, Mat::zeros(Size(imgDisparity.cols, imgDisparity.rows), CV_8UC1), imgDisparityMaskedRect, maskRect);

    Mat maskEllipse = Util::generateMaskEllipse(Size(imgDisparity.cols, imgDisparity.rows),
                                                Size(imgDisparity.cols / 2, imgDisparity.rows / 2),
                                                Size(imgDisparity.cols * 0.85, imgDisparity.rows * 0.85));
    cv::Mat imgDisparityMaskedEllipse;
    cv::add(imgDisparityMaskedRect, Mat::zeros(Size(imgDisparity.cols, imgDisparity.rows), CV_8UC1), imgDisparityMaskedEllipse, maskEllipse);

    Mat imgDisparityDilate;
    cv::dilate(imgDisparityMaskedEllipse, imgDisparityDilate, cv::getStructuringElement(MORPH_ELLIPSE, Size(8, 8)));
    cv::erode(imgDisparityDilate, disparityMask, cv::getStructuringElement(MORPH_ELLIPSE, Size(32, 32)));
}

void DisparityUtil::reconstruct(const tuple<Mat, Mat>& imgRectifyPair,
                                const IdxPair& idxPair,
                                const Mat& disparity, const Mat& disparityMask,
                                const bool& isVertical, const bool& isReverse, const tuple<int, int>& disparityRange,
                                const tuple<Mat, Mat>& rRectifyPair,
                                const MatPair& kMatPair, const MatPair& rMatPair, const MatPair& tVecPair,
                                WordPtList& pts, ColorList& colors) {
    const Mat& imgRectify = get<0>(imgRectifyPair);
    double thresholdMin = get<0>(disparityRange) + (get<1>(disparityRange) - get<0>(disparityRange)) * 0.1;
    double thresholdMax = get<1>(disparityRange) - (get<1>(disparityRange) - get<0>(disparityRange)) * 0.1;
    int countTotal = 0, countMasked = 0, countMin = 0, countMax = 0;
    double pixel = 0;

    Mat zStandard = (Mat_<double>(3, 1) << 0,0,1);
    Mat rRectifies[] = {MAT_PAIR_0(rRectifyPair), MAT_PAIR_1(rRectifyPair)};
    Mat kMats[] = {MAT_PAIR_0(kMatPair), MAT_PAIR_1(kMatPair)};
    Mat rMats[] = {MAT_PAIR_0(rMatPair), MAT_PAIR_1(rMatPair)};
    Mat tVecs[] = {MAT_PAIR_0(tVecPair), MAT_PAIR_1(tVecPair)};
    Mat o[2], zVec[2], zVecRemapped[2];
    double zCos[2];
    for (int i=0; i<2; i++) {
        o[i] = - rMats[i].t() * tVecs[i];
        zVec[i] = rMats[i].t() * zStandard;
        zVec[i] /= norm(zVec[i]);
        zVecRemapped[i] = (rRectifies[i] * rMats[i]).t() * zStandard;
        zVecRemapped[i] /= norm(zVecRemapped[i]);
        zCos[i] = Mat(zVec[i].t() * zVecRemapped[i]).at<double>(0, 0);
    }
    bool isBackward = zCos[0] < 0 && zCos[1] < 0;

    Mat kr = Mat(6, 3, CV_64FC1);
    Mat(kMats[0] * rRectifies[0] * rMats[0]).copyTo(kr.rowRange(0, 3));
    Mat(kMats[1] * rRectifies[1] * rMats[1]).copyTo(kr.rowRange(3, 6));
    Mat kt = Mat(6, 1, CV_64FC1);
    Mat(kMats[0] * rRectifies[0] * tVecs[0]).copyTo(kt.rowRange(0, 3));
    Mat(kMats[1] * rRectifies[1] * tVecs[1]).copyTo(kt.rowRange(3, 6));

    Mat h = Mat::zeros(4, 6, CV_64FC1);
    h.at<double>(0, 0) = h.at<double>(1, 1) = h.at<double>(2, 3) = h.at<double>(3, 4) = 1;

    Mat krh = Mat::zeros(6, 5, CV_64FC1);
    kr.copyTo(krh.colRange(0, 3));
    krh.at<double>(2, 3) = -1;
    krh.at<double>(5, 4) = -1;

    int intervalX = 7, intervalY = 7;
    if (disparity.cols > disparity.rows) {
        intervalY = int(intervalX * disparity.rows / (float)disparity.cols) + 1;
    } else {
        intervalX = int(intervalY * disparity.cols / (float)disparity.rows) + 1;
    }
    int rangeX[] = {0, disparity.cols / intervalX * intervalX};
    int rangeY[] = {0, disparity.rows / intervalY * intervalY};

    WordPtList ptsFull;
    ColorList colorsFull;
    vector<tuple<int, int, double>> imgPts;
    imgPts.reserve(intervalX * intervalY);
    auto comparator = [](const tuple<int, int, double>& p1, const tuple<int, int, double>& p2) {
        return get<2>(p1) < get<2>(p2);
    };

    for (int i = rangeX[0]; i < rangeX[1]; i += intervalX) {
        for (int j = rangeY[0]; j < rangeY[1]; j += intervalY) {
            imgPts.clear();
            for (int _i = i; _i < i+intervalX; _i++) {
                for (int _j = j; _j < j+intervalY; _j++) {
                    countTotal++;
                    auto& mask = disparityMask.at<unsigned char>(_j, _i);
                    if (mask < 127) {
                        countMasked++;
                        continue;
                    }
                    pixel = disparity.at<double>(_j, _i);
                    if (pixel <= thresholdMin) {
                        countMin++;
                    } else if (pixel >= thresholdMax) {
                        countMax++;
                    } else {
                        imgPts.emplace_back(make_tuple(_i, _j, pixel));
                    }
                }
            }
            if (!imgPts.empty()) {
                sort(imgPts.begin(), imgPts.end(), comparator);
                auto &imgPt = imgPts[imgPts.size() / 2];
                Mat pt = DisparityUtil::reconstruct(get<0>(imgPt), get<01>(imgPt), get<2>(imgPt),
                                                    isVertical, isReverse, h, kr, kt);
//                Mat pt = DisparityUtil::reconstruct(get<0>(imgPt), get<1>(imgPt), get<2>(imgPt),
//                                                    isVertical, isReverse, krh, kt);
                double zPtCos[2];
                for (int k=0; k<2; k++) {
                    Mat ptVec = pt - o[k];
                    zPtCos[k] = Mat(zVecRemapped[k].t() * ptVec).at<double>(0, 0) / norm(ptVec);
                }
                bool isOpposite = zPtCos[0] < 0 && zPtCos[1] < 0;
                if ((isOpposite && !isBackward) || (!isOpposite && isBackward)) {
                    pt *= -1;
                }
                ptsFull.emplace_back(pt);
                colorsFull.emplace_back(imgRectify.at<Vec3b>(get<1>(imgPt), get<0>(imgPt)));
            }
        }
    }

    Mat ptCenter = accumulate(ptsFull.begin(), ptsFull.end(), Mat::zeros(3, 1, CV_64FC1));
    ptCenter /= ptsFull.size();
    vector<double> errNorm;
    errNorm.reserve(ptsFull.size());
    for (auto& pt : ptsFull) {
        errNorm.push_back(norm(pt - ptCenter));
    }
    double errNormAvg = accumulate(errNorm.begin(), errNorm.end(), 0.0);
    errNormAvg /= errNorm.size();
    vector<double> errNormSe;
    errNormSe.reserve(errNorm.size());
    for (auto& e : errNorm) {
        errNormSe.push_back(pow(e - errNormAvg, 2));
    }
    double errNormSeAvg = accumulate(errNormSe.begin(), errNormSe.end(), 0.0);
    errNormSeAvg /= errNormSe.size();
    for (int i=0; i<ptsFull.size(); i++) {
        if (errNormSe[i] / errNormSeAvg < 3.0) {
            pts.emplace_back(ptsFull[i]);
            colors.emplace_back(colorsFull[i]);
        }
    }

    FLOGD("Reconstruct : [%d-%d] : threshold = (%.2f, %.2f), total = %d, masked = %d, min = %d, max = %d, ptsFull = %d, pts = %d",
          IDX_PAIR_0(idxPair), IDX_PAIR_1(idxPair), thresholdMin, thresholdMax, countTotal, countMasked, countMin, countMax, ptsFull.size(), pts.size());
}

Mat DisparityUtil::reconstruct(double x, double y, double disparity, bool isVertical, bool isReverse,
                               Mat& h, const Mat& kr, const Mat& kt) {
    h.at<double>(0, 2) = -x;
    h.at<double>(1, 2) = -y;
    h.at<double>(2, 5) = -(x - (isVertical ? 0 : disparity) * (isReverse ? -1 : 1));
    h.at<double>(3, 5) = -(y - (isVertical ? disparity : 0) * (isReverse ? -1 : 1));

    Mat pt;
    if (!Util::solve(h * kr, - h * kt, pt)) {
        FLOGE("failed to solve");
        return Mat();
    }
    return pt;
}

Mat DisparityUtil::reconstruct(double x, double y, double disparity, bool isVertical, bool isReverse,
                               Mat& krh, const Mat& kt) {
    krh.at<double>(0, 3) = -x;
    krh.at<double>(1, 3) = -y;
    krh.at<double>(3, 4) = -(x - (isVertical ? 0 : disparity) * (isReverse ? -1 : 1));
    krh.at<double>(4, 4) = -(y - (isVertical ? disparity : 0) * (isReverse ? -1 : 1));

    Mat pt;
    if (!Util::solve(krh, - kt, pt)) {
        FLOGE("failed to solve");
        return Mat();
    }
    pt = pt.rowRange(0, 3);
    return pt;
}
