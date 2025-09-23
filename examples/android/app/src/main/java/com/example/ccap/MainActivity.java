package com.example.ccap;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.method.ScrollingMovementMethod;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class MainActivity extends AppCompatActivity {

    private static final int CAMERA_PERMISSION_REQUEST_CODE = 100;
    
    private TextView statusText;
    private TextView frameCountText;
    private TextView logText;
    private Spinner cameraSpinner;
    private Button openCameraButton;
    private Button startCaptureButton;
    private Button stopCaptureButton;
    private Button closeCameraButton;
    
    private Handler uiHandler;
    private StringBuilder logBuffer;
    
    // Load native library
    static {
        System.loadLibrary("ccap_android_demo");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        initViews();
        
        // Initialize CCAP native library
        if (!nativeInitialize()) {
            addLog("Failed to initialize CCAP library");
        } else {
            addLog("CCAP library initialized successfully");
        }
        
        uiHandler = new Handler(Looper.getMainLooper());
        logBuffer = new StringBuilder();
        
        // Check and request camera permission
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) 
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, 
                new String[]{Manifest.permission.CAMERA}, 
                CAMERA_PERMISSION_REQUEST_CODE);
        } else {
            initCamera();
        }
    }
    
    private void initViews() {
        statusText = findViewById(R.id.statusText);
        frameCountText = findViewById(R.id.frameCountText);
        logText = findViewById(R.id.logText);
        cameraSpinner = findViewById(R.id.cameraSpinner);
        openCameraButton = findViewById(R.id.openCameraButton);
        startCaptureButton = findViewById(R.id.startCaptureButton);
        stopCaptureButton = findViewById(R.id.stopCaptureButton);
        closeCameraButton = findViewById(R.id.closeCameraButton);
        
        // Make log text scrollable
        logText.setMovementMethod(new ScrollingMovementMethod());
        
        // Set button click listeners
        openCameraButton.setOnClickListener(v -> openCamera());
        startCaptureButton.setOnClickListener(v -> startCapture());
        stopCaptureButton.setOnClickListener(v -> stopCapture());
        closeCameraButton.setOnClickListener(v -> closeCamera());
    }
    
    private void initCamera() {
        addLog("Initializing camera system...");
        
        boolean success = nativeInitialize();
        if (success) {
            addLog("Camera system initialized successfully");
            loadCameraList();
        } else {
            addLog("Failed to initialize camera system");
            updateStatus("Initialization Failed");
        }
    }
    
    private void loadCameraList() {
        String[] cameras = nativeGetCameraList();
        if (cameras != null && cameras.length > 0) {
            ArrayAdapter<String> adapter = new ArrayAdapter<>(this, 
                android.R.layout.simple_spinner_item, cameras);
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            cameraSpinner.setAdapter(adapter);
            addLog("Found " + cameras.length + " camera(s)");
            updateStatus("Ready");
        } else {
            addLog("No cameras found");
            updateStatus("No Cameras");
        }
    }
    
    private void openCamera() {
        if (cameraSpinner.getSelectedItem() == null) {
            Toast.makeText(this, R.string.select_camera, Toast.LENGTH_SHORT).show();
            return;
        }
        
        String cameraId = cameraSpinner.getSelectedItem().toString();
        addLog("Opening camera: " + cameraId);
        
        boolean success = nativeOpenCamera(cameraId);
        if (success) {
            addLog("Camera opened successfully");
            updateStatus("Camera Opened");
            openCameraButton.setEnabled(false);
            startCaptureButton.setEnabled(true);
            closeCameraButton.setEnabled(true);
        } else {
            addLog("Failed to open camera");
            updateStatus("Open Failed");
        }
    }
    
    private void startCapture() {
        addLog("Starting capture...");
        
        boolean success = nativeStartCapture();
        if (success) {
            addLog("Capture started");
            updateStatus("Capturing");
            startCaptureButton.setEnabled(false);
            stopCaptureButton.setEnabled(true);
            
            // Start frame count updates
            startFrameCountUpdates();
        } else {
            addLog("Failed to start capture");
            updateStatus("Capture Failed");
        }
    }
    
    private void stopCapture() {
        addLog("Stopping capture...");
        
        nativeStopCapture();
        addLog("Capture stopped");
        updateStatus("Camera Opened");
        startCaptureButton.setEnabled(true);
        stopCaptureButton.setEnabled(false);
    }
    
    private void closeCamera() {
        addLog("Closing camera...");
        
        nativeCloseCamera();
        addLog("Camera closed");
        updateStatus("Ready");
        openCameraButton.setEnabled(true);
        startCaptureButton.setEnabled(false);
        stopCaptureButton.setEnabled(false);
        closeCameraButton.setEnabled(false);
    }
    
    private void startFrameCountUpdates() {
        uiHandler.post(new Runnable() {
            @Override
            public void run() {
                if (nativeIsCapturing()) {
                    long frameCount = nativeGetFrameCount();
                    frameCountText.setText(getString(R.string.frame_count, frameCount));
                    uiHandler.postDelayed(this, 100); // Update every 100ms
                }
            }
        });
    }
    
    private void updateStatus(String status) {
        uiHandler.post(() -> statusText.setText(getString(R.string.camera_status, status)));
    }
    
    private void addLog(String message) {
        uiHandler.post(() -> {
            logBuffer.append(message).append("\n");
            logText.setText(logBuffer.toString());
            
            // Auto scroll to bottom
            final int scrollAmount = logText.getLayout() != null ? 
                logText.getLayout().getLineTop(logText.getLineCount()) - logText.getHeight() : 0;
            if (scrollAmount > 0) {
                logText.scrollTo(0, scrollAmount);
            } else {
                logText.scrollTo(0, 0);
            }
        });
    }
    
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, 
                                         @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        
        if (requestCode == CAMERA_PERMISSION_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                initCamera();
            } else {
                Toast.makeText(this, R.string.camera_permission_required, Toast.LENGTH_LONG).show();
                updateStatus("Permission Denied");
            }
        }
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        nativeCleanup();
    }
    
    // Native methods
    public native boolean nativeInitialize();
    public native String[] nativeGetCameraList();
    public native boolean nativeOpenCamera(String cameraId);
    public native boolean nativeStartCapture();
    public native void nativeStopCapture();
    public native void nativeCloseCamera();
    public native long nativeGetFrameCount();
    public native boolean nativeIsCapturing();
    public native void nativeCleanup();
}