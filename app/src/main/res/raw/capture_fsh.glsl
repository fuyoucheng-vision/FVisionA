#extension GL_OES_EGL_image_external : require
precision mediump float;
varying vec2 vUV;
uniform samplerExternalOES sTexture;
uniform int iARGB2BGRA;
uniform int iVFlip;
void main()
{
  vec2 uv = vUV;
  if (iVFlip != 0) {
    uv.y = (1.0 - vUV.y);
  }

  vec4 color = texture2D(sTexture, uv);
  if (iARGB2BGRA != 0) {
    color = vec4(color.a, color.r, color.g, color.b);
  }

  gl_FragColor = color;
}