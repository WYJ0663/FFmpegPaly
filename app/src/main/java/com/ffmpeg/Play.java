package com.ffmpeg;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;


/**
 * Created by LC on 2017/11/20.
 */

public class Play implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("ffmpeg");
        System.loadLibrary("native-lib");
    }

    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private Handler mMainHandler;

    private SurfaceView surfaceView;

    public Play() {
        mHandlerThread = new HandlerThread("play");
        mHandlerThread.start();

        mHandler = new Handler(mHandlerThread.getLooper());
        mMainHandler = new Handler(Looper.getMainLooper());
    }

    public void setSurfaceView(SurfaceView surfaceView) {
        this.surfaceView = surfaceView;

        surfaceView.getHolder().addCallback(this);

    }

    public void play(final String path) {
        if (surfaceView == null) {
            return;
        }

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _play(path);
            }
        });
    }

    public void display(final Surface surface) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _display(surface);
            }
        });
    }

    public void stop() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _stop();
            }
        });
    }

    public void pause() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _pause();
            }
        });
    }

    public void getTotalTime() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _getTotalTime();
            }
        });
    }

    public void getCurrentPosition() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _getCurrentPosition();
            }
        });
    }

    public void seekTo(final int msec) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _seekTo(msec);
            }
        });
    }

    public void stepBack() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _stepBack();
            }
        });
    }

    public void stepUp() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                _stepUp();
            }
        });
    }

    public native int _play(String path);

    public native void _display(Surface surface);

    public native void _stop();

    public native void _pause();

    public native int _getTotalTime();

    public native double _getCurrentPosition();

    public native void _seekTo(int sec);

    public native void _stepBack();//快退

    public native void _stepUp();//快进


    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {
        display(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int width, int height) {
//        display(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {

    }

    private int mWidth;
    private int mHeight;

    public void changeSize(final int width, final int height) {
        if (width > 0 && height > 0 && width != mWidth && height != mHeight) {
            mWidth = width;
            mHeight = height;
            if (mOnPlayCallback != null) {
                mMainHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mOnPlayCallback.onChangeSize(width, height);
                    }
                });
            }
        }
    }

    public void setTotalTime(final int sec) {
        if (mOnPlayCallback != null) {
            mMainHandler.post(new Runnable() {
                @Override
                public void run() {
                    mOnPlayCallback.onTotalTime(sec);
                }
            });
        }
    }

    public void setCurrentTime(final int sec) {
        if (mOnPlayCallback != null) {
            mMainHandler.post(new Runnable() {
                @Override
                public void run() {
                    mOnPlayCallback.onCurrentTime(sec);
                }
            });

        }
    }

    private OnPlayCallback mOnPlayCallback;

    public void setOnPlayCallback(OnPlayCallback onPlayCallback) {
        mOnPlayCallback = onPlayCallback;
    }

    /**
     * 时间单位为毫秒
     */
    public interface OnPlayCallback {
        void onChangeSize(int width, final int height);

        void onTotalTime(int sec);

        void onCurrentTime(int sec);
    }
}
