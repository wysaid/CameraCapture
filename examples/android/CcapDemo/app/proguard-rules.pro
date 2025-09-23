# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# proguardFiles setting in build.gradle.

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep CCAP related classes
-keep class com.example.ccap.** { *; }

# Standard Android rules
-dontwarn javax.annotation.**
-keep class android.support.** { *; }
-keep class androidx.** { *; }