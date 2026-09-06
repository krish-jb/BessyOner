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

void create_config_file(JNIEnv *env, jobject thiz, AppConfig *config);

JNIEXPORT jint JNICALL
Java_com_bessy_thebeast_MainActivity_nativeSaveConfigAndSetupKey(
        JNIEnv *env,
        jobject thiz,
        jstring target_ip,
        jstring mac_adder,
        jstring broadcast_ip,
        jstring username,
        jstring password,
        jstring key_path,
        jstring config_path) {
    const char *c_ip = (*env)->GetStringUTFChars(env, target_ip, NULL);
    const char *c_mac_adder = (*env)->GetStringUTFChars(env, mac_adder, NULL);
    const char *c_broadcast_ip = (*env)->GetStringUTFChars(env, broadcast_ip, NULL);
    const char *c_username = (*env)->GetStringUTFChars(env, username, NULL);
    const char *c_password = (*env)->GetStringUTFChars(env, password, NULL);
    const char *c_key_path = (*env)->GetStringUTFChars(env, key_path, NULL);

    char public_key_path[512];
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


    const char *path = (*env)->GetStringUTFChars(env, config_path, NULL);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(&config, sizeof(AppConfig), 1, f);
        fclose(f);
    }

    (*env)->ReleaseStringUTFChars(env, target_ip, c_ip);
    (*env)->ReleaseStringUTFChars(env, config_path, path);
    (*env)->ReleaseStringUTFChars(env, username, c_username);
    (*env)->ReleaseStringUTFChars(env, password, c_password);
    (*env)->ReleaseStringUTFChars(env, key_path, c_key_path);
    (*env)->ReleaseStringUTFChars(env, mac_adder, c_mac_adder);
    (*env)->ReleaseStringUTFChars(env, broadcast_ip, c_broadcast_ip);

    return 0;
}

JNIEXPORT jobjectArray JNICALL
Java_com_bessy_thebeast_MainActivity_nativeLoadConfig(
        JNIEnv *env,
        jobject thiz,
        jstring config_path) {
    const char *path = (*env)->GetStringUTFChars(env, config_path, NULL);
    FILE *f = fopen(path, "rb");
    (*env)->ReleaseStringUTFChars(env, config_path, path);

    if (!f) return NULL;

    AppConfig config;
    if (fread(&config, sizeof(AppConfig), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    fclose(f);

    jclass stringClass = (*env)->FindClass(env, "java/lang/String");
    jobjectArray result = (*env)->NewObjectArray(env, 4, stringClass, NULL);

    if (!result) {
        (*env)->DeleteLocalRef(env, stringClass);
        return NULL;
    }

    jstring str_ip = (*env)->NewStringUTF(env, config.ip);
    jstring str_mac_adder = (*env)->NewStringUTF(env, config.mac);
    jstring str_username = (*env)->NewStringUTF(env, config.username);
    jstring str_broadcast_ip = (*env)->NewStringUTF(env, config.broadcast_ip);


    (*env)->SetObjectArrayElement(env, result, 0, str_ip);
    (*env)->SetObjectArrayElement(env, result, 1, str_mac_adder);
    (*env)->SetObjectArrayElement(env, result, 2, str_broadcast_ip);
    (*env)->SetObjectArrayElement(env, result, 3, str_username);

    (*env)->DeleteLocalRef(env, stringClass);
    (*env)->DeleteLocalRef(env, str_ip);
    (*env)->DeleteLocalRef(env, str_mac_adder);
    (*env)->DeleteLocalRef(env, str_broadcast_ip);
    (*env)->DeleteLocalRef(env, str_username);

    return result;
}

JNIEXPORT jint JNICALL
Java_com_bessy_thebeast_WolTileService_nativeExecuteWol(
        JNIEnv *env,
        jobject thiz,
        jstring config_path) {
    const char *path = (*env)->GetStringUTFChars(env, config_path, NULL);

    FILE *f = fopen(path, "rb");
    (*env)->ReleaseStringUTFChars(env, config_path, path);

    if (!f) return -1;

    AppConfig config;
    if (fread(&config, sizeof(AppConfig), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);

    return send_wol_native(config.broadcast_ip, config.mac);
}

JNIEXPORT jint JNICALL
Java_com_bessy_thebeast_WolTileService_nativeExecuteShutdown(
        JNIEnv *env,
        jobject thiz,
        jstring config_path) {
    const char *path = (*env)->GetStringUTFChars(env, config_path, NULL);

    FILE *f = fopen(path, "rb");


    if (!f) {
        (*env)->ReleaseStringUTFChars(env, config_path, path);
        return -1;
    }

    AppConfig config;
    if (fread(&config, sizeof(AppConfig), 1, f) != 1) {
        fclose(f);
        (*env)->ReleaseStringUTFChars(env, config_path, path);
        return -1;
    }
    fclose(f);

    char key_path[512];
    snprintf(key_path, sizeof(key_path), "%s/id_ed25519", path);
    (*env)->ReleaseStringUTFChars(env, config_path, path);

    return send_ssh_shutdown_native(config.ip, config.username, key_path);
}