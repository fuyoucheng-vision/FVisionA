package com.fyc.fvision.capture;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.ImageFormat;
import android.graphics.Point;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.util.Log;
import android.util.Size;
import android.util.SizeF;
import android.util.SparseIntArray;
import android.view.Surface;

import com.fyc.fvision.common.FLog;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.Timer;
import java.util.TimerTask;

public class CaptureUtil {

    private static final String TAG = CaptureUtil.class.getSimpleName();

    private static final int MAX_PREVIEW_WIDTH = 1920;
    private static final int MAX_PREVIEW_HEIGHT = 1080;

    public static final int TARGET_CAPTURE_HEIGHT = 720;

    private static final SparseIntArray ORIENTATIONS = new SparseIntArray();
    static {
        ORIENTATIONS.append(Surface.ROTATION_0, 90);
        ORIENTATIONS.append(Surface.ROTATION_90, 0);
        ORIENTATIONS.append(Surface.ROTATION_180, 270);
        ORIENTATIONS.append(Surface.ROTATION_270, 180);
    }

    public static class CaptureInfo {
        final public String cameraId;
        final public Size largestSize;
        final public Size previewSize;
        final public int sensorOrientation;
        final public SizeF physicalSize;
        final public float[] focal;

        public CaptureInfo(String cameraId, Size largestSize, Size previewSize, int sensorOrientation, SizeF physicalSize, float[] focal) {
            this.cameraId = cameraId;
            this.largestSize = largestSize;
            this.previewSize = previewSize;
            this.sensorOrientation = sensorOrientation;
            this.physicalSize = physicalSize;
            this.focal = focal;
        }
    }

