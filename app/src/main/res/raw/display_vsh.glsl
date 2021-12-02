attribute vec4 aPosition;
attribute vec4 aColor;
varying vec4 vColor;
uniform mat4 uMVPMatrix;
attribute float aPointSize;
void main()
{
    gl_Position = uMVPMatrix * aPosition;
    gl_PointSize = aPointSize;
    vColor = aColor;
}
