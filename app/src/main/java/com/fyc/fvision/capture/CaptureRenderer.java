package com.fyc.fvision.capture;

import android.graphics.SurfaceTexture;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.view.Surface;

import com.fyc.fvision.R;
import com.fyc.fvision.common.FLog;
import com.fyc.fvision.common.GLUtil;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class CaptureRenderer implements GLSurfaceView.Renderer, SurfaceTexture.OnFrameAvailableListener {

    private static final String TAG = CaptureRenderer.class.getSimpleName();

    private static final String[] AttribNames = new String[]{"aPosition", "aUV"};
    private static final String[] UniformNames = new String[] {"sTexture", "uTextureTransform", "uRotationTransform", "iARGB2BGRA", "iVFlip"};

    private static final float[] VERTICES = {
            -1.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
    };
    private static final float[] UVS = {
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
    };
    private static final float[] UVAnchor = {
            0.5f, 0.5f
    };
    private static final short[] INDICES = {
            0, 1, 2,
            1, 3, 2,
    };

    private static final float[] surfaceTextureTransform = new float[] {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1,
    };
    private static final float[] rotationTransform = new float[] {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1,
    };

    private final GLUtil.Program program = new GLUtil.Program();
    private final GLUtil.FBO fbo = new GLUtil.FBO();

    private FloatBuffer verticesBuffer;
    private FloatBuffer uvsBuffer;
    private ShortBuffer indicesBuffer;

    private int surfaceTextureId;
    private SurfaceTexture surfaceTexture;
    private boolean isFrameAvailable = false;
    private final Object frameAvailableLock = new Object();

    private int displayRotation = Surface.ROTATION_0;
    private boolean isUpdateDisplayRotation = false;
    private final Object displayRotationLock = new Object();

    private int viewportWidth, viewportHeight;
    private int captureWidth, captureHeight = CaptureUtil.TARGET_CAPTURE_HEIGHT;
    private ByteBuffer pixelsBuffer;

    public interface SurfaceListener {
        void onSurfaceCreated();
        void onSurfaceChanged(int width, int height);
        void onDrawFrame();
    }
    private SurfaceListener surfaceListener;
    public void setSurfaceListener(SurfaceListener listener) {
        surfaceListener = listener;
    }

    public SurfaceTexture getSurfaceTexture() {
        return surfaceTexture;
    }

    public boolean isSurfaceCreated () {
        return surfaceTexture != null;
    }

    public void updateDisplayRotation(int displayRotation) {
        synchronized (displayRotationLock) {
            this.displayRotation = displayRotation;
            isUpdateDisplayRotation = true;
        }
    }

    public interface CaptureListener {
        void onCaptured(int[] buffer, int width, int height);
    }
    private CaptureListener captureListener;
    private final Object captureLock = new Object();
    public void capturePixels(CaptureListener listener) {
        synchronized (captureLock) {
            captureListener = listener;
        }
    }

    private final WeakReference<GLSurfaceView> glSurfaceViewRef;

    //*************************************************************

    public CaptureRenderer(GLSurfaceView glSurfaceView) {
        glSurfaceViewRef = new WeakReference<>(glSurfaceView);

        program.loadShader(glSurfaceView.getContext(), R.raw.capture_vsh, R.raw.capture_fsh);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        FLog.d(TAG, "onSurfaceCreated");

        // prepare vertices
        verticesBuffer = ByteBuffer.allocateDirect(VERTICES.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer().put(VERTICES);
        verticesBuffer.position(0);
        uvsBuffer = ByteBuffer.allocateDirect(UVS.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer().put(UVS);
        uvsBuffer.position(0);
        indicesBuffer = ByteBuffer.allocateDirect(INDICES.length * 2).order(ByteOrder.nativeOrder()).asShortBuffer().put(INDICES);
        indicesBuffer.position(0);

        // prepare shader
        program.compile(AttribNames, UniformNames);

        // create surfaceTexture
        surfaceTextureId = GLUtil.createTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
        surfaceTexture = new SurfaceTexture(surfaceTextureId);
        surfaceTexture.setOnFrameAvailableListener(this);
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);

        // create fbo
        fbo.onSurfaceCreated();

        if (surfaceListener != null) {
            surfaceListener.onSurfaceCreated();
        }
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        FLog.d(TAG, "onSurfaceChanged : " + width + ", " + height);

        viewportWidth = width;
        viewportHeight = height;
        captureWidth = (int)(width / (float)height * captureHeight + 0.5f);

        pixelsBuffer = ByteBuffer.allocateDirect(captureWidth * captureHeight * 4).order(ByteOrder.BIG_ENDIAN);

        GLES20.glViewport(0, 0, width, height);

        fbo.onSurfaceChanged(captureWidth, captureHeight);

        if (surfaceListener != null) {
            surfaceListener.onSurfaceChanged(width, height);
        }
    }

    @Override
    public void onDrawFrame(GL10 gl) {
//        FLog.d(TAG, "onDrawFrame");
        synchronized (frameAvailableLock) {
            if (isFrameAvailable) {
//                FLog.d(TAG, "updateTexImage");
                surfaceTexture.updateTexImage();
                surfaceTexture.getTransformMatrix(surfaceTextureTransform);
                isFrameAvailable = false;
            }
        }
        synchronized (displayRotationLock) {
            if (isUpdateDisplayRotation) {
                updateRotationTransform();
                isUpdateDisplayRotation = false;
            }
        }

        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        GLES20.glClearDepthf(1.0f);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

        // render surfaceTexture
        renderSurfaceTexture(false, false);

        // render to fbo, readPixels from fbo texture
        checkCaptureImage();

        if (surfaceListener != null) {
            surfaceListener.onDrawFrame();
        }
    }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
//        FLog.d(TAG, "onFrameAvailable");
        synchronized (frameAvailableLock) {
            isFrameAvailable = true;
        }
        final GLSurfaceView glSurfaceView = glSurfaceViewRef.get();
        if (glSurfaceView != null) {
            glSurfaceView.requestRender();
        }
    }

    private void updateRotationTransform() {
        float angle = 0f;
        if (Surface.ROTATION_90 == displayRotation) {
            angle = 90f;
        } else if (Surface.ROTATION_180 == displayRotation) {
            angle = 180f;
        } else if (Surface.ROTATION_270 == displayRotation) {
            angle = 270f;
        }
        angle = (float)Math.toRadians(angle);
        float cosr = (float)Math.cos(angle);
        float sinr = (float)Math.sin(angle);
        float[] t0 = new float[] {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                -UVAnchor[0], -UVAnchor[1], 0, 1,
        };
        float[] t1 = new float[] {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                UVAnchor[0], UVAnchor[1], 0, 1,
        };
        float[] r = new float[] {
                cosr, sinr, 0, 0,
                -sinr, cosr, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1,
        };
        Matrix.multiplyMM(rotationTransform, 0, r, 0, t0, 0);
        Matrix.multiplyMM(rotationTransform, 0, t1, 0, rotationTransform, 0);
    }

    private void renderSurfaceTexture(boolean isARGB2BGRA, boolean isVFlip) {
        GLES20.glUseProgram(program.getProgramHandler());

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, surfaceTextureId);
        GLES20.glUniform1i(program.getUniformHandler("sTexture"), 0);

        GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, 0);
        GLES20.glBindBuffer(GLES20.GL_ELEMENT_ARRAY_BUFFER, 0);

        GLES20.glVertexAttribPointer(program.getAttribHandler("aPosition"), 3, GLES20.GL_FLOAT, false, 0, verticesBuffer);
        GLES20.glEnableVertexAttribArray(program.getAttribHandler("aPosition"));

        GLES20.glVertexAttribPointer(program.getAttribHandler("aUV"), 2, GLES20.GL_FLOAT, false, 0, uvsBuffer);
        GLES20.glEnableVertexAttribArray(program.getAttribHandler("aUV"));

        GLES20.glUniformMatrix4fv(program.getUniformHandler("uTextureTransform"), 1, false, surfaceTextureTransform, 0);
        GLES20.glUniformMatrix4fv(program.getUniformHandler("uRotationTransform"), 1, false, rotationTransform, 0);

        GLES20.glUniform1i(program.getUniformHandler("iARGB2BGRA"), isARGB2BGRA ? 1 : 0);
        GLES20.glUniform1i(program.getUniformHandler("iVFlip"), isVFlip ? 1 : 0);

        GLES20.glDrawElements(GLES20.GL_TRIANGLES, indicesBuffer.remaining(), GLES20.GL_UNSIGNED_SHORT, indicesBuffer);
    }

    private void checkCaptureImage() {
        int[] ibuf = null;
        CaptureListener listener = null;
        synchronized (captureLock) {
            if (captureListener != null) {
                listener = captureListener;
                long timestamp = System.currentTimeMillis();

                GLES20.glViewport(0, 0, captureWidth, captureHeight);
                fbo.onDrawBegin();

                // render to fbo
                renderSurfaceTexture(true, true);

                // readPixels from fbo
                GLES20.glReadPixels(0, 0, captureWidth, captureHeight, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, pixelsBuffer);

                fbo.onDrawEnd();
                GLES20.glViewport(0, 0, viewportWidth, viewportHeight);

                FLog.d(TAG, "checkCaptureImage : renderSurfaceTexture & glReadPixels : " + (System.currentTimeMillis() - timestamp));
                pixelsBuffer.position(0);
                IntBuffer buf = pixelsBuffer.asIntBuffer();
                ibuf = new int[buf.remaining()];
                buf.get(ibuf);
                captureListener = null;
            }
        }
        if (listener != null) {
            listener.onCaptured(ibuf, captureWidth, captureHeight);
        }
    }

}
