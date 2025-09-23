# Android Application Configuration Examples

## app/build.gradle

```gradle
android {
    compileSdk 34

    defaultConfig {
        minSdk 21  // Minimum for Camera2 API
        targetSdk 34
        
        ndk {
            abiFilters 'arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64'
        }
        
        externalNativeBuild {
            cmake {
                cppFlags "-std=c++17"
                arguments "-DANDROID_STL=c++_shared"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path file('src/main/cpp/CMakeLists.txt')
            version '3.18.1'
        }
    }
}

dependencies {
    implementation 'androidx.camera:camera-core:1.3.0'
    implementation 'androidx.camera:camera-camera2:1.3.0'
    implementation 'androidx.camera:camera-lifecycle:1.3.0'
}
```

## app/src/main/AndroidManifest.xml

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    
    <!-- Camera permissions -->
    <uses-permission android:name="android.permission.CAMERA" />
    <uses-feature android:name="android.hardware.camera" android:required="true" />
    <uses-feature android:name="android.hardware.camera2.full" android:required="false" />
    
    <application
        android:name=".CcapApplication"
        android:label="CCAP Camera App">
        
        <activity android:name=".MainActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
        
    </application>
</manifest>
```

## Usage in Android Application

```java
// CcapApplication.java
public class CcapApplication extends Application {
    static {
        System.loadLibrary("ccap_android");
    }
    
    @Override
    public void onCreate() {
        super.onCreate();
        // Initialize CCAP with application context
        CameraHelper.initializeGlobal(this);
    }
}

// MainActivity.java
public class MainActivity extends AppCompatActivity {
    private CameraHelper mCameraHelper;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Check camera permission
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) 
            != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, 
                new String[]{Manifest.permission.CAMERA}, 100);
            return;
        }
        
        // Initialize camera helper
        mCameraHelper = new CameraHelper(0); // Native pointer will be set by native code
        mCameraHelper.initialize(this);
        
        // Use native CCAP API
        startCameraCapture();
    }
    
    private native void startCameraCapture();
}
```

## Native Integration

```cpp
// Native method implementation
extern "C" JNIEXPORT void JNICALL
Java_com_example_MainActivity_startCameraCapture(JNIEnv *env, jobject thiz) {
    // Create CCAP provider (will automatically use Android backend)
    ccap::Provider provider;
    
    // Find cameras
    auto cameras = provider.findDeviceNames();
    if (!cameras.empty()) {
        // Open first camera
        provider.open(cameras[0]);
        provider.start();
        
        // Set frame callback
        provider.setNewFrameCallback([](const std::shared_ptr<ccap::VideoFrame>& frame) {
            // Process frame data
            CCAP_LOG_I("Received frame: %dx%d", frame->width, frame->height);
            return true;
        });
    }
}
```