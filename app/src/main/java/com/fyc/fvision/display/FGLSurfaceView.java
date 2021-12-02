package com.fyc.fvision.display;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

public class FGLSurfaceView extends GLSurfaceView
        implements GestureDetector.OnGestureListener, ScaleGestureDetector.OnScaleGestureListener {

    private static final String TAG = FGLSurfaceView.class.getName();

    private static final float TOUCH_OFFSET_FACTOR = 1.5f;

    private FRenderer renderer;
    private GestureDetector gestureDetector;
    private ScaleGestureDetector scaleGestureDetector;

    private float scale = 1.0f;
    private float scaleFactor = 1.0f;

    public FGLSurfaceView(Context context) {
        super(context);
        init(context);
    }

    public FGLSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    private void init(Context context) {
        setEGLContextClientVersion(2);
        renderer = new FRenderer(context);
        setRenderer(renderer);
        setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);

        gestureDetector = new GestureDetector(context, this);
        scaleGestureDetector = new ScaleGestureDetector(context, this);
    }

    public void loadVertex() {
        renderer.loadVertex();
    }

    public void updateVertex(String[] keys) {
        renderer.updateVertex(keys);
        requestRender();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        boolean ret = scaleGestureDetector.onTouchEvent(event);
        if (!scaleGestureDetector.isInProgress()) {
            ret = gestureDetector.onTouchEvent(event);
        }
        return ret;
    }

    // *************************************************************

    @Override
    public boolean onDown(MotionEvent motionEvent) {
        return true;
    }

    @Override
    public void onShowPress(MotionEvent motionEvent) {

    }

    @Override
    public boolean onSingleTapUp(MotionEvent motionEvent) {
        return false;
    }

    @Override
    public boolean onScroll(MotionEvent motionEvent, MotionEvent motionEvent1, float x, float y) {
//        FLog.d(TAG, "onScroll : " + x + ", " + y);
        renderer.onRotate(x / getWidth() * TOUCH_OFFSET_FACTOR, y / getWidth() * TOUCH_OFFSET_FACTOR);
        requestRender();
        return false;
    }

    @Override
    public void onLongPress(MotionEvent motionEvent) {

    }

    @Override
    public boolean onFling(MotionEvent motionEvent, MotionEvent motionEvent1, float v, float v1) {
        return false;
    }

    // *************************************************************

    @Override
    public boolean onScale(ScaleGestureDetector scaleGestureDetector) {
//        FLog.d(TAG, "onScale : " + scaleGestureDetector.getScaleFactor());
        renderer.onScale(scaleGestureDetector.getScaleFactor() * scaleFactor);
        requestRender();
        return false;
    }

    @Override
    public boolean onScaleBegin(ScaleGestureDetector scaleGestureDetector) {
//        FLog.d(TAG, "onScaleBegin : " + scaleGestureDetector.getScaleFactor());
        scaleFactor = scale / scaleGestureDetector.getScaleFactor();
        return true;
    }

    @Override
    public void onScaleEnd(ScaleGestureDetector scaleGestureDetector) {
//        FLog.d(TAG, "onScaleEnd : " + scaleGestureDetector.getScaleFactor());
        renderer.onScale(scaleGestureDetector.getScaleFactor() * scaleFactor);
        scale = scaleGestureDetector.getScaleFactor() * scaleFactor;
        requestRender();
    }

}
