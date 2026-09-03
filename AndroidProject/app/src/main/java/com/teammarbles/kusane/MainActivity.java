package com.teammarbles.kusane;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.os.PerformanceHintManager;
import android.os.Process;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.util.Log;
import android.widget.TextView;
import org.fmod.FMOD;
import java.util.concurrent.locks.LockSupport;

public class MainActivity extends AppCompatActivity implements SurfaceHolder.Callback {

    // Used to load the 'gam300android' library on application startup.
    static {
        System.loadLibrary("gam300android");
    }

    private static final String TAG = "GAM300";
    private static final float TARGET_FRAME_RATE = 60.0f;
    private static final long TARGET_FRAME_TIME_NS = 1_000_000_000L / 60L;
    // The 3D renderer scales internally, but tone mapping and deferred UI run
    // at surface resolution. Avoid spending frame time on 1440p/ultrawide phone
    // buffers that are visually indistinguishable at normal viewing distance.
    private static final int MAX_SURFACE_LONG_EDGE = 1920;
    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    private GameThread gameThread;
    private boolean engineReady = false;
    private float touchToSurfaceScaleX = 1.0f;
    private float touchToSurfaceScaleY = 1.0f;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // --- IMMERSIVE FULLSCREEN ---
        hideSystemUI();

        Log.i(TAG, "MainActivity onCreate");

        // Initialize FMOD
        FMOD.init(this);

