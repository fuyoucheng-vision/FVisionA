//
// Created by fuyoucheng on 2021/4/12.
//

#ifndef FVISIONA_STEREO_DEFINE_H
#define FVISIONA_STEREO_DEFINE_H

#include <string>
#include <opencv2/opencv.hpp>

//
typedef std::tuple<int, int> IdxPair;
typedef std::tuple<int, int, int> IdxTriple;

#define MAKE_IDX_PAIR(idx0, idx1) make_tuple(idx0, idx1)

#define IDX_PAIR_0(idxPair) std::get<0>(idxPair)
#define IDX_PAIR_1(idxPair) std::get<1>(idxPair)

//
typedef std::vector<cv::KeyPoint> KeyPointList;
typedef std::vector<cv::DMatch> MatchList;

typedef std::tuple<KeyPointList, KeyPointList> KeyPointListPair;

#define MAKE_KEY_POINT_LIST_PAIR(kpList0, kpList1) make_tuple(kpList0, kpList1)

#define KEY_POINT_LIST_PAIR_0(keyPointListPair) std::get<0>(keyPointListPair)
#define KEY_POINT_LIST_PAIR_1(keyPointListPair) std::get<1>(keyPointListPair)

//
typedef std::tuple<cv::Mat, cv::Mat> MatPair;

#define MAKE_MAT_PAIR(mat0, mat1) make_tuple(mat0, mat1)

#define MAT_PAIR_0(matPair) std::get<0>(matPair)
#define MAT_PAIR_1(matPair) std::get<1>(matPair)

//
typedef std::vector<cv::Mat> WordPtList;
typedef std::vector<cv::Vec3b> ColorList;


#endif //FVISIONA_STEREO_DEFINE_H
