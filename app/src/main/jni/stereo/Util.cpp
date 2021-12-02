//
// Created by fuyoucheng on 2021/3/22.
//

#include "Util.h"
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include "../FLog.h"

using namespace std;
using namespace cv;

string Util::debugDir;

void Util::setupDebugDir(const std::string& dir) {
    debugDir = dir + "/debug/";
    fstream fs;
    fs.open(debugDir, ios::in);
    if (fs) {
        FLOGD("setupDebugDir : %s : exist", debugDir.c_str());
    } else {
        int ret = mkdir(debugDir.c_str(), S_IRWXU);
        FLOGD("setupDebugDir : %s : mkdir %d", debugDir.c_str(), ret);
    }
}

vector<string> Util::getImageFiles(const string& path) {
    vector<string> images;
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr) {
        struct dirent *file;
        while ((file = readdir(dir)) != nullptr) {
            if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) {
                continue;
            }
            string fileStr(file->d_name);
            if (fileStr.rfind("jpg") != string::npos
                    || fileStr.rfind("jpeg") != string::npos
                    || fileStr.rfind("png") != string::npos) {
                images.push_back(string(path) + "/" + fileStr);
            }
        }
    }
    sort(images.begin(), images.end());
    return images;
}

int64_t Util::getTimestamp() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

Mat Util::loadMat(const Json::Value& node, int rows, int cols) {
    Mat mat(rows, cols, CV_64FC1);
    for (int i=0; i<rows; i++) {
        for (int j=0; j<cols; j++) {
            mat.at<double>(i, j) = atof(node[i*cols+j].asString().c_str());
        }
    }
    return mat;
}

void Util::printMat(const cv::Mat& mat, const char *titleFormat, ...) {
    char title[1024] = {0};
    va_list argList;
    va_start(argList, titleFormat);
    vsnprintf(title, 1024, titleFormat, argList);
    va_end(argList);

    FLOGD("%s", title);
    stringstream ss;
    for (int i=0; i<mat.rows; i++) {
        ss.str("");
        for (int j=0; j<mat.cols; j++) {
            if (mat.type() == CV_64F) {
                ss << mat.at<double>(i, j) << ", ";
            } else if (mat.type() == CV_32F) {
                ss<<mat.at<float>(i, j)<<", ";
            } else if (mat.type() == CV_8U) {
                ss<<(int)mat.at<unsigned char>(i, j)<<", ";
            }
        }
        FLOGD("%s", ss.str().c_str());
    }
}

void Util::makePairs(int range, vector<IdxPair>& pairs) {
    for (int i=0; i<range-1; i++) {
        for (int j=i+1; j<range; j++) {
            pairs.emplace_back(MAKE_IDX_PAIR(i, j));
        }
    }
    //
    stringstream ss;
    for (const auto &pair : pairs) {
        ss<<"("<<IDX_PAIR_0(pair)<<", "<<IDX_PAIR_1(pair)<<"), ";
    }
    FLOGD("makePairs : %s", ss.str().c_str());
}

