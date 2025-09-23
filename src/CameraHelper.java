package org.wysaid.ccap;

import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.camera2.*;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Helper class to interface with Android Camera2 API from native code
 */
public class CameraHelper {
    private static final String TAG = "CameraHelper";
    
    private final long mNativePtr;
    private Context mContext;
    private CameraManager mCameraManager;
    private CameraDevice mCameraDevice;
    private CameraCaptureSession mCaptureSession;
    private ImageReader mImageReader;
    private HandlerThread mBackgroundThread;
    private Handler mBackgroundHandler;
    private final Semaphore mCameraOpenCloseLock = new Semaphore(1);
    
    // Current configuration
    private String mCurrentCameraId;
    private int mImageWidth = 640;
    private int mImageHeight = 480;
    private int mImageFormat = ImageFormat.YUV_420_888;
    
    /**
     * Constructor called from native code
     * @param nativePtr Pointer to native ProviderAndroid instance
     */
    public CameraHelper(long nativePtr) {
        mNativePtr = nativePtr;
    }
    
    /**
     * Initialize with Android context
     */
    public void initialize(Context context) {
        mContext = context;
        mCameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        startBackgroundThread();
    }
    
    /**
     * Get list of available camera IDs
     */
    public String[] getCameraList() {
        try {
            return mCameraManager.getCameraIdList();
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to get camera list", e);
            return new String[0];
        }
    }
    
    /**
     * Get supported sizes for a camera
     * @param cameraId Camera ID to query
     * @return Array of [width, height] pairs
     */
    public int[] getSupportedSizes(String cameraId) {
        try {
            CameraCharacteristics characteristics = mCameraManager.getCameraCharacteristics(cameraId);
            StreamConfigurationMap map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            
            if (map == null) {
                return new int[0];
            }
            
            Size[] sizes = map.getOutputSizes(ImageFormat.YUV_420_888);
            if (sizes == null) {
                return new int[0];
            }
            
            List<Integer> sizeList = new ArrayList<>();
            for (Size size : sizes) {
                sizeList.add(size.getWidth());
                sizeList.add(size.getHeight());
            }
            
            return sizeList.stream().mapToInt(i -> i).toArray();
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to get supported sizes for camera " + cameraId, e);
            return new int[0];
        }
    }
    
