package com.fyc.fvision.display;

import android.support.v4.app.Fragment;

import com.fyc.fvision.common.BaseActivity;

public class DisplayActivity extends BaseActivity {

    @Override
    protected Fragment getFragment() {
        return new DisplayFragment();
    }

    protected boolean isFullScreen() {
        return true;
    }

}