void Util::makeTriple(const vector<IdxPair>& idxPairs,
                      vector<IdxTriple>& idxTriples,
                      vector<tuple<IdxPair, IdxPair, IdxPair>>& idxPairsTriples) {
    for (int i=0; i<idxPairs.size(); i++) {
        auto idxTriple = make_tuple(i, -1, -1);
        auto idxPairsTriple = make_tuple(idxPairs[i], MAKE_IDX_PAIR(-1, -1), MAKE_IDX_PAIR(-1, -1));
        for (int j=i+1; j<idxPairs.size(); j++) {
            if (Util::contains(idxPairs[i], IDX_PAIR_0(idxPairs[j]))
                    || Util::contains(idxPairs[i], IDX_PAIR_1(idxPairs[j]))) {
                get<1>(idxTriple) = j;
                get<1>(idxPairsTriple) = idxPairs[j];
                for (int k=j+1; k<idxPairs.size(); k++) {
                    if ((Util::contains(idxPairs[i], IDX_PAIR_0(idxPairs[k])) && Util::contains(idxPairs[j], IDX_PAIR_1(idxPairs[k])))
                            || (Util::contains(idxPairs[j], IDX_PAIR_0(idxPairs[k])) && Util::contains(idxPairs[i], IDX_PAIR_1(idxPairs[k])))) {
                        get<2>(idxTriple) = k;
                        get<2>(idxPairsTriple) = idxPairs[k];
                        break;
                    }
                }
                break;
            }
        }
        if (!Util::contains(idxTriple, -1)) {
            if (Util::contains(get<2>(idxPairsTriple), IDX_PAIR_1(get<0>(idxPairsTriple)))) {
                idxTriple = make_tuple(get<0>(idxTriple), get<2>(idxTriple), get<1>(idxTriple));
                idxPairsTriple = make_tuple(get<0>(idxPairsTriple), get<2>(idxPairsTriple), get<1>(idxPairsTriple));
            }
            idxTriples.push_back(idxTriple);
            idxPairsTriples.push_back(idxPairsTriple);
        }
    }
}

void Util::saveDebugImage(const Mat& image, const char *fileNameFormat, ...) {
    char fileName[1024] = {0};
    va_list argList;
    va_start(argList, fileNameFormat);
    vsnprintf(fileName, 1024, fileNameFormat, argList);
    va_end(argList);
    imwrite((debugDir + fileName + ".jpg").c_str(), image);
}

void Util::saveDebugMat(const cv::Mat& mat, const char *fileNameFormat, ...) {
    char fileName[1024] = {0};
    va_list argList;
    va_start(argList, fileNameFormat);
    vsnprintf(fileName, 1024, fileNameFormat, argList);
    va_end(argList);

    string path = debugDir + fileName + ".dat";
    ofstream ofs;
    ofs.open(path.c_str(), ios::binary);
    if (!ofs.is_open()) {
        FLOGE("failed to open : %s", path.c_str());
        return;
    }
    size_t matLength = mat.rows * mat.cols;
    if (mat.type() == CV_64F) {
        matLength *= sizeof(double);
    } else if (mat.type() == CV_32F) {
        matLength *= sizeof(float);
    } else if (mat.type() == CV_16U || mat.type() == CV_16S) {
        matLength *= sizeof(short);
    }
    ofs.write((char*)mat.ptr(), matLength);
    ofs.flush();
    ofs.close();
}

void Util::saveDebugPly(const vector<Mat>& pts, const vector<Vec3b>& colors, const char *fileNameFormat, ...) {
    char fileName[1024] = {0};
    va_list argList;
    va_start(argList, fileNameFormat);
    vsnprintf(fileName, 1024, fileNameFormat, argList);
    va_end(argList);

    double *ptsData = new double[pts.size() * 3], *pd = ptsData;
    unsigned char *ptsColor = new unsigned char[colors.size() * 3], *pc = ptsColor;
    for (int j=0; j<pts.size(); j++) {
        for (int k=0; k<3; k++) {
            *(pd++) = pts[j].at<double>(k, 0);
            *(pc++) = colors[j][k];
        }
    }

    string path = debugDir + fileName + ".ply";
    ofstream ofs;
    ofs.open(path.c_str());
    if (!ofs.is_open()) {
        FLOGE("failed to open : %s", path.c_str());
        return;
    }
    ofs<<"ply\n"
       <<"format ascii 1.0\n"
       <<"element vertex "<<pts.size()<<"\n"
       <<"property float x\n"
       <<"property float y\n"
       <<"property float z\n"
       <<"property uchar blue\n"
       <<"property uchar green\n"
       <<"property uchar red\n"
       <<"end_header\n";
    pd = ptsData;
    pc = ptsColor;
    for (int i=0; i<pts.size(); i++) {
        for (int j=0; j<3; j++)
            ofs<<*pd++<<" ";
        for (int j=0; j<3; j++)
            ofs<<(int)(*pc++)<<" ";
        ofs<<std::endl;
    }
    ofs.flush();
    ofs.close();

    delete[] ptsData;
    delete[] ptsColor;
}

