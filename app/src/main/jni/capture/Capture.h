//
// Created by FuYoucheng on 3/4/2021.
//

#ifndef FVISIONA_CAPTURE_H
#define FVISIONA_CAPTURE_H

#include <string>

class Capture {
public:
    Capture();
    virtual ~Capture();

    static void saveImageYUV420888(const unsigned char* planes[], const int* pixelStrides, const int* rowStrides,
                                   int width, int height, int targetHeight, const std::string& path);

    static void saveImageBGRA(const int* buffer, int width, int height, const std::string& path);

protected:
    static void yuv2bgra(const unsigned char& y, const unsigned char& u, const unsigned char& v, int& pixel);

};


#endif //FVISIONA_CAPTURE_H
