//
// Created by FuYoucheng on 3/4/2021.
//

#ifndef FVISIONA_DISPLAY_RENDERER_H
#define FVISIONA_DISPLAY_RENDERER_H

#include <string>
#include <vector>
#include <GLES2/gl2.h>
#include "mat4x4.hpp"
#include "opencv2/opencv.hpp"
#include "../stereo/Util.h"

class Renderer {
public:
    Renderer();
    virtual ~Renderer();

    void loadVertex(const std::vector<IdxPair>& idxPairs,
                    const std::vector<std::vector<cv::Mat>>& worldPtsList,
                    const std::vector<std::vector<cv::Vec3b>>& colorsList);
    void updateVertex(const std::vector<std::string>& keys);

    void initGL(const std::string& vsh, const std::string& fsh);

    void updateView(int width, int height);

    void draw(float offsetX, float offsetY, float scale);

    // debug
    void loadVertex(const std::string& dirPath,
                    std::vector<IdxPair>& idxPairs,
                    std::vector<std::vector<cv::Mat>>& worldPtsList,
                    std::vector<std::vector<cv::Vec3b>>& colorsList);

protected:
    void releaseVertex();

    static int createProgram(const std::string& vsh, const std::string& fsh);
    static int compileShader(int type, const std::string& shaderStr);

protected:
    GLint program;
    GLint uMVPMatrixHandle;
    GLint aPositionHandle;
    GLint aColorHandle;
    GLint aPointSizeHandle;

    GLfloat* vertexArray;
    GLfloat* colorArray;
    int vertexCount;

    double vertexCenter[3] = {0};
    double vertexRange;

    std::map<const std::string, std::tuple<int, GLfloat*, GLfloat*>> vertexColorMap;

    glm::mat4 projection;
    glm::mat4 view;
    float cameraDistance;

    double totalX, totalY;

    const glm::vec3 eye, center, up;

};


#endif //FVISIONA_DISPLAY_RENDERER_H
