# Hilt/Dagger: keep @Inject constructors and fields (consumer rules cover the rest)
-keepclassmembers class * {
    @javax.inject.Inject <init>(...);
}
-keepclassmembers class * {
    @javax.inject.Inject <fields>;
}

# Nordic BLE: BleManager reaches BluetoothGatt internals via reflection
-keep class no.nordicsemi.android.ble.** { *; }
-dontwarn no.nordicsemi.android.ble.**

# Readable crash stack traces via mapping.txt
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile
