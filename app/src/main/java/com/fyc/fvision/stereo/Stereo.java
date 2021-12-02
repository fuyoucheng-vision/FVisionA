package com.fyc.fvision.stereo;

import android.preference.PreferenceManager;

import com.fyc.fvision.capture.CaptureFragment;
import com.fyc.fvision.common.FApplication;
import com.fyc.fvision.common.FLog;

public class Stereo {

    private static final String TAG = Stereo.class.getName();

    public static final String ErrCode_AlreadyStarted = "E00001";

    public interface Callback {
        void onSucceed(String[] keys);
        void onError(String code, String msg);
        void onInfo(String info);
    }

    private Callback callback;
    private boolean isStarted;

    public Stereo() {

    }

    public void setCallback(Callback callback) {
        this.callback = callback;
    }

    public void start(String dir) {
        FLog.d(TAG, "start : " + dir);
        if (isStarted) {
            if (callback != null) {
                callback.onError(ErrCode_AlreadyStarted, "Already stared");
            }
            return;
        }
        new Thread(() -> {
            boolean isDebugInfo = PreferenceManager.getDefaultSharedPreferences(FApplication.getInstance()).getBoolean(CaptureFragment.PREF_KEY_DEBUG_INFO, false);
            nativeProcess(dir, isDebugInfo, callback);
        }).start();
    }

    private native void nativeProcess(String dir, boolean isDebugInfo, Callback callback);

}
