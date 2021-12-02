package com.fyc.fvision.display;

import android.content.Context;
import android.opengl.GLSurfaceView;

import com.fyc.fvision.R;
import com.fyc.fvision.common.GLUtil;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class FRenderer implements GLSurfaceView.Renderer {

    private static final String TAG = FRenderer.class.getName();

    private int width = 0, height = 0;
    private String vsh, fsh;
    private float offsetX = 0, offsetY = 0, scale = 1;

    public FRenderer(Context context) {
        super();

        vsh = GLUtil.loadShaderStr(context, R.raw.display_vsh);
        fsh = GLUtil.loadShaderStr(context, R.raw.display_fsh);
    }

    public void loadVertex() {
        nativeLoadVertex();
    }

    public void updateVertex(String[] keys) {
        nativeUpdateVertex(keys);
    }

    public void onRotate(float _offsetX, float _offsetY) {
        offsetX = _offsetX;
        offsetY = _offsetY / height * width;
//        FLog.d(TAG, "onRotate : " + offsetX + ", " + offsetY);
    }

    public void onScale(float _scale) {
        scale = _scale;
//        FLog.d(TAG, "onScale : " + scale);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        nativeOnSurfaceCreated(vsh, fsh);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        this.width = width;
        this.height = height;
        nativeOnSurfaceChanged(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        nativeOnDrawFramed(offsetX, offsetY, scale);
        offsetX = 0;
        offsetY = 0;
    }

    private native void nativeOnSurfaceCreated(String vsh, String fsh);

    private native void nativeOnSurfaceChanged(int width, int height);

    private native void nativeOnDrawFramed(float offsetX, float offsetY, float scale);

    private native void nativeLoadVertex();

    private native void nativeUpdateVertex(String[] keys);

}
