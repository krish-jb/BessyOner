-repackageclasses ''
-allowaccessmodification
-dontusemixedcaseclassnames
-dontnote **
-dontwarn **
-ignorewarnings

# Preserve JNI function linkages
-keepclasseswithmembernames class * {
    native <methods>;
}

# Preserve Activity and Service entry points
-keep public class com.bessy.thebeast.MainActivity
-keep public class com.bessy.thebeast.WolTileService