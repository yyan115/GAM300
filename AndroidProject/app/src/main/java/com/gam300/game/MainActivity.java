package com.gam300.game;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.util.Log;
import android.widget.TextView;

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
        
        Log.i(TAG, "MainActivity onCreate");
        
        // Create a surface view for OpenGL rendering
        surfaceView = new SurfaceView(this);
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);
        
        setContentView(surfaceView);
        
        // Test JNI connection
        String message = stringFromJNI();
        Log.i(TAG, "JNI Message: " + message);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "Surface created");
        setSurface(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "Surface changed: " + width + "x" + height);
        
        // Initialize engine with surface dimensions
        initEngine(width, height);
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
    public native void initEngine(int width, int height);
    public native void setSurface(Surface surface);
    public native void renderFrame();
    public native void destroyEngine();
}