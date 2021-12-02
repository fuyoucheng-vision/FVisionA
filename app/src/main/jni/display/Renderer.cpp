//
// Created by FuYoucheng on 3/4/2021.
//

#include "Renderer.h"
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <fstream>
#include <sstream>
#include "ext.hpp"
#include "../FLog.h"

using namespace std;
using namespace cv;

Renderer::Renderer() : program(0), uMVPMatrixHandle(0), aPositionHandle(0), aColorHandle(0), aPointSizeHandle(0),
                       vertexArray(nullptr), colorArray(nullptr), vertexCount(0), vertexRange(1),
                       cameraDistance(5.f), totalX(0), totalY(0),
                       eye(0.f, 0.f, 1.f), center(0.f, 0.f, 0.f), up(0.f, 1.f, 0.f) {

}

Renderer::~Renderer() {
    releaseVertex();
}

void Renderer::releaseVertex() {
    vertexCount = 0;
    if (vertexArray != nullptr) {
        delete[] vertexArray;
    }
    vertexArray = nullptr;
    if (colorArray != nullptr) {
        delete[] colorArray;
    }
    colorArray = nullptr;
    for (auto it = vertexColorMap.begin(); it != vertexColorMap.end(); it++) {
        if (get<1>(it->second) != nullptr) {
            delete []get<1>(it->second);
        }
        if (get<2>(it->second) != nullptr) {
            delete []get<2>(it->second);
        }
    }
    vertexColorMap.clear();
    vertexCenter[0] = vertexCenter[1] = vertexCenter[2] = 0;
    vertexRange = 0;
}

void Renderer::loadVertex(const vector<IdxPair>& idxPairs,
                          const vector<vector<Mat>>& worldPtsList,
                          const vector<vector<Vec3b>>& colorsList) {
    releaseVertex();
    int count = 0;
    char idxStr[100] = {0};
    for (size_t i=0; i<idxPairs.size(); i++) {
        snprintf(idxStr, 100, "%d-%d", get<0>(idxPairs[i]), get<1>(idxPairs[i]));
        string key = idxStr;
        auto &pts = worldPtsList[i];
        auto &colors = colorsList[i];
        FLOGD("loadVertex : %s, %d", key.c_str(), pts.size());
        GLfloat *vertex = new GLfloat[3 * pts.size()], *pv = vertex;
        GLfloat *color = new GLfloat[4 * pts.size()], *pc = color;
        for (size_t j=0; j<pts.size(); j++) {
            *pv++ = pts[j].at<double>(0, 0);
            *pv++ = pts[j].at<double>(1, 0);
            *pv++ = pts[j].at<double>(2, 0);
            *pc++ = colors[j][2] / 255.f;
            *pc++ = colors[j][1] / 255.f;
            *pc++ = colors[j][0] / 255.f;
            *pc++ = 1.f;
            vertexCenter[0] += pts[j].at<double>(0, 0);
            vertexCenter[1] += pts[j].at<double>(1, 0);
            vertexCenter[2] += pts[j].at<double>(2, 0);
            count++;
        }
        vertexColorMap.insert(make_pair(key, make_tuple((int)pts.size(), vertex, color)));
    }
    for (int i=0; i<3; i++) {
        vertexCenter[i] /= count;
    }
    for (size_t i=0; i<idxPairs.size(); i++) {
        for (auto & pt : worldPtsList[i]) {
            vertexRange += sqrt(pow(pt.at<double>(0, 0) - vertexCenter[0], 2)
                    + pow(pt.at<double>(1, 0) - vertexCenter[1], 2)
                    + pow(pt.at<double>(2, 0) - vertexCenter[2], 2));
        }
    }
    vertexRange /= count;
    FLOGD("loadVertex : (%f, %f, %f), %f", vertexCenter[0], vertexCenter[1], vertexCenter[2], vertexRange);
}

void Renderer::updateVertex(const std::vector<std::string>& keys) {
    if (vertexArray != nullptr) {
        delete[] vertexArray;
    }
    vertexArray = nullptr;
    if (colorArray != nullptr) {
        delete[] colorArray;
    }
    vertexCount = 0;
    for (auto &key : keys) {
        auto it = vertexColorMap.find(key);
        if (it != vertexColorMap.end()) {
            vertexCount += get<0>(it->second);
        }
    }
    FLOGD("updateVertex : %d, %d", keys.size(), vertexCount);
    vertexArray = new GLfloat[3 * vertexCount];
    colorArray = new GLfloat[4 * vertexCount];
    int baseCount = 0;
    for (auto &key : keys) {
        auto it = vertexColorMap.find(key);
        if (it != vertexColorMap.end()) {
            int n = get<0>(it->second);
            memcpy(vertexArray + 3 * baseCount, get<1>(it->second), 3 * n * sizeof(GLfloat));
            memcpy(colorArray + 4 * baseCount, get<2>(it->second), 4 * n * sizeof(GLfloat));
            baseCount += n;
        }
    }
}

void Renderer::initGL(const string& vsh, const string& fsh) {
    program = createProgram(vsh, fsh);
    uMVPMatrixHandle = glGetUniformLocation(program, "uMVPMatrix");
    aPositionHandle = glGetAttribLocation(program, "aPosition");
    aColorHandle = glGetAttribLocation(program, "aColor");
    aPointSizeHandle = glGetAttribLocation(program, "aPointSize");

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glDisable(GL_DEPTH_TEST);
}

void Renderer::updateView(int width, int height) {
    projection = glm::perspective(45.f, (float) width / (float) height, 0.001f, 100.f);
    view = glm::lookAt(eye * cameraDistance, center, up);
    glViewport(0, 0, width, height);
}

void Renderer::draw(float offsetX, float offsetY, float scale) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double scaleFactor = 1 / vertexRange;

    glm::mat4x4 module(1.f);
    module = glm::scale(module, glm::vec3(scaleFactor * scale, scaleFactor * scale, scaleFactor * scale));
    module = glm::translate(module, glm::vec3(-vertexCenter[0], -vertexCenter[1], -vertexCenter[2]));

    totalY += offsetY;
    totalX += offsetX;
    glm::mat3 rMatX(1.f, 0.f, 0.f,
                    0.f, cos(totalY), sin(totalY),
                    0.f, -sin(totalY), cos(totalY));
    glm::mat3 rMatY(cos(totalX), 0.f, -sin(totalX),
                    0.0, 1.f, 0.f,
                    sin(totalX), 0.f, cos(totalX));
    view = glm::lookAt(rMatY * rMatX * eye * cameraDistance, center, rMatY * rMatX * up);

    glm::mat4 mvpMat = projection * view * module;
    auto mvp = (float *) glm::value_ptr(mvpMat);

    glUseProgram(program);
    glUniformMatrix4fv(uMVPMatrixHandle, 1, GL_FALSE, mvp);
    glVertexAttribPointer(aPositionHandle, 3, GL_FLOAT, GL_FALSE, 3 * 4, vertexArray);
    glVertexAttribPointer(aColorHandle, 4, GL_FLOAT, GL_FALSE, 4 * 4, colorArray);
    glVertexAttrib1f(aPointSizeHandle, min(max(2.f, (scale - 1.f) * 2.f + 2.f), 3.f));

    glEnableVertexAttribArray(aPositionHandle);
    glEnableVertexAttribArray(aColorHandle);
    glDrawArrays(GL_POINTS, 0, vertexCount);
}

int Renderer::createProgram(const std::string& vsh, const std::string& fsh) {
    GLint program = glCreateProgram();
    if (program == 0) {
        FLOGE("glCreateProgram failed");
        return 0;
    }
    int vshId = compileShader(GL_VERTEX_SHADER, vsh);
    int fshId = compileShader(GL_FRAGMENT_SHADER, fsh);
    if (vshId == 0 || fshId == 0) {
        return 0;
    }
    glAttachShader(program, vshId);
    glAttachShader(program, fshId);
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == 0) {
        glDeleteProgram(program);
        FLOGE("link program failed");
        return 0;
    }
    return program;
}

int Renderer::compileShader(int type, const string& shaderStr) {
    int shader = glCreateShader(type);
    if (shader == 0) {
        FLOGE("glCreateShader failed");
        return 0;
    }
    const char* sz = shaderStr.c_str();
    glShaderSource(shader, 1, &(sz), nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        glDeleteShader(shader);
        FLOGE("compile shader failed");
        return 0;
    }
    return shader;
}

void Renderer::loadVertex(const string& dirPath,
                          std::vector<IdxPair>& idxPairs,
                          std::vector<std::vector<cv::Mat>>& worldPtsList,
                          std::vector<std::vector<cv::Vec3b>>& colorsList) {
    idxPairs.clear();
    worldPtsList.clear();
    colorsList.clear();
    DIR *dir = opendir(dirPath.c_str());
    if (dir != nullptr) {
        struct dirent *file;
        while ((file = readdir(dir)) != nullptr) {
            if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) {
                continue;
            }
            string fileStr(file->d_name);
            if (fileStr.rfind("ply") != string::npos) {
                ifstream ifs;
                ifs.open((string(dirPath) + "/" + fileStr).c_str());
                if (ifs.is_open()) {
                    int pos0 = fileStr.find('_'), pos1 = fileStr.rfind('-'), pos2 = fileStr.rfind('.');
                    int idx0 = atoi(fileStr.substr(pos0+1, pos1-pos0-1).c_str());
                    int idx1 = atoi(fileStr.substr(pos1+1, pos2-pos1-1).c_str());
                    idxPairs.emplace_back(make_tuple(idx0, idx1));
                    worldPtsList.emplace_back(vector<Mat>());
                    colorsList.emplace_back(vector<Vec3b>());
                    vector<Mat>& pts = *worldPtsList.rbegin();
                    vector<Vec3b>& colors = *colorsList.rbegin();

                    char buf[1024] = {0};
                    int b, g, r;
                    bool isBegan = false;
                    stringstream ss;
                    while (ifs.getline(buf, 1024)) {
                        if (isBegan) {
                            ss.str("");
                            ss << buf;
                            Mat pt(3, 1, CV_64FC1);
                            Vec3b color;
                            ss >> pt.at<double>(0, 0) >> pt.at<double>(1, 0) >> pt.at<double>(2, 0);
                            ss >> b >> g >> r;
                            pts.push_back(pt);
                            colors.push_back(Vec3d(b, g, r));
                        } else if (strcmp(buf, "end_header") == 0) {
                            isBegan = true;
                        }
                    }
                }
            }
        }
    }
}