vector<double> Util::rotationMatrixToEuler(const Mat& rMat) {
    vector<double> euler;
    euler.push_back(atan2(rMat.at<double>(2, 1), rMat.at<double>(2, 2)));
    euler.push_back(atan2(- rMat.at<double>(2, 0), sqrt(pow(rMat.at<double>(2, 1), 2) + pow(rMat.at<double>(2, 2), 2))));
    euler.push_back(atan2(rMat.at<double>(1, 0), rMat.at<double>(0, 0)));
    for (double &e : euler) {
        e = degrees(e);
    }
    return euler;
}

Mat Util::eulerToRotationMatrix(const vector<double>& eular) {
    vector<double> _euler;
    for (double e : eular) {
        _euler.emplace_back(radians(e));
    }
    double sin0 = sin(_euler[0]);
    double cos0 = cos(_euler[0]);
    double sin1 = sin(_euler[1]);
    double cos1 = cos(_euler[1]);
    double sin2 = sin(_euler[2]);
    double cos2 = cos(_euler[2]);
    Mat r0 = (Mat_<double>(3, 3) << 1, 0, 0, 0, cos0, -sin0, 0, sin0, cos0);
    Mat r1 = (Mat_<double>(3, 3) << cos1, 0, sin1, 0, 1, 0, -sin1, 0, cos1);
    Mat r2 = (Mat_<double>(3, 3) << cos2, -sin2, 0, sin2, cos2, 0, 0, 0, 1);
    return r2 * r1 * r0;
}

bool Util::solve(const Mat& A, const Mat& b, Mat& x) {
    return cv::solve(A, b, x, DECOMP_SVD);
}

Mat Util::solveZ(const Mat& A) {
    Mat w, u, vt;
    SVD::compute(A, w, u, vt, SVD::FULL_UV);
    for (int i=w.rows-1; i>=0; i--) {
        double value = 0;
        if (w.type() == CV_64F) {
            value = w.at<double>(i, 0);
        } else if (w.type() == CV_32F) {
            value = w.at<float>(i, 0);
        }
        if (value > 1e-8) {
            return vt.row(i).t();
        }
    }
    return vt.row(w.rows-1).t();
//    double minValue;
//    Point minIdx;
//    minMaxLoc(w, &minValue, nullptr, &minIdx, nullptr);
//    return vt.row(minIdx.y).t();
}

Mat Util::toGray(const Mat& image) {
    if (image.channels() == 3) {
        Mat imageGray;
        cvtColor(image, imageGray, COLOR_BGR2GRAY);
        return imageGray;
    } else if (image.channels() == 4) {
        Mat imageGray;
        cvtColor(image, imageGray, COLOR_BGRA2GRAY);
        return imageGray;
    } else {
        return image;
    }
}

Mat Util::makeTx(double t0, double t1, double t2) {
    return (Mat_<double>(3, 3) <<
            0, -t2, t1,
            t2, 0, -t0,
            -t1, t0, 0);
}

double Util::checkErrEMat(const KeyPointListPair& imgPtsPair,
                          const MatPair& kMatPair,
                          const MatPair& rMatPair,
                          const Mat& twTrans) {
    const auto& [imgPts0, imgPts1] = imgPtsPair;
    Mat rTrans = MAT_PAIR_1(rMatPair) * MAT_PAIR_0(rMatPair).t();
    Mat tTrans = MAT_PAIR_0(rMatPair) * twTrans;
    Mat tTransX = makeTx(tTrans.at<double>(0, 0), tTrans.at<double>(1, 0), tTrans.at<double>(2, 0));
    Mat eMat = rTrans * tTransX;
    Mat fMat = MAT_PAIR_1(kMatPair).inv().t() * eMat * MAT_PAIR_0(kMatPair).inv();
    fMat /= norm(fMat);

    Mat err = Mat::zeros(imgPts0.size(), 1, CV_64FC1);
    for (int i=0; i<imgPts0.size(); i++) {
        Mat p0 = (Mat_<double>(3, 1) << imgPts0[i].pt.x, imgPts0[i].pt.y, 1);
        Mat p1 = (Mat_<double>(3, 1) << imgPts1[i].pt.x, imgPts1[i].pt.y, 1);
        err.at<double>(i, 0) = Mat(p1.t() * fMat * p0).at<double>(0, 0);
    }

    return sqrt(Mat(err.t() * err).at<double>(0, 0)) / imgPts0.size();
}

tuple<double, double> Util::checkErrRecon(const KeyPointListPair& imgPtsPair,
                                          const MatPair& kMatPair,
                                          const MatPair& rMatPair,
                                          const Mat& twTrans) {
    Mat pt3Ds, errRecon, errImgPt;
    reconstructPt3D(imgPtsPair, kMatPair, rMatPair, twTrans, &pt3Ds, &errRecon, &errImgPt);
    return make_tuple(sqrt(Mat(errRecon.t() * errRecon).at<double>(0, 0)) / (errRecon.rows / 4), sum(errImgPt)(0) / errImgPt.rows);
}

void Util::reconstructPt3D(const KeyPointListPair& imgPtsPair,
                           const MatPair& kMatPair, const MatPair& rMatPair, const Mat& twTrans,
                           Mat* pt3D, Mat* errRecon, Mat* errImgPt) {
    const reference_wrapper<const KeyPointList> imgPts[] = {ref(KEY_POINT_LIST_PAIR_0(imgPtsPair)), ref(KEY_POINT_LIST_PAIR_1(imgPtsPair))};
    const Mat kMats[] = {MAT_PAIR_0(kMatPair), MAT_PAIR_1(kMatPair)};
    const Mat rMats[] = {MAT_PAIR_0(rMatPair), MAT_PAIR_1(rMatPair)};
    const Mat tVecs[] = {Mat::zeros(3, 1, CV_64FC1), - MAT_PAIR_1(rMatPair) * twTrans};
    int n = imgPts[0].get().size();

    Mat kr = Mat::zeros(6, 3, CV_64FC1);
    Mat(kMats[0] * rMats[0]).copyTo(kr.rowRange(0, 3));
    Mat(kMats[1] * rMats[1]).copyTo(kr.rowRange(3, 6));
    Mat kt = Mat::zeros(6, 1, CV_64FC1);
    Mat(kMats[0] * tVecs[0]).copyTo(kt.rowRange(0, 3));
    Mat(kMats[1] * tVecs[1]).copyTo(kt.rowRange(3, 6));

    if (pt3D != nullptr) {
        pt3D->create(n * 3, 1, CV_64FC1);
    }
    if (errRecon != nullptr) {
        errRecon->create(n * 4, 1, CV_64FC1);
    }
    if (errImgPt != nullptr) {
        errImgPt->create(n * 2, 1, CV_64FC1);
    }
    for (int i=0; i<n; i++) {
        Mat h = (Mat_<double>(4, 6) <<
                1, 0, -imgPts[0].get()[i].pt.x, 0, 0, 0,
                0, 1, -imgPts[0].get()[i].pt.y, 0, 0, 0,
                0, 0, 0, 1, 0, -imgPts[1].get()[i].pt.x,
                0, 0, 0, 0, 1, -imgPts[1].get()[i].pt.y);
        Mat A = h * kr;
        Mat b = - h * kt;
        Mat x = (A.t() * A).inv() * A.t() * b;
        if (pt3D != nullptr) {
            x.copyTo(pt3D->rowRange(i*3, (i+1)*3));
        }
        if (errRecon != nullptr) {
            Mat(A * x - b).copyTo(errRecon->rowRange(i*4, (i+1)*4));
        }
        if (errImgPt != nullptr) {
            for (int j=0; j<2; j++) {
                Mat pt = kMats[j] * (rMats[j] * x + tVecs[j]);
                pt /= pt.at<double>(2, 0);
                double dis = sqrt(pow(pt.at<double>(0, 0) - imgPts[j].get()[i].pt.x, 2)
                                + pow(pt.at<double>(1, 0) - imgPts[j].get()[i].pt.y, 2));
                errImgPt->at<double>(i*2+j, 0) = dis;
            }
        }
    }
}

void Util::normalizeTVecs(const vector<Mat>& rMats, const vector<Mat>& tVecs,
                          const vector<IdxPair>& idxPairs, const double twTransStd,
                          vector<Mat>& tVecsNormalized) {
    vector<Mat> twVecs;
    Mat twVecsAvg = Mat::zeros(3, 1, CV_64FC1);
    for (int i=0; i<tVecs.size(); i++) {
        twVecs.emplace_back(- rMats[i].t() * tVecs[i]);
        twVecsAvg += *twVecs.rbegin();
    }
    twVecsAvg /= twVecs.size();

    double twTransNormAvg = 0;
    for (const auto& idxPair : idxPairs) {
        twTransNormAvg += norm(twVecs[IDX_PAIR_1(idxPair)] - twVecs[IDX_PAIR_0(idxPair)]);
    }
    twTransNormAvg /= idxPairs.size();

    double s = twTransStd / twTransNormAvg;
    FLOGD("normalizeTVecs : s = %f", s);

    tVecsNormalized.clear();
    for (int i=0; i<tVecs.size(); i++) {
        tVecsNormalized.emplace_back(- rMats[i] * (twVecs[i] - twVecsAvg) * s);
    }
}

void Util::getImgPtsInlier(const KeyPointListPair& imgPtsPair, const std::vector<int>& inlierIdxs,
                           KeyPointListPair& imgPtsPairInlier) {
    auto& [imgPts0, imgPts1] = imgPtsPairInlier;
    imgPts0.clear();
    imgPts0.reserve(inlierIdxs.size());
    imgPts1.clear();
    imgPts1.reserve(inlierIdxs.size());
    for (int idx : inlierIdxs) {
        imgPts0.emplace_back(KEY_POINT_LIST_PAIR_0(imgPtsPair)[idx]);
        imgPts1.emplace_back(KEY_POINT_LIST_PAIR_1(imgPtsPair)[idx]);
    }
}

double Util::cosTZ(const MatPair& rMatPair, const MatPair& tVecPair) {
    Mat rTrans = MAT_PAIR_1(rMatPair) * MAT_PAIR_0(rMatPair).t();
    Mat tTrans = MAT_PAIR_1(tVecPair) - rTrans * MAT_PAIR_0(tVecPair);

    Mat om = Mat::zeros(3, 1, CV_64FC1);
    cv::Rodrigues(rTrans, om);


    om *= -0.5;
    Mat rr = Mat::zeros(3, 3, CV_64FC1);
    cv::Rodrigues(om, rr);
    Mat t = rr * tTrans;
    t /= norm(t);

    Mat uu = (Mat_<double>(3, 1) << 0, 0, 1);
    return Mat(t.t() * uu).at<double>(0, 0);
}

Mat Util::generateMaskRect(const cv::Size& maskSize, const cv::Size& center, const cv::Size& size) {
    Mat mask = Mat::zeros(maskSize, CV_8UC1);
    cv::rectangle(mask,
                  Point(center.width - size.width/2, center.height - size.height/2),
                  Point(center.width + size.width/2, center.height + size.height/2),
                  Scalar(255, 255, 255), cv::FILLED);
    return mask;
}

Mat Util::generateMaskEllipse(const cv::Size& maskSize, const cv::Size& center, const cv::Size& size) {
    Mat mask = Mat::zeros(maskSize, CV_8UC1);
    cv::ellipse(mask,
                Point(center.width, center.height),
                Point(size.width/2, size.height/2),
                0, 0, 360, Scalar(255, 255, 255), cv::FILLED);
    return mask;
}

