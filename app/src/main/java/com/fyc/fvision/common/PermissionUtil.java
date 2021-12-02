package com.fyc.fvision.common;

import android.Manifest;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.content.ContextCompat;

import com.fyc.fvision.R;

public class PermissionUtil {

    private static final String TAG = PermissionUtil.class.getName();

    private static final String PERMISSION_DIALOG_TAG = "permission_dialog";

    private static final String PERMISSION_DIALOG_ARGUMENT_PERMISSION = "permission";
    private static final String PERMISSION_DIALOG_ARGUMENT_REQUEST_CODE = "requestCode";

    public static boolean hasPermission(Context context, String permission) {
        return ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED;
    }

    public static void requestPermission(Fragment fragment, String permission, int requestCode) {
        if (fragment.shouldShowRequestPermissionRationale(permission)) {
            showPermissionDlg(fragment, permission, requestCode);
        } else {
            fragment.requestPermissions(new String[]{Manifest.permission.CAMERA}, requestCode);
        }
    }

    public static void onRequestPermissionsResult(@NonNull int[] grantResults, Fragment fragment, String permission, int requestCode) {
        if (grantResults.length != 1 || grantResults[0] != PackageManager.PERMISSION_GRANTED) {
            showPermissionDlg(fragment, permission, requestCode);
        }
    }

    private static void showPermissionDlg(Fragment fragment, String permission, int requestCode) {
        PermissionDialog dlg = new PermissionDialog();
        Bundle bundle = new Bundle();
        bundle.putString(PERMISSION_DIALOG_ARGUMENT_PERMISSION, permission);
        bundle.putInt(PERMISSION_DIALOG_ARGUMENT_REQUEST_CODE, requestCode);
        dlg.setArguments(bundle);
        dlg.show(fragment.getChildFragmentManager(), PERMISSION_DIALOG_TAG);
    }

    // *************************************************************

    public static class PermissionDialog extends DialogFragment {

        @NonNull
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final Fragment parent = getParentFragment();
            return new AlertDialog.Builder(getActivity())
                    .setMessage(R.string.request_permission)
                    .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                        if (parent != null)
                            if (getArguments() != null) {
                                parent.requestPermissions(
                                        new String[]{getArguments().getString(PERMISSION_DIALOG_ARGUMENT_PERMISSION)},
                                        getArguments().getInt(PERMISSION_DIALOG_ARGUMENT_REQUEST_CODE));
                            } else {
                                FLog.e(TAG, "PermissionDialog : arguments == null");
                            }
                    })
                    .setNegativeButton(android.R.string.cancel, (dialog, which) -> {
                        if (parent != null && parent.getActivity() != null)
                            parent.getActivity().finish();
                    })
                    .create();
        }
    }
}
