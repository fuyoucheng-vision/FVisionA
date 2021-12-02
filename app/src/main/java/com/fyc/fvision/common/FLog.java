package com.fyc.fvision.common;

import android.util.Log;

public class FLog {

    private static String getTag(String tag) {
        return "flog-"+tag;
    }

    public static void v(String tag, String msg) {
        Log.v(getTag(tag), msg);
    }

    public static void d(String tag, String msg) {
        Log.d(getTag(tag), msg);
    }

    public static void i(String tag, String msg) {
        Log.i(getTag(tag), msg);
    }

    public static void w(String tag, String msg) {
        Log.w(getTag(tag), msg);
    }

    public static void e(String tag, String msg) {
        Log.e(getTag(tag), msg);
    }

}
