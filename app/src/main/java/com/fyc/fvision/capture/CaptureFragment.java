package com.fyc.fvision.capture;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.media.Image;
import android.media.ImageReader;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.constraint.ConstraintLayout;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;
import android.widget.Toast;

import com.fyc.fvision.R;
import com.fyc.fvision.capture.sensor.CardboardWrapper;
import com.fyc.fvision.capture.sensor.SensorWrapper;
import com.fyc.fvision.common.FLog;
import com.fyc.fvision.common.PermissionUtil;
import com.fyc.fvision.display.DisplayActivity;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.TimerTask;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

public class CaptureFragment extends Fragment {

    private static final String TAG = CaptureFragment.class.getSimpleName();

    private static final int REQUEST_CODE_PERMISSION_CAMERA = 1;
    public static final String PREF_KEY_DEBUG_INFO = "PREF_KEY_DEBUG_INFO";
    private static final String PREF_KEY_CONTINUOUSLY = "PREF_KEY_CONTINUOUSLY";
    private static final int SAVE_IMAGE_THREADS = 3;
    private static final long CONTINUOUSLY_CAPTURE_PERIOD = 500;

    private enum CaptureState {
        Preview, WaitingLock, WaitingPrecapture, WaitingNonPrecapture, PictureTaken
    }

    private CaptureRenderer captureRenderer;
    private GLSurfaceView captureSurfaceView;

    private Button btnCapture;
    private TextView tvCount;
    private CheckBox chkDebugInfo;
    private CheckBox chkContinuously;

    private HandlerThread backgroundThread;
    private Handler backgroundHandler;

    private int cameraFacing = CameraCharacteristics.LENS_FACING_BACK;
    private CaptureUtil.CaptureInfo captureInfo;

    private static int PreviewImageFormat = ImageFormat.YUV_420_888;
    private static int CaptureImageFormat = ImageFormat.JPEG;

    private ImageReader previewImageReader, captureImageReader;
    private Semaphore cameraOpenCloseLock = new Semaphore(1);
    private CameraDevice cameraDevice;
    private CaptureRequest.Builder previewRequestBuilder;
    private CameraCaptureSession captureSession;
    private CaptureRequest previewRequest;
    private CaptureState captureState = CaptureState.Preview;

    private SensorWrapper sensorWrapper;

    private boolean isFirstCapture = true;
    private String savePath;

    private enum CaptureType { CaptureImageReader, PreviewImageReader, PreviewBuffer }
    private final CaptureType captureType = CaptureType.PreviewBuffer;

    private boolean isCapturePreviewImageReader;
    private final Object capturePreviewImageReaderLock = new Object();

    private ExecutorService saveImageThreadPool;