    public static CaptureInfo getCaptureInfo(Activity activity, int cameraFacing, int imageFormat, int width, int height) {
        try {
            CameraManager manager = (CameraManager) activity.getSystemService(Context.CAMERA_SERVICE);
            for (String cameraId : manager.getCameraIdList()) {
                CameraCharacteristics characteristics = manager.getCameraCharacteristics(cameraId);

                // We don't use a front facing camera in this sample.
                Integer facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                if (facing != null && facing != cameraFacing) {
                    continue;
                }

                // save for calibration
                SizeF physicalSize = characteristics.get(CameraCharacteristics.SENSOR_INFO_PHYSICAL_SIZE); // mm
                float[] focal = characteristics.get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS); // mm
                FLog.d(TAG, String.format(Locale.CHINA, "getCaptureInfo : cameraId = %s, physicalSize = %s, focalCount = %s, focal[0] = %f",
                        cameraId, physicalSize.toString(), focal.length, focal[0]));

                StreamConfigurationMap map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                if (map == null) {
                    continue;
                }
                for (int format : map.getOutputFormats()) {
                    StringBuilder sb = new StringBuilder();
                    for (Size sz : map.getOutputSizes(format)) {
                        sb.append(sz);
                        sb.append(", ");
                    }
                    FLog.d(TAG, String.format(Locale.CHINA, "getCaptureInfo : cameraId = %s, format = %d (%s), outputSize = %s",
                            cameraId, format, getImageFormatName(format), sb.toString()));
                }

                Size largest = chooseCaptureSize(map, imageFormat);

                // Find out if we need to swap dimension to get the preview size relative to sensor
                // coordinate.
                int displayRotation = activity.getWindowManager().getDefaultDisplay().getRotation();
                //noinspection ConstantConditions
                int sensorOrientation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);
                boolean swappedDimensions = false;
                switch (displayRotation) {
                    case Surface.ROTATION_0:
                    case Surface.ROTATION_180:
                        if (sensorOrientation == 90 || sensorOrientation == 270) {
                            swappedDimensions = true;
                        }
                        break;
                    case Surface.ROTATION_90:
                    case Surface.ROTATION_270:
                        if (sensorOrientation == 0 || sensorOrientation == 180) {
                            swappedDimensions = true;
                        }
                        break;
                    default:
                        Log.e(TAG, "Display rotation is invalid: " + displayRotation);
                }

                Point displaySize = new Point();
                activity.getWindowManager().getDefaultDisplay().getSize(displaySize);
                int rotatedPreviewWidth = width;
                int rotatedPreviewHeight = height;
                int maxPreviewWidth = displaySize.x;
                int maxPreviewHeight = displaySize.y;

                if (swappedDimensions) {
                    rotatedPreviewWidth = height;
                    rotatedPreviewHeight = width;
                    maxPreviewWidth = displaySize.y;
                    maxPreviewHeight = displaySize.x;
                }

                if (maxPreviewWidth > MAX_PREVIEW_WIDTH) {
                    maxPreviewWidth = MAX_PREVIEW_WIDTH;
                }

                if (maxPreviewHeight > MAX_PREVIEW_HEIGHT) {
                    maxPreviewHeight = MAX_PREVIEW_HEIGHT;
                }

                // Danger, W.R.! Attempting to use too large a preview size could  exceed the camera
                // bus' bandwidth limitation, resulting in gorgeous previews but the storage of
                // garbage capture data.
                Size previewSize = chooseOptimalSize(map.getOutputSizes(SurfaceTexture.class),
                        rotatedPreviewWidth, rotatedPreviewHeight, maxPreviewWidth,
                        maxPreviewHeight, largest);

                return new CaptureInfo(cameraId, largest, previewSize, sensorOrientation, physicalSize, focal);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    private static Size chooseCaptureSize(StreamConfigurationMap map, int imageFormat) {
//        // from demo:
//        // For still image captures, we use the largest available size.
//        return Collections.max(Arrays.asList(map.getOutputSizes(imageFormat)), new CompareSizesByArea());

        // 最接近***P（***P附近面积最大）
        return Collections.min(Arrays.asList(map.getOutputSizes(imageFormat)), new Comparator<Size>() {
            @Override
            public int compare(Size s1, Size s2) {
                int d1 = Math.abs(s1.getHeight() - TARGET_CAPTURE_HEIGHT);
                int d2 = Math.abs(s2.getHeight() - TARGET_CAPTURE_HEIGHT);
                if (d1 == d2) {
                    return Long.signum((long) s1.getWidth() * s1.getHeight() -
                            (long) s2.getWidth() * s2.getHeight());
                } else {
                    return d1 - d2;
                }
            }
        });
    }

    private static Size chooseOptimalSize(Size[] choices, int textureViewWidth, int textureViewHeight, int maxWidth, int maxHeight, Size aspectRatio) {
        // Collect the supported resolutions that are at least as big as the preview Surface
        List<Size> bigEnough = new ArrayList<>();
        // Collect the supported resolutions that are smaller than the preview Surface
        List<Size> notBigEnough = new ArrayList<>();
        int w = aspectRatio.getWidth();
        int h = aspectRatio.getHeight();
        for (Size option : choices) {
            if (option.getWidth() <= maxWidth && option.getHeight() <= maxHeight &&
                    option.getHeight() == option.getWidth() * h / w) {
                if (option.getWidth() >= textureViewWidth &&
                        option.getHeight() >= textureViewHeight) {
                    bigEnough.add(option);
                } else {
                    notBigEnough.add(option);
                }
            }
        }
        // Pick the smallest of those big enough. If there is no one big enough, pick the
        // largest of those not big enough.
        if (bigEnough.size() > 0) {
            return Collections.min(bigEnough, new CompareSizesByArea());
        } else if (notBigEnough.size() > 0) {
            return Collections.max(notBigEnough, new CompareSizesByArea());
        } else {
            FLog.e(TAG, "Couldn't find any suitable preview size");
            return choices[0];
        }
    }

    private static class CompareSizesByArea implements Comparator<Size> {
        @Override
        public int compare(Size lhs, Size rhs) {
            // We cast here to ensure the multiplications won't overflow
            return Long.signum((long) lhs.getWidth() * lhs.getHeight() -
                    (long) rhs.getWidth() * rhs.getHeight());
        }
    }

    public static class CameraParams {
        public SizeF physicalSize;
        public float[] focal;

        public CameraParams(SizeF physicalSize, float[] focal) {
            this.physicalSize = physicalSize;
            this.focal = focal;
        }
    }

    private static CameraParams getCameraParams(Context context, String cameraId) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            CameraManager cm = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
            try {
                CameraCharacteristics cc = cm.getCameraCharacteristics(cameraId);
                SizeF size = cc.get(CameraCharacteristics.SENSOR_INFO_PHYSICAL_SIZE); // mm
                float[] focal = cc.get(CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS); // mm
                FLog.d(TAG, String.format(Locale.CHINA, "getCameraParams : cameraId = %s, physicalSize = %s, focalCount = %s, focal[0] = %f",
                        cameraId, size.toString(), focal.length, focal[0]));
                return new CameraParams(size, focal);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return null;
    }

    private static String getImageFormatName(int imageFormat) {
        String imageFormatName = "None";
        switch (imageFormat) {
            case ImageFormat.DEPTH16:
                imageFormatName = "DEPTH16";
                break;
            case ImageFormat.DEPTH_JPEG:
                imageFormatName = "DEPTH_JPEG";
                break;
            case ImageFormat.DEPTH_POINT_CLOUD:
                imageFormatName = "DEPTH_POINT_CLOUD";
                break;
            case ImageFormat.FLEX_RGB_888:
                imageFormatName = "FLEX_RGB_888";
                break;
            case ImageFormat.FLEX_RGBA_8888:
                imageFormatName = "FLEX_RGBA_8888";
                break;
            case ImageFormat.HEIC:
                imageFormatName = "HEIC";
                break;
            case ImageFormat.JPEG:
                imageFormatName = "JPEG";
                break;
            case ImageFormat.NV16:
                imageFormatName = "NV16";
                break;
            case ImageFormat.NV21:
                imageFormatName = "NV21";
                break;
            case ImageFormat.PRIVATE:
                imageFormatName = "PRIVATE";
                break;
            case ImageFormat.RAW10:
                imageFormatName = "RAW10";
                break;
            case ImageFormat.RAW12:
                imageFormatName = "RAW12";
                break;
            case ImageFormat.RAW_PRIVATE:
                imageFormatName = "RAW_PRIVATE";
                break;
            case ImageFormat.RAW_SENSOR:
                imageFormatName = "RAW_SENSOR";
                break;
            case ImageFormat.RGB_565:
                imageFormatName = "RGB_565";
                break;
            case ImageFormat.UNKNOWN:
                imageFormatName = "UNKNOWN";
                break;
            case ImageFormat.Y8:
                imageFormatName = "Y8";
                break;
            case ImageFormat.YUV_420_888:
                imageFormatName = "YUV_420_888";
                break;
            case ImageFormat.YUV_422_888:
                imageFormatName = "YUV_422_888";
                break;
            case ImageFormat.YUV_444_888:
                imageFormatName = "YUV_444_888";
                break;
            case ImageFormat.YUY2:
                imageFormatName = "YUY2";
                break;
            case ImageFormat.YV12:
                imageFormatName = "YV12";
                break;
        }
        return imageFormatName;
    }

    public static String getSavePath(Context context) {
        return context.getExternalFilesDir(null).getPath();
    }

    public static int getOrientation(int rotation, int sensorOrientation) {
        // Sensor orientation is 90 for most devices, or 270 for some devices (eg. Nexus 5X)
        // We have to take that into account and rotate JPEG properly.
        // For devices with orientation of 90, we simply return our mapping from ORIENTATIONS.
        // For devices with orientation of 270, we need to rotate the JPEG 180 degrees.
        return (ORIENTATIONS.get(rotation) + sensorOrientation + 270) % 360;
    }

    //*************************************************************

    public abstract static class Saver implements Runnable {

        public interface SaverListener {
            void onSaved(String imagePath);
        }

        private final String path;
        private final long timestamp;
        private final CaptureUtil.CaptureInfo captureInfo;
        private final float[] rotationMatrix;
        private final SaverListener listener;

        protected Saver(String path, long timestamp, CaptureUtil.CaptureInfo captureInfo, float[] rotationMatrix, SaverListener listener) {
            this.path = path;
            this.timestamp = timestamp;
            this.captureInfo = captureInfo;
            this.rotationMatrix = rotationMatrix;
            this.listener = listener;
        }

        abstract protected String getImageSuffix();
        abstract protected void saveImage(String imagePath);

        @Override
        public void run() {
            FLog.d(TAG, String.format(Locale.CHINA, "Saver : %s, %d, %f, %s",
                    path, timestamp, this.captureInfo.focal[0], this.captureInfo.physicalSize.toString()));
            if (rotationMatrix != null) {
                StringBuilder sb = new StringBuilder();
                for(float _rotateMatrix : rotationMatrix) {
                    sb.append(_rotateMatrix);
                    sb.append(", ");
                }
                FLog.d(TAG, String.format(Locale.CHINA, "Saver : rotateMatrix = [%s]", sb.toString()));
            } else {
                FLog.e(TAG, "Saver : rotateMatrix = null");
            }

//            if (this.captureInfo.sensorOrientation != 0) {
//                int orientationTag = ExifInterface.ORIENTATION_NORMAL;
//                if (this.captureInfo.sensorOrientation == 90) {
//                    orientationTag = ExifInterface.ORIENTATION_ROTATE_90;
//                } else if (this.captureInfo.sensorOrientation == 180) {
//                    orientationTag = ExifInterface.ORIENTATION_ROTATE_180;
//                } else if (this.captureInfo.sensorOrientation == 270) {
//                    orientationTag = ExifInterface.ORIENTATION_ROTATE_270;
//                }
//                try {
//                    ExifInterface exif = new ExifInterface(imagePath);
//                    exif.setAttribute(ExifInterface.TAG_ORIENTATION, "" + orientationTag);
//                    exif.saveAttributes();
//                } catch (Exception e) {
//                    e.printStackTrace();
//                }
//            }

            String infoPath = this.path + File.separator + this.timestamp + ".txt";
            String infoJson = "{}";
            try {
                // json保存float有精度问题，要转成string
                JSONObject json = new JSONObject();
                json.put("sensorOrientation", captureInfo.sensorOrientation);
                json.put("focal", convertFloat(captureInfo.focal[0]));
                JSONArray jsonPhysicalSize = new JSONArray();
                jsonPhysicalSize.put(convertFloat(captureInfo.physicalSize.getWidth()));
                jsonPhysicalSize.put(convertFloat(captureInfo.physicalSize.getHeight()));
                json.put("physicalSize", jsonPhysicalSize);
                JSONArray jsonRotationMatrix = new JSONArray();
                for (float _rotationMatrix : rotationMatrix) {
                    jsonRotationMatrix.put(convertFloat(_rotationMatrix));
                }
                json.put("rotationMatrix", jsonRotationMatrix);
                infoJson = json.toString();
            } catch (Exception e) {
                e.printStackTrace();
            }
            save(infoPath, infoJson.getBytes());

            String imagePath = this.path + File.separator + this.timestamp + getImageSuffix();
            saveImage(imagePath);

            if (listener != null) {
                listener.onSaved(imagePath);
            }
        }

        private String convertFloat(float value) {
//            return String.format(Locale.CHINA, "%.10f", value);
            return Float.valueOf(value).toString();
        }

        protected void save(String path, byte[] bytes) {
            File file = new File(path);
            if (file.exists()) {
                file.delete();
            }

            FileOutputStream output = null;
            try {
                output = new FileOutputStream(file);
                output.write(bytes);
            } catch (IOException e) {
                e.printStackTrace();
            } finally {
                if (null != output) {
                    try {
                        output.close();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
        }
    }

    public static class ImageSaver extends Saver {

        private final byte[][] imageBytes;
        private final int[] pixelStrides;
        private final int[] rowStrides;
        private final int format;
        private final int width;
        private final int height;

        public ImageSaver(byte[][] imageBytes, int[] pixelStrides, int[] rowStrides, int format, int width, int height,
                   String path, long timestamp, CaptureUtil.CaptureInfo captureInfo, float[] rotationMatrix, SaverListener listener) {
            super(path, timestamp, captureInfo, rotationMatrix, listener);
            this.imageBytes = imageBytes;
            this.pixelStrides = pixelStrides;
            this.rowStrides = rowStrides;
            this.format = format;
            this.width = width;
            this.height = height;
        }

        @Override
        protected String getImageSuffix() {
//            return format == ImageFormat.JPEG ? ".jpg" : ".png";
            return ".jpg";
        }

        @Override
        protected void saveImage(String imagePath) {
            FLog.d(TAG, "saveImage : " + getImageFormatName(format) + ", " + width + ", " + height);
            if (format == ImageFormat.JPEG) {
                save(imagePath, imageBytes[0]);
            } else if (format == ImageFormat.YUV_420_888) {
                nativeSaveImageYUV420888(imageBytes[0], imageBytes[1], imageBytes[2], pixelStrides, rowStrides, width, height, TARGET_CAPTURE_HEIGHT, imagePath);
            }
        }

        protected native void nativeSaveImageYUV420888(byte[] planeY, byte[] planeU, byte[] planeV,
                                                       int[] pixelStrides, int[] rowStrides, int width, int height, int targetHeight, String path);
    }

    public static class BufferSaver extends Saver {

        private final int[] buffer;
        private final int width;
        private final int height;

        public BufferSaver(int[] buffer, int width, int height,
                    String path, long timestamp, CaptureUtil.CaptureInfo captureInfo, float[] rotationMatrix, SaverListener listener) {
            super(path, timestamp, captureInfo, rotationMatrix, listener);
            this.buffer = buffer;
            this.width = width;
            this.height = height;
        }

        @Override
        protected String getImageSuffix() {
//            return ".png";
            return ".jpg";
        }

        @Override
        protected void saveImage(String imagePath) {
            nativeSaveImageBGRA(buffer, width, height, imagePath);
        }

        protected native void nativeSaveImageBGRA(int[] buffer, int width, int height, String path);
    }

    //*************************************************************

    public static class ContinuousTrigger {

        private final long period;
        private Timer timer;
        private final Runnable task;

        public ContinuousTrigger(long period, Runnable task) {
            this.period = period;
            this.task = task;
        }

        public void start() {
            if (timer != null) {
                timer.cancel();
            }
            timer = new Timer();
            timer.scheduleAtFixedRate(new TimerTask() {
                @Override
                public void run() {
                    task.run();
                }
            }, 0, period);
        }

        public void stop() {
            if (timer != null) {
                timer.cancel();
                timer = null;
            }
        }
    }

}
