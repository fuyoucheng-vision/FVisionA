package com.fyc.fvision.common;

import android.app.Application;

public class FApplication extends Application {

    private static FApplication instance;

    public static synchronized FApplication getInstance() {
        return instance;
    }

    static {
        System.loadLibrary("fvision_jni");
    }

    public FApplication() {
        super();
        instance = this;
    }

}