        // Create a surface view for OpenGL rendering
        surfaceView = new SurfaceView(this);
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);

        setContentView(surfaceView);

        // Test JNI connection
        String message = stringFromJNI();
        Log.i(TAG, "JNI Message: " + message);
    }

    // Re-hide bars when the window gains focus (important for immersive mode)
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    // Immersive fullscreen helper
    private void hideSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(
                android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
        );
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (engineReady) {
            int action = event.getActionMasked();
            int pointerIndex = event.getActionIndex();
            int pointerId = event.getPointerId(pointerIndex);

            switch (action) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    // A finger went down
                    onTouchEventWithId(0, pointerId,
                            event.getX(pointerIndex) * touchToSurfaceScaleX,
                            event.getY(pointerIndex) * touchToSurfaceScaleY);
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    // A finger went up
                    onTouchEventWithId(1, pointerId,
                            event.getX(pointerIndex) * touchToSurfaceScaleX,
                            event.getY(pointerIndex) * touchToSurfaceScaleY);
                    break;

                case MotionEvent.ACTION_MOVE:
                    // One or more fingers moved - send all pointer positions
                    for (int i = 0; i < event.getPointerCount(); i++) {
                        int id = event.getPointerId(i);
                        onTouchEventWithId(2, id,
                                event.getX(i) * touchToSurfaceScaleX,
                                event.getY(i) * touchToSurfaceScaleY);
                    }
                    break;

                case MotionEvent.ACTION_CANCEL:
                    // All fingers cancelled - release all
                    for (int i = 0; i < event.getPointerCount(); i++) {
                        int id = event.getPointerId(i);
                        onTouchEventWithId(1, id,
                                event.getX(i) * touchToSurfaceScaleX,
                                event.getY(i) * touchToSurfaceScaleY);
                    }
                    break;

                default:
                    return super.onTouchEvent(event);
            }

            return true;
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyDown(keyCode, event);
        }
        if (engineReady) {
            onKeyEvent(keyCode, 0); // 0 = KEY_DOWN
            Log.d(TAG, "Key down: " + keyCode);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyUp(keyCode, event);
        }
        if (engineReady) {
            onKeyEvent(keyCode, 1); // 1 = KEY_UP
            Log.d(TAG, "Key up: " + keyCode);
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "Surface created");
        // Don't set surface yet - wait for surfaceChanged
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "Surface changed: " + width + "x" + height);

        int longEdge = Math.max(width, height);
        if (longEdge > MAX_SURFACE_LONG_EDGE) {
            float scale = (float) MAX_SURFACE_LONG_EDGE / (float) longEdge;
            int optimizedWidth = Math.max(2, Math.round(width * scale)) & ~1;
            int optimizedHeight = Math.max(2, Math.round(height * scale)) & ~1;
            Log.i(TAG, "Requesting optimized game surface: "
                    + optimizedWidth + "x" + optimizedHeight);
            holder.setFixedSize(optimizedWidth, optimizedHeight);
            return;
        }

        // MotionEvent coordinates use the full-screen SurfaceView dimensions,
        // while native input normalizes against the fixed-size buffer. Convert
        // into buffer pixels first so hit testing remains resolution-independent.
        int viewWidth = surfaceView.getWidth();
        int viewHeight = surfaceView.getHeight();
        touchToSurfaceScaleX = viewWidth > 0 ? (float) width / viewWidth : 1.0f;
        touchToSurfaceScaleY = viewHeight > 0 ? (float) height / viewHeight : 1.0f;

        // Tell SurfaceFlinger this is a fixed-rate 60 Hz game surface. This lets
        // high-refresh devices choose a display mode with clean 60 Hz cadence.
        holder.getSurface().setFrameRate(
                TARGET_FRAME_RATE,
                Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);

        // Initialize engine first with surface dimensions and AssetManager
        initEngine(getAssets(), getFilesDir().getAbsolutePath(), width, height);

        // Now set surface after engine is initialized
        setSurface(holder.getSurface());

        engineReady = true;

        // Resume audio here (not in onResume, where engineReady is still false)
        resumeAudio();

        // Start game loop thread
        if (gameThread == null || !gameThread.isAlive()) {
            gameThread = new GameThread();
            gameThread.start();
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "Surface destroyed");
        engineReady = false;
        
        // Stop game thread
        if (gameThread != null) {
            gameThread.stopGameThread();
            try {
                gameThread.join();
            } catch (InterruptedException e) {
                Log.e(TAG, "Error joining game thread", e);
            }
        }
        
        setSurface(null);
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "MainActivity onPause");
        if (engineReady) {
            pauseAudio();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "MainActivity onResume");
        // Audio resume moved to surfaceChanged — engineReady is still false here
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "MainActivity onDestroy");
        destroyEngine();
    }

    // Game loop thread
    private class GameThread extends Thread {
        private volatile boolean running = true;

        public void stopGameThread() {
            running = false;
            interrupt();
        }

        @Override
        public void run() {
            Log.i(TAG, "Game thread started");
            Process.setThreadPriority(Process.THREAD_PRIORITY_DISPLAY);
            long nextFrameDeadlineNs = System.nanoTime();
            PerformanceHintManager.Session performanceSession = null;

            // ADPF lets Android schedule the engine thread against the same
            // 16.67 ms budget used by the frame pacer instead of reacting only
            // after missed frames. Unsupported devices simply return no session.
            try {
                PerformanceHintManager hintManager =
                        getSystemService(PerformanceHintManager.class);
                if (hintManager != null) {
                    performanceSession = hintManager.createHintSession(
                            new int[] { Process.myTid() }, TARGET_FRAME_TIME_NS);
                }
            } catch (RuntimeException e) {
                Log.w(TAG, "Performance hint session unavailable", e);
            }

            while (running && engineReady) {
                long workStartNs = System.nanoTime();
                renderFrame();
                long workDurationNs = Math.max(1L, System.nanoTime() - workStartNs);
                if (performanceSession != null) {
                    try {
                        performanceSession.reportActualWorkDuration(workDurationNs);
                    } catch (RuntimeException e) {
                        performanceSession.close();
                        performanceSession = null;
                        Log.w(TAG, "Performance hint reporting disabled", e);
                    }
                }

                // Check if Lua called Screen.RequestClose()
                if (shouldQuit()) {
                    running = false;
                    runOnUiThread(() -> finish());
                    break;
                }

                // Pace to a 60 Hz deadline instead of sleeping 16 ms after all
                // frame work. The old loop added render time + 16 ms, which
                // turns a 16 ms frame into ~32 ms (roughly 30 FPS).
                nextFrameDeadlineNs += TARGET_FRAME_TIME_NS;
                long remainingNs = nextFrameDeadlineNs - System.nanoTime();
                while (running && remainingNs > 0L) {
                    LockSupport.parkNanos(remainingNs);
                    if (Thread.interrupted()) {
                        running = false;
                        break;
                    }
                    remainingNs = nextFrameDeadlineNs - System.nanoTime();
                }

                // Do not accumulate lateness after a slow frame.
                if (remainingNs <= 0L) {
                    nextFrameDeadlineNs = System.nanoTime();
                }
            }

            if (performanceSession != null) {
                performanceSession.close();
            }

            Log.i(TAG, "Game thread stopped");
        }
    }

    // Native methods - must match JNI function names exactly
    public native String stringFromJNI();
    public native void initEngine(android.content.res.AssetManager assetManager, String filesDir, int width, int height);
    public native void setSurface(Surface surface);
    public native void renderFrame();
    public native void destroyEngine();
    
    // Input handling native methods
    public native void onTouchEventWithId(int action, int pointerId, float x, float y);
    public native void onKeyEvent(int keyCode, int action);

    // Returns true when the engine requests a quit (e.g. Lua Screen.RequestClose())
    public native boolean shouldQuit();

    // Pause/resume audio when app goes to background/foreground
    public native void pauseAudio();
    public native void resumeAudio();
}
