attribute vec4 aPosition;
attribute vec4 aUV;
uniform mat4 uTextureTransform;
uniform mat4 uRotationTransform;
varying vec2 vUV;
void main()
{
  gl_Position = aPosition;
  vUV = (uRotationTransform * uTextureTransform * aUV).xy;
}