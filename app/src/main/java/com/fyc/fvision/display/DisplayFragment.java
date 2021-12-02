package com.fyc.fvision.display;

import android.app.Activity;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.animation.LinearInterpolator;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.Toast;

import com.fyc.fvision.R;
import com.fyc.fvision.common.FLog;
import com.fyc.fvision.stereo.Stereo;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class DisplayFragment extends Fragment {

    private static final String TAG = DisplayFragment.class.getName();

    private FGLSurfaceView glSurfaceView;
    private View vMask, vProgress;
    private RecyclerView recyclerView;
    private KeyAdapter adapter;

    private Stereo stereo;

    private List<Pair<String, Boolean>> keyList = new ArrayList<>();

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_display, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        glSurfaceView = view.findViewById(R.id.gl_view);
        vMask = view.findViewById(R.id.mask);
        vMask.setOnTouchListener((v, m) -> true);

        vProgress = view.findViewById(R.id.progress);
        Animation progressAnim = AnimationUtils.loadAnimation(getContext(), R.anim.progress_rotate);
        LinearInterpolator linearInterpolator = new LinearInterpolator();
        progressAnim.setInterpolator(linearInterpolator);
        vProgress.startAnimation(progressAnim);

        adapter = new KeyAdapter();
        recyclerView = view.findViewById(R.id.key_list);
        recyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        recyclerView.setAdapter(adapter);

        String capturePath = getActivity().getIntent().getStringExtra("CapturePath");
        if (TextUtils.isEmpty(capturePath)) {
            capturePath = getDebugPath();
        }
        FLog.d(TAG, "onViewCreated : capturePath : " + capturePath);

        stereo = new Stereo();
        stereo.setCallback(new Stereo.Callback() {
            @Override
            public void onSucceed(String[] keys) {
                FLog.d(TAG, "Stereo.onSucceed");
                for (String key : keys) {
                    FLog.d(TAG, "Stereo.onSucceed : " + key);
                }
                keyList.clear();
                for (String key : keys) {
                    keyList.add(new Pair<>(key, true));
                }

                final Activity activity = getActivity();
                if (activity != null) {
                    activity.runOnUiThread(() -> {
                        vMask.setVisibility(View.GONE);
                        vProgress.clearAnimation();
                        Toast.makeText(getContext(), "succeed", Toast.LENGTH_SHORT).show();

                        glSurfaceView.loadVertex();
                        updateVertex();
                        adapter.notifyDataSetChanged();
                    });
                }
            }

            @Override
            public void onError(String code, String msg) {
                FLog.e(TAG, "Stereo.onError : " + code + ", " + msg);
                final Activity activity = getActivity();
                if (activity != null) {
                    activity.runOnUiThread(() -> {
                        vMask.setVisibility(View.GONE);
                        vProgress.clearAnimation();
                        Toast.makeText(getContext(), "failed : " + code + ", " + msg, Toast.LENGTH_SHORT).show();
                    });
                }
            }

            @Override
            public void onInfo(String info) {
//                FLog.d(TAG, "Stereo.onInfo : " + info);
            }
        });
        stereo.start(capturePath);
    }

    private void updateVertex() {
        List<String> keys = new ArrayList<>();
        for (Pair<String, Boolean> key : keyList) {
            if (key.second) {
                keys.add(key.first);
            }
        }
        String[] keyArray = new String[keys.size()];
        glSurfaceView.updateVertex(keys.toArray(keyArray));
    }

    // *************************************************************

    private class KeyAdapter extends RecyclerView.Adapter<KeyAdapter.ViewHolder> {

        private class ViewHolder extends RecyclerView.ViewHolder implements CompoundButton.OnCheckedChangeListener {
            private CheckBox chk;

            public ViewHolder(View view) {
                super(view);
                chk = view.findViewById(R.id.chk);
                chk.setOnCheckedChangeListener(this);
            }

            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if(compoundButton.isPressed()) {
                    int idx = getAdapterPosition();
                    keyList.set(idx, new Pair<>(keyList.get(idx).first, isChecked));
                    updateVertex();
                }
            }
        }

        public KeyAdapter() {

        }

        @Override
        public KeyAdapter.ViewHolder onCreateViewHolder(ViewGroup viewGroup, int i) {
            View view = LayoutInflater.from(viewGroup.getContext()).inflate(R.layout.layout_key_item, viewGroup, false);
            ViewHolder holder = new ViewHolder(view);
            return holder;
        }

        @Override
        public void onBindViewHolder(KeyAdapter.ViewHolder viewHolder, int i) {
            Pair<String, Boolean> keyItem = keyList.get(i);
            viewHolder.chk.setText(keyItem.first);
            viewHolder.chk.setChecked(keyItem.second);
        }

        @Override
        public int getItemCount() {
            return keyList.size();
        }
    }

    // *************************************************************

    private String getDebugPath() {
        return getActivity().getExternalFilesDir(null).getPath() + File.separator + "debug";
    }

}
