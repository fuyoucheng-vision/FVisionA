package com.fyc.fvision.common;

import android.content.Context;
import android.opengl.GLES20;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.Buffer;
import java.util.HashMap;
import java.util.Map;

public class GLUtil {

    private static final String TAG = GLUtil.class.getName();

    public static String loadShaderStr(Context context, int resId) {
        StringBuilder shaderStr = new StringBuilder();
        try {
            InputStream is = context.getResources().openRawResource(resId);
            BufferedReader reader = new BufferedReader(new InputStreamReader(is));
            String line;
            while ((line = reader.readLine()) != null) {
                shaderStr.append(line);
                shaderStr.append("\n");
            }
            shaderStr.deleteCharAt(shaderStr.length() - 1);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return shaderStr.toString();
    }

    public static int compileShaders(String vertexShaderCode, String fragmentShaderCode) {
        int vertexShader = compileShader(GLES20.GL_VERTEX_SHADER, vertexShaderCode);
        int fragmentShader = compileShader(GLES20.GL_FRAGMENT_SHADER, fragmentShaderCode);
        int program = GLES20.glCreateProgram();
        GLES20.glAttachShader(program, vertexShader);
        GLES20.glAttachShader(program, fragmentShader);
        GLES20.glLinkProgram(program);

        final int[] linkStatus = new int[1];
        GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
        if (linkStatus[0] == 0)
        {
            FLog.e(TAG, "compileShaders : link failed : " + GLES20.glGetProgramInfoLog(program));
            GLES20.glDeleteProgram(program);
            return -1;
        }

        return program;
    }

    private static int compileShader(int type, String shaderCode){
        int shader = GLES20.glCreateShader(type);
        GLES20.glShaderSource(shader, shaderCode);
        GLES20.glCompileShader(shader);
        return shader;
    }

    public static int createTexture(int target) {
        int[] texture = new int[1];
        GLES20.glGenTextures(1, texture, 0);
        GLES20.glBindTexture(target, texture[0]);
        GLES20.glTexParameteri(target, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_REPEAT);
        GLES20.glTexParameteri(target, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_REPEAT);
        GLES20.glTexParameteri(target, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(target, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        return texture[0];
    }

    public void checkGLError(String tip) {
        int glError;
        while ((glError = GLES20.glGetError()) != GLES20.GL_NO_ERROR) {
            FLog.e(TAG, String.format("checkGLError : 0x%x, %s", glError, tip));
        }
    }

    //*************************************************************

    public static class Program {
        private String vsh, fsh;
        private int programHandler = -1;
        private final Map<String, Integer> attribHandlers = new HashMap<>();
        private final Map<String, Integer>  uniformHandlers = new HashMap<>();

        public void loadShader(Context context, int vshResId, int fshResId) {
            vsh = loadShaderStr(context, vshResId);
            fsh = loadShaderStr(context, fshResId);
        }

        public void compile(String[] attribNames, String[] uniformNames) {
            programHandler = compileShaders(vsh, fsh);
            for (String attribName : attribNames) {
                attribHandlers.put(attribName, GLES20.glGetAttribLocation(programHandler, attribName));
            }
            for (String uniformName : uniformNames) {
                uniformHandlers.put(uniformName, GLES20.glGetUniformLocation(programHandler, uniformName));
            }
        }

        public int getProgramHandler() {
            return programHandler;
        }

        public int getAttribHandler(String attribName) {
            return attribHandlers.getOrDefault(attribName, -1);
        }

        public int getUniformHandler(String uniformName) {
            return uniformHandlers.getOrDefault(uniformName, -1);
        }
    }

    //*************************************************************

    public static class FBO {
        private int fboId = -1;
        private int fboTextureId = -1;

        public void onSurfaceCreated() {
            // create fbo
            int[] fbo = new int[1];
            GLES20.glGenFramebuffers(1, fbo, 0);
            fboId = fbo[0];
        }

        public void onSurfaceChanged(int width, int height) {
            // create fbo texture
            fboTextureId = createTexture(GLES20.GL_TEXTURE_2D);
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, width, height, 0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        }

        public void onDrawBegin() {
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, fboId);
            GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, fboTextureId, 0);
        }

        public void onDrawEnd() {
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
        }
    }

}
