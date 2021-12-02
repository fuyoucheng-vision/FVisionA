package com.fyc.fvision.capture;

import android.support.v4.app.Fragment;

import com.fyc.fvision.common.BaseActivity;

public class CaptureActivity extends BaseActivity {

    @Override
    protected Fragment getFragment() {
        return new CaptureFragment();
    }

    protected boolean isFullScreen() {
        return true;
    }
}