    private CaptureUtil.ContinuousTrigger continuousCaptureTrigger;
    private boolean isContinuouslyCapturing = false;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_capture, container, false);
    }

    @Override
    public void onViewCreated(@NonNull final View view, Bundle savedInstanceState) {
        savePath = getActivity().getExternalFilesDir(null).getPath() + File.separator + System.currentTimeMillis();

        btnCapture = view.findViewById(R.id.capture_image);
        btnCapture.setOnClickListener(v -> {
            if (isContinuously() && isContinuouslyCapturing) {
                isContinuouslyCapturing = false;
                continuousCaptureTrigger.stop();
                chkContinuously.setEnabled(true);
                btnCapture.setEnabled(true);
                btnCapture.setText(R.string.capture);
            } else {
                isContinuouslyCapturing = isContinuously();
                if (isFirstCapture) {
                    isFirstCapture = false;
                    File saveDirFile = new File(savePath);
                    if (saveDirFile.exists()) {
                        saveDirFile.deleteOnExit();
                    }
                    saveDirFile.mkdir();
                }
                chkContinuously.setEnabled(false);
                if (isContinuously()) {
                    btnCapture.setText(R.string.capturing);
                    continuousCaptureTrigger.start();
                } else {
                    btnCapture.setEnabled(false);
                    capture();
                }
            }
        });
        view.findViewById(R.id.capture_next).setOnClickListener(v -> {
            Intent intent = new Intent(getActivity(), DisplayActivity.class);
            intent.putExtra("CapturePath", savePath);
            startActivity(intent);
            getActivity().finish();
        });
        tvCount = view.findViewById(R.id.capture_count);
        chkDebugInfo = view.findViewById(R.id.capture_chk_debug_info);
        chkDebugInfo.setChecked(PreferenceManager.getDefaultSharedPreferences(getContext()).getBoolean(PREF_KEY_DEBUG_INFO, false));
        chkDebugInfo.setOnCheckedChangeListener((compoundButton, isChecked) -> {
            SharedPreferences.Editor editor = PreferenceManager.getDefaultSharedPreferences(getContext()).edit();
            editor.putBoolean(PREF_KEY_DEBUG_INFO, isChecked);
            editor.commit();
        });
        chkContinuously = view.findViewById(R.id.capture_chk_continuously);
        chkContinuously.setChecked(isContinuously());
        chkContinuously.setOnCheckedChangeListener((compoundButton, isChecked) -> {
            SharedPreferences.Editor editor = PreferenceManager.getDefaultSharedPreferences(getContext()).edit();
            editor.putBoolean(PREF_KEY_CONTINUOUSLY, isChecked);
            editor.commit();
        });

        captureSurfaceView = view.findViewById(R.id.capture_surface);
        captureSurfaceView.setEGLContextClientVersion(2);
        captureSurfaceView.getHolder().setFormat(PixelFormat.TRANSLUCENT);
        captureRenderer = new CaptureRenderer(captureSurfaceView);
        captureRenderer.setSurfaceListener(new CaptureRenderer.SurfaceListener() {
            @Override
            public void onSurfaceCreated() {
                final Activity activity = getActivity();
                if (null != activity) {
                    activity.runOnUiThread(() -> openCamera(captureSurfaceView.getWidth(), captureSurfaceView.getHeight()));
                }
            }

            @Override
            public void onSurfaceChanged(int width, int height) {

            }

            @Override
            public void onDrawFrame() {

            }
        });
        captureSurfaceView.setRenderer(captureRenderer);
        captureSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);

        sensorWrapper = new CardboardWrapper(getContext());
        sensorWrapper.create();

        saveImageThreadPool = Executors.newFixedThreadPool(SAVE_IMAGE_THREADS);

        continuousCaptureTrigger = new CaptureUtil.ContinuousTrigger(CONTINUOUSLY_CAPTURE_PERIOD, this::capture);
    }

    @Override
    public void onResume() {
        super.onResume();
        startBackgroundThread();
        sensorWrapper.resume();
        captureSurfaceView.onResume();
        if (captureRenderer.isSurfaceCreated() && captureSession == null) {
            openCamera(captureSurfaceView.getWidth(), captureSurfaceView.getHeight());
        }
    }

    @Override
    public void onPause() {
        closeCamera();
        stopBackgroundThread();
        sensorWrapper.pause();
        captureSurfaceView.onPause();
        super.onPause();
    }

    @Override
    public void onDestroyView() {
        sensorWrapper.destory();
        saveImageThreadPool.shutdown();
        super.onDestroyView();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == REQUEST_CODE_PERMISSION_CAMERA) {
            PermissionUtil.onRequestPermissionsResult(grantResults, this, Manifest.permission.CAMERA, requestCode);
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    // *************************************************************

    private CameraCaptureSession.CaptureCallback captureCallback = new CameraCaptureSession.CaptureCallback() {
        @Override
        public void onCaptureProgressed(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull CaptureResult partialResult) {
            process(partialResult);
        }

        @Override
        public void onCaptureCompleted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull TotalCaptureResult result) {
            process(result);
        }

        private void process(CaptureResult result) {
            switch (captureState) {
                case Preview: {
                    // We have nothing to do when the camera preview is working normally.
                    break;
                }
                case WaitingLock: {
                    Integer afState = result.get(CaptureResult.CONTROL_AF_STATE);
                    FLog.d(TAG, "process : " + captureState + ", " + afState);
                    if (afState == null) {
                        captureStillPicture();
                    } else if (CaptureResult.CONTROL_AF_STATE_FOCUSED_LOCKED == afState ||
                            CaptureResult.CONTROL_AF_STATE_NOT_FOCUSED_LOCKED == afState) {
                        // CONTROL_AE_STATE can be null on some devices
                        Integer aeState = result.get(CaptureResult.CONTROL_AE_STATE);
                        if (aeState == null ||
                                aeState == CaptureResult.CONTROL_AE_STATE_CONVERGED) {
                            captureState = CaptureState.PictureTaken;
                            captureStillPicture();
                        } else {
                            runPrecaptureSequence();
                        }
                    }
                    break;
                }
                case WaitingPrecapture: {
                    // CONTROL_AE_STATE can be null on some devices
                    Integer aeState = result.get(CaptureResult.CONTROL_AE_STATE);
                    FLog.d(TAG, "process : " + captureState + ", " + aeState);
                    if (aeState == null ||
                            aeState == CaptureResult.CONTROL_AE_STATE_PRECAPTURE ||
                            aeState == CaptureRequest.CONTROL_AE_STATE_FLASH_REQUIRED) {
                        captureState = CaptureState.WaitingNonPrecapture;
                    }
                    break;
                }
                case WaitingNonPrecapture: {
                    // CONTROL_AE_STATE can be null on some devices
                    Integer aeState = result.get(CaptureResult.CONTROL_AE_STATE);
                    FLog.d(TAG, "process : " + captureState + ", " + aeState);
                    if (aeState == null || aeState != CaptureResult.CONTROL_AE_STATE_PRECAPTURE) {
                        captureState = CaptureState.PictureTaken;
                        captureStillPicture();
                    }
                    break;
                }
            }
        }
    };

    private void startBackgroundThread() {
        backgroundThread = new HandlerThread("CameraBackground");
        backgroundThread.start();
        backgroundHandler = new Handler(backgroundThread.getLooper());
    }

    private void stopBackgroundThread() {
        backgroundThread.quitSafely();
        try {
            backgroundThread.join();
            backgroundThread = null;
            backgroundHandler = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void openCamera(int width, int height) {
        FLog.d(TAG, "openCamera : " + width + ", " + height);
        if (!PermissionUtil.hasPermission(getContext(), Manifest.permission.CAMERA)) {
            PermissionUtil.requestPermission(this, Manifest.permission.CAMERA, REQUEST_CODE_PERMISSION_CAMERA);
            return;
        }

        captureInfo = CaptureUtil.getCaptureInfo(getActivity(), cameraFacing, CaptureImageFormat, width, height);
        if (captureInfo == null) {
            FLog.e(TAG, "openCamera : captureInfo == null!");
            return;
        }
        FLog.d(TAG, String.format("captureInfo : cameraId = %s, largestSize = %s, previewSize = %s",
                captureInfo.cameraId, captureInfo.largestSize.toString(), captureInfo.previewSize.toString()));

        setUpCameraOutputs(captureInfo, PreviewImageFormat, CaptureImageFormat);

        try {
            CameraManager manager = (CameraManager) getActivity().getSystemService(Context.CAMERA_SERVICE);
            if (!cameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS)) {
                throw new RuntimeException("Time out waiting to lock camera opening.");
            }
            manager.openCamera(captureInfo.cameraId, new CameraDevice.StateCallback() {
                @Override
                public void onOpened(@NonNull CameraDevice cameraDevice) {
                    cameraOpenCloseLock.release();
                    CaptureFragment.this.cameraDevice = cameraDevice;
                    createCameraPreviewSession();
                }

                @Override
                public void onDisconnected(@NonNull CameraDevice camera) {
                    cameraOpenCloseLock.release();
                    cameraDevice.close();
                    CaptureFragment.this.cameraDevice = null;
                }

                @Override
                public void onError(@NonNull CameraDevice camera, int error) {
                    cameraOpenCloseLock.release();
                    cameraDevice.close();
                    CaptureFragment.this.cameraDevice = null;
                    FLog.e(TAG, "CameraDevice.StateCallback.onError : " + error);
                    Activity activity = getActivity();
                    if (null != activity) {
                        activity.finish();
                    }
                }
            }, backgroundHandler);
        } catch (SecurityException e) {
            e.printStackTrace();
        } catch (CameraAccessException e) {
            e.printStackTrace();
        } catch (InterruptedException e) {
            throw new RuntimeException("Interrupted while trying to lock camera opening.", e);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void setUpCameraOutputs(CaptureUtil.CaptureInfo captureInfo, final int previewImageFormat, final int captureImageFormat) {
        previewImageReader = ImageReader.newInstance(captureInfo.previewSize.getWidth(), captureInfo.previewSize.getHeight(), previewImageFormat, 2);
        previewImageReader.setOnImageAvailableListener(reader -> {
//            FLog.d(TAG, "onImageAvailable : preview");
            Image image = reader.acquireLatestImage();
            synchronized (capturePreviewImageReaderLock) {
                if (isCapturePreviewImageReader) {
                    isCapturePreviewImageReader = false;
                    FLog.d(TAG, "onImageAvailable : preview : saveImage");
                    saveImage(image, previewImageFormat, captureInfo.previewSize.getWidth(), captureInfo.previewSize.getHeight());
                    return;
                }
            }
            image.close();
        }, backgroundHandler);

        captureImageReader = ImageReader.newInstance(captureInfo.largestSize.getWidth(), captureInfo.largestSize.getHeight(), captureImageFormat, 2);
        captureImageReader.setOnImageAvailableListener(reader -> {
            FLog.d(TAG, "onImageAvailable : capture");
            saveImage(reader.acquireLatestImage(), captureImageFormat, captureInfo.largestSize.getWidth(), captureInfo.largestSize.getHeight());
        }, backgroundHandler);

        ConstraintLayout.LayoutParams lp = (ConstraintLayout.LayoutParams) captureSurfaceView.getLayoutParams();
        int orientation = getResources().getConfiguration().orientation;
        if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
            lp.width = (int)((float)captureSurfaceView.getMeasuredHeight() / (float)captureInfo.previewSize.getHeight() * (float)captureInfo.previewSize.getWidth());
            lp.height = captureSurfaceView.getMeasuredHeight();
        } else {
            lp.width = captureSurfaceView.getMeasuredWidth();
            lp.height = (int)((float)captureSurfaceView.getMeasuredWidth() / (float)captureInfo.previewSize.getHeight() * (float)captureInfo.previewSize.getWidth());
        }
        captureSurfaceView.setLayoutParams(lp);

        final Activity activity = getActivity();
        if (null != activity) {
            captureRenderer.updateDisplayRotation(activity.getWindowManager().getDefaultDisplay().getRotation());
        }
    }

    private void saveImage(Image image, int format, int width, int height) {
        int planeCount = image.getPlanes().length;
        byte[][] imageBytes = new byte[planeCount][];
        int[] pixelStrides = new int[planeCount];
        int[] rowStrides = new int[planeCount];
        for (int i=0; i<planeCount; i++) {
            Image.Plane plane = image.getPlanes()[i];
            ByteBuffer buffer = plane.getBuffer();
            imageBytes[i] = new byte[buffer.remaining()];
            buffer.get(imageBytes[i]);
            pixelStrides[i] = plane.getPixelStride();
            rowStrides[i] = plane.getRowStride();
            FLog.d(TAG, "saveImage : plane : " + i + ", " + imageBytes[i].length + ", " + pixelStrides[i] + ", " + rowStrides[i]);
        }
        image.close();

        saveImageThreadPool.execute(new CaptureUtil.ImageSaver(
                imageBytes, pixelStrides, rowStrides, format, width, height,
                savePath, System.currentTimeMillis(), captureInfo, sensorWrapper.getRotationMatrix(), this::onImageSaved));
    }

    private void saveImage(int[] buffer, int width, int height) {
        saveImageThreadPool.execute(new CaptureUtil.BufferSaver(
                buffer, width, height,
                savePath, System.currentTimeMillis(), captureInfo, sensorWrapper.getRotationMatrix(), this::onImageSaved));
    }

    private void onImageSaved(String imagePath) {
        FLog.d(TAG, "onImageSaved : " + imagePath);
        final Activity ac = getActivity();
        if (ac != null) {
            ac.runOnUiThread(() -> {
                if (!isContinuously()) {
                    btnCapture.setEnabled(true);
                    chkContinuously.setEnabled(true);
                    Toast.makeText(getContext(), "Capture succeed!", Toast.LENGTH_SHORT).show();
                }
                tvCount.setText(String.valueOf(Integer.parseInt(tvCount.getText().toString())+1));
            });
        }
    }

    private void createCameraPreviewSession() {
        FLog.d(TAG, "createCameraPreviewSession");
        try {
            final SurfaceTexture captureTexture = captureRenderer.getSurfaceTexture();
            assert captureTexture != null;

            // We configure the size of default buffer to be the size of camera preview we want.
            captureTexture.setDefaultBufferSize(captureInfo.previewSize.getWidth(), captureInfo.previewSize.getHeight());

            // This is the output Surface we need to start preview.
            Surface surface = new Surface(captureTexture);

            // We set up a CaptureRequest.Builder with the output Surface.
            previewRequestBuilder = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            previewRequestBuilder.addTarget(surface);
            if (captureType == CaptureType.PreviewImageReader) {
                previewRequestBuilder.addTarget(previewImageReader.getSurface());
            }

            // Here, we create a CameraCaptureSession for camera preview.
            cameraDevice.createCaptureSession(Arrays.asList(surface, previewImageReader.getSurface(), captureImageReader.getSurface()),
                    new CameraCaptureSession.StateCallback() {

                        @Override
                        public void onConfigured(@NonNull CameraCaptureSession cameraCaptureSession) {
                            // The camera is already closed
                            if (null == cameraDevice) {
                                return;
                            }

                            // When the session is ready, we start displaying the preview.
                            captureSession = cameraCaptureSession;
                            try {
                                // Auto focus should be continuous for camera preview.
                                previewRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);

                                // Finally, we start displaying the camera preview.
                                previewRequest = previewRequestBuilder.build();
                                captureSession.setRepeatingRequest(previewRequest, captureCallback, backgroundHandler);
                            } catch (CameraAccessException e) {
                                e.printStackTrace();
                            }
                        }

                        @Override
                        public void onConfigureFailed(@NonNull CameraCaptureSession cameraCaptureSession) {
                            FLog.e(TAG, "CameraCaptureSession.StateCallback : onConfigureFailed : failed");
                        }
                    }, null
            );
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private void closeCamera() {
        FLog.d(TAG, "closeCamera");
        try {
            cameraOpenCloseLock.acquire();
            if (null != captureSession) {
                captureSession.close();
                captureSession = null;
            }
            if (null != cameraDevice) {
                cameraDevice.close();
                cameraDevice = null;
            }
            if (null != previewImageReader) {
                previewImageReader.close();
                previewImageReader = null;
            }
            if (null != captureImageReader) {
                captureImageReader.close();
                captureImageReader = null;
            }
        } catch (InterruptedException e) {
            throw new RuntimeException("Interrupted while trying to lock camera closing.", e);
        } finally {
            cameraOpenCloseLock.release();
        }
    }

    private boolean isContinuously() {
        return PreferenceManager.getDefaultSharedPreferences(getContext()).getBoolean(PREF_KEY_CONTINUOUSLY, false);
    }

    private void capture() {
        if (captureType == CaptureType.CaptureImageReader) {
            lockFocus();
        } else if (captureType == CaptureType.PreviewImageReader) {
            synchronized (capturePreviewImageReaderLock) {
                isCapturePreviewImageReader = true;
            }
        } else if (captureType == CaptureType.PreviewBuffer) {
            captureRenderer.capturePixels(this::saveImage);
        }
    }

    private void lockFocus() {
        FLog.d(TAG, "lockFocus");
        try {
            // This is how to tell the camera to lock focus.
            previewRequestBuilder.set(CaptureRequest.CONTROL_AF_TRIGGER, CameraMetadata.CONTROL_AF_TRIGGER_START);
            // Tell #mCaptureCallback to wait for the lock.
            captureState = CaptureState.WaitingLock;
            captureSession.capture(previewRequestBuilder.build(), captureCallback, backgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private void runPrecaptureSequence() {
        FLog.d(TAG, "runPrecaptureSequence");
        try {
            // This is how to tell the camera to trigger.
            previewRequestBuilder.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER, CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START);
            // Tell #mCaptureCallback to wait for the precapture sequence to be set.
            captureState = CaptureState.WaitingPrecapture;
            captureSession.capture(previewRequestBuilder.build(), captureCallback, backgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private void captureStillPicture() {
        FLog.d(TAG, "captureStillPicture");
        try {
            final Activity activity = getActivity();
            if (null == activity || null == cameraDevice) {
                return;
            }
            // This is the CaptureRequest.Builder that we use to take a picture.
            final CaptureRequest.Builder captureBuilder = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
            captureBuilder.addTarget(captureImageReader.getSurface());

            // Use the same AE and AF modes as the preview.
            captureBuilder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);

            captureBuilder.set(CaptureRequest.JPEG_QUALITY, (byte)100);

            // Orientation
            int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();
            captureBuilder.set(CaptureRequest.JPEG_ORIENTATION, CaptureUtil.getOrientation(rotation, captureInfo.sensorOrientation));

            captureSession.stopRepeating();
            captureSession.abortCaptures();
            captureSession.capture(captureBuilder.build(), new CameraCaptureSession.CaptureCallback() {
                @Override
                public void onCaptureCompleted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull TotalCaptureResult result) {
                    FLog.d(TAG, "onCaptureCompleted");
                    unlockFocus();
                }
            }, null);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private void unlockFocus() {
        FLog.d(TAG, "unlockFocus");
        try {
            // Reset the auto-focus trigger
            previewRequestBuilder.set(CaptureRequest.CONTROL_AF_TRIGGER, CameraMetadata.CONTROL_AF_TRIGGER_CANCEL);
            captureSession.capture(previewRequestBuilder.build(), captureCallback, backgroundHandler);
            // After this, the camera will go back to the normal state of preview.
            captureState = CaptureState.Preview;
            captureSession.setRepeatingRequest(previewRequest, captureCallback, backgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

}
