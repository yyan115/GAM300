package com.gam300.game;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.util.Log;
import android.widget.TextView;
import org.fmod.FMOD;

public class MainActivity extends AppCompatActivity implements SurfaceHolder.Callback {

    // Used to load the 'gam300android' library on application startup.
    static {
        System.loadLibrary("gam300android");
    }

    private static final String TAG = "GAM300";
    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    private GameThread gameThread;
    private boolean engineReady = false;

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
                    onTouchEventWithId(0, pointerId, event.getX(pointerIndex), event.getY(pointerIndex));
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    // A finger went up
                    onTouchEventWithId(1, pointerId, event.getX(pointerIndex), event.getY(pointerIndex));
                    break;

                case MotionEvent.ACTION_MOVE:
                    // One or more fingers moved - send all pointer positions
                    for (int i = 0; i < event.getPointerCount(); i++) {
                        int id = event.getPointerId(i);
                        onTouchEventWithId(2, id, event.getX(i), event.getY(i));
                    }
                    break;

                case MotionEvent.ACTION_CANCEL:
                    // All fingers cancelled - release all
                    for (int i = 0; i < event.getPointerCount(); i++) {
                        int id = event.getPointerId(i);
                        onTouchEventWithId(1, id, event.getX(i), event.getY(i));
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
        if (engineReady) {
            onKeyEvent(keyCode, 0); // 0 = KEY_DOWN
            Log.d(TAG, "Key down: " + keyCode);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
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

        // Initialize engine first with surface dimensions and AssetManager
        initEngine(getAssets(), width, height);

        // Now set surface after engine is initialized
        setSurface(holder.getSurface());

        engineReady = true;

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
        }
        
        @Override
        public void run() {
            Log.i(TAG, "Game thread started");
            
            while (running && engineReady) {
                renderFrame();
                
                try {
                    Thread.sleep(16); // ~60 FPS
                } catch (InterruptedException e) {
                    break;
                }
            }
            
            Log.i(TAG, "Game thread stopped");
        }
    }

    // Native methods - must match JNI function names exactly
    public native String stringFromJNI();
    public native void initEngine(android.content.res.AssetManager assetManager, int width, int height);
    public native void setSurface(Surface surface);
    public native void renderFrame();
    public native void destroyEngine();
    
    // Input handling native methods
    public native void onTouchEventWithId(int action, int pointerId, float x, float y);
    public native void onKeyEvent(int keyCode, int action);
}