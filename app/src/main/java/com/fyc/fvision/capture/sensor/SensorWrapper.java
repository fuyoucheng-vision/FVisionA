package com.fyc.fvision.capture.sensor;

import android.content.Context;

import java.lang.ref.WeakReference;

public abstract class SensorWrapper {

    private static final String TAG = SensorWrapper.class.getName();

    protected WeakReference<Context> contextRef;

    protected SensorWrapper(Context context) {
        contextRef = new WeakReference<>(context);
    }

    public void create() {
        onCreate();
    }

    public void destory() {
        onDestory();
    }

    public void pause() {
        onPause();
    }

    public void resume() {
        onResume();
    }

    protected abstract void onCreate();

    protected abstract void onDestory();

    protected abstract void onPause();

    protected abstract void onResume();

    public abstract float[] getRotationMatrix();

}