    /**
     * Open camera device
     */
    public boolean openCamera(String cameraId) {
        mCurrentCameraId = cameraId;
        
        try {
            if (!mCameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS)) {
                throw new RuntimeException("Time out waiting to lock camera opening.");
            }
            
            mCameraManager.openCamera(cameraId, mStateCallback, mBackgroundHandler);
            return true;
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to open camera " + cameraId, e);
            return false;
        } catch (InterruptedException e) {
            throw new RuntimeException("Interrupted while trying to lock camera opening.", e);
        }
    }
    
    /**
     * Close camera device
     */
    public void closeCamera() {
        try {
            mCameraOpenCloseLock.acquire();
            if (null != mCaptureSession) {
                mCaptureSession.close();
                mCaptureSession = null;
            }
            if (null != mCameraDevice) {
                mCameraDevice.close();
                mCameraDevice = null;
            }
            if (null != mImageReader) {
                mImageReader.close();
                mImageReader = null;
            }
        } catch (InterruptedException e) {
            throw new RuntimeException("Interrupted while trying to lock camera closing.", e);
        } finally {
            mCameraOpenCloseLock.release();
        }
    }
    
    /**
     * Start capture session
     */
    public boolean startCapture() {
        if (mCameraDevice == null) {
            Log.e(TAG, "Camera device is null");
            return false;
        }
        
        try {
            // Setup ImageReader for capturing frames
            mImageReader = ImageReader.newInstance(mImageWidth, mImageHeight, mImageFormat, 1);
            mImageReader.setOnImageAvailableListener(mOnImageAvailableListener, mBackgroundHandler);
            
            // Create capture session
            List<Surface> surfaces = Arrays.asList(mImageReader.getSurface());
            mCameraDevice.createCaptureSession(surfaces, new CameraCaptureSession.StateCallback() {
                @Override
                public void onConfigured(CameraCaptureSession cameraCaptureSession) {
                    if (null == mCameraDevice) {
                        return;
                    }
                    
                    mCaptureSession = cameraCaptureSession;
                    try {
                        // Create capture request
                        CaptureRequest.Builder captureRequestBuilder = 
                            mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                        captureRequestBuilder.addTarget(mImageReader.getSurface());
                        captureRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE,
                            CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
                        
                        // Start repeating capture
                        mCaptureSession.setRepeatingRequest(
                            captureRequestBuilder.build(), mCaptureCallback, mBackgroundHandler);
                            
                    } catch (CameraAccessException e) {
                        Log.e(TAG, "Failed to start capture", e);
                    }
                }
                
                @Override
                public void onConfigureFailed(CameraCaptureSession cameraCaptureSession) {
                    Log.e(TAG, "Camera capture session configuration failed");
                }
            }, null);
            
            return true;
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to start capture", e);
            return false;
        }
    }
    
    /**
     * Stop capture session
     */
    public void stopCapture() {
        try {
            if (mCaptureSession != null) {
                mCaptureSession.stopRepeating();
                mCaptureSession.abortCaptures();
            }
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to stop capture", e);
        }
    }
    
    /**
     * Release resources
     */
    public void release() {
        closeCamera();
        stopBackgroundThread();
    }
    
    private void startBackgroundThread() {
        mBackgroundThread = new HandlerThread("CameraBackground");
        mBackgroundThread.start();
        mBackgroundHandler = new Handler(mBackgroundThread.getLooper());
    }
    
    private void stopBackgroundThread() {
        if (mBackgroundThread != null) {
            mBackgroundThread.quitSafely();
            try {
                mBackgroundThread.join();
                mBackgroundThread = null;
                mBackgroundHandler = null;
            } catch (InterruptedException e) {
                Log.e(TAG, "Interrupted while stopping background thread", e);
            }
        }
    }
    
    private final CameraDevice.StateCallback mStateCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(CameraDevice cameraDevice) {
            mCameraOpenCloseLock.release();
            mCameraDevice = cameraDevice;
            Log.d(TAG, "Camera opened: " + mCurrentCameraId);
        }
        
        @Override
        public void onDisconnected(CameraDevice cameraDevice) {
            mCameraOpenCloseLock.release();
            cameraDevice.close();
            mCameraDevice = null;
            Log.w(TAG, "Camera disconnected: " + mCurrentCameraId);
            nativeOnCameraDisconnected(mNativePtr);
        }
        
        @Override
        public void onError(CameraDevice cameraDevice, int error) {
            mCameraOpenCloseLock.release();
            cameraDevice.close();
            mCameraDevice = null;
            Log.e(TAG, "Camera error: " + error + " for camera: " + mCurrentCameraId);
            nativeOnCameraError(mNativePtr, error);
        }
    };
    
    private final ImageReader.OnImageAvailableListener mOnImageAvailableListener = 
        new ImageReader.OnImageAvailableListener() {
        @Override
        public void onImageAvailable(ImageReader reader) {
            Image image = reader.acquireLatestImage();
            if (image != null) {
                // Notify native code
                nativeOnImageAvailable(mNativePtr, image);
                image.close();
            }
        }
    };
    
    private final CameraCaptureSession.CaptureCallback mCaptureCallback = 
        new CameraCaptureSession.CaptureCallback() {
        @Override
        public void onCaptureCompleted(CameraCaptureSession session,
                                     CaptureRequest request,
                                     TotalCaptureResult result) {
            // Handle capture completion if needed
        }
    };
    
    // Native method declarations
    private native void nativeOnImageAvailable(long nativePtr, Image image);
    private native void nativeOnCameraDisconnected(long nativePtr);
    private native void nativeOnCameraError(long nativePtr, int error);
}