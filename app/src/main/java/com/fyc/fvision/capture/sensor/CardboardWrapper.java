package com.fyc.fvision.capture.sensor;

import android.content.Context;

public class CardboardWrapper extends SensorWrapper {

    private static final String TAG = CardboardWrapper.class.getName();

    private long cardboardHandler;

    public CardboardWrapper(Context context) {
        super(context);
    }

    @Override
    protected void onCreate() {
        cardboardHandler = nativeCreate();
    }

    @Override
    public void onDestory() {
        nativeDestory(cardboardHandler);
    }

    @Override
    public void onPause() {
        nativePause(cardboardHandler);
    }

    @Override
    public void onResume() {
        nativeResume(cardboardHandler);
    }

    @Override
    public float[] getRotationMatrix() {
        return nativeGetRotationMatrix(cardboardHandler);
    }

    private native long nativeCreate();

    private native void nativeDestory(long cardboardHandler);

    private native void nativePause(long cardboardHandler);

    private native void nativeResume(long cardboardHandler);

    private native float[] nativeGetRotationMatrix(long cardboardHandler);

}
