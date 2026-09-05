#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "WOL_Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern int send_wol_native(const char *broadcast_ip, const char *mac_address);
extern int send_ssh_shutdown_native(const char *ip, const char *username, const char *private_key);
extern int generate_ed25519_keypair(const char *key_path);
extern int setup_remote_public_key(
        const char *ip,
        const char *user,
        const char *password,
        const char *public_key_path
);

typedef struct {
    char ip[16];
    char mac[18];
    char broadcast_ip[16];
    char username[32];
} AppConfig;

JNIEXPORT jint JNICALL
Java_com_bessy_thebeast_MainActivity_nativeSaveConfigAndSetupKey(
        JNIEnv *env,
        jobject thiz,
        jstring target_ip,
        jstring mac_adder,
        jstring broadcast_ip,
        jstring username,
        jstring password,
        jstring key_path) {
    const char *c_ip = (*env)->GetStringUTFChars(env, target_ip, NULL);
    const char *c_mac_adder = (*env)->GetStringUTFChars(env, mac_adder, NULL);
    const char *c_broadcast_ip = (*env)->GetStringUTFChars(env, broadcast_ip, NULL);
    const char *c_username = (*env)->GetStringUTFChars(env, username, NULL);
    const char *c_password = (*env)->GetStringUTFChars(env, password, NULL);
    const char *c_key_path = (*env)->GetStringUTFChars(env, key_path, NULL);

    char public_key_path[140];
    snprintf(public_key_path, sizeof(public_key_path), "%s.pub", c_key_path);
    generate_ed25519_keypair(c_key_path);

    if (strlen(c_password) > 0) {
        if (setup_remote_public_key(
                c_ip, c_username, c_password, public_key_path) != 0) {
            LOGI("Failed to append public key to server");
        }
    }

    AppConfig config;
    memset(&config, 0, sizeof(AppConfig));
    strncpy(config.ip, c_ip, sizeof(config.ip) - 1);
    strncpy(config.mac, c_mac_adder, sizeof(config.mac) - 1);
    strncpy(config.username, c_username, sizeof(config.username) - 1);
    strncpy(config.broadcast_ip, c_broadcast_ip, sizeof(config.broadcast_ip) - 1);

    jclass contextClass = (*env)->GetObjectClass(env, thiz);

    jmethodID getFilesDirMethod = (*env)->GetMethodID(
            env, contextClass, "getFilesDir", "()Ljava/io/File;");
    jobject fileObj = (*env)->CallObjectMethod(env, thiz, getFilesDirMethod);

    jclass fileClass = (*env)->GetObjectClass(env, fileObj);
    jmethodID getAbsolutePathMethod = (*env)->GetMethodID(
            env, fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jstring pathJString = (jstring)(*env)->CallObjectMethod(env, fileObj, getAbsolutePathMethod);

    const char *dir_path = (*env)->GetStringUTFChars(env, pathJString, NULL);

    char full_config_path[512];
    snprintf(
            full_config_path, sizeof(full_config_path), "%s/config.bin", dir_path);

    FILE *f = fopen(full_config_path, "wb");
    if (f) {
        fwrite(&config, sizeof(AppConfig), 1, f);
        fclose(f);
    }

    (*env)->ReleaseStringUTFChars(env, pathJString, dir_path);
    (*env)->DeleteLocalRef(env, pathJString);
    (*env)->DeleteLocalRef(env, fileClass);
    (*env)->DeleteLocalRef(env, fileObj);
    (*env)->DeleteLocalRef(env, contextClass);

    (*env)->ReleaseStringUTFChars(env, target_ip, c_ip);
    (*env)->ReleaseStringUTFChars(env, username, c_username);
    (*env)->ReleaseStringUTFChars(env, password, c_password);
    (*env)->ReleaseStringUTFChars(env, key_path, c_key_path);
    (*env)->ReleaseStringUTFChars(env, mac_adder, c_mac_adder);
    (*env)->ReleaseStringUTFChars(env, broadcast_ip, c_broadcast_ip);

    return 0;
}