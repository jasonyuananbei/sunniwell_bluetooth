/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "<=== 3dd===>Bluetooth3ddProfileJni"

#define LOG_NDEBUG 0

#include "utils/Log.h"
#include <string.h>
#include "com_android_bluetooth.h"
#include "android_runtime/AndroidRuntime.h"

namespace android {
static jmethodID method_on3ddEvent;


static const bt_interface_t* btInf;
static jobject mCallbacksObj = NULL;
static JNIEnv *sCallbackEnv = NULL;

/*
*utilits function
*/
static void dumpData(jbyte* data, int len) {
       ALOGE(" the len is:%d, the data is : ", len);
       for(int i=0; i<len; i++)
              ALOGE("%2x ", data[i]);
}


static bool checkCallbackThread() {
    // Always fetch the latest callbackEnv from AdapterService.
    // Caching this could cause this sCallbackEnv to go out-of-sync
    // with the AdapterService's ENV if an ASSOCIATE/DISASSOCIATE event
    // is received
    sCallbackEnv = getCallbackEnv();

    JNIEnv* env = AndroidRuntime::getJNIEnv();
    if (sCallbackEnv != env || sCallbackEnv == NULL) return false;
    return true;
}

static void bt3dd_event_callback(uint8_t len, uint8_t* data) {
    jbyteArray evt_data;
    jbyte lenth = (jbyte)len;
    ALOGI("%s, data_len = %d", __FUNCTION__, len);

    if (!checkCallbackThread()) {                                       \
    ALOGE("Callback: '%s' is not called on the correct thread", __FUNCTION__); \
    return;                                                         \
    }

    if(sCallbackEnv == NULL) {
    ALOGI("%s, sCallbackEnv is NULL!!!!", __FUNCTION__);
    return;
    }

    evt_data = sCallbackEnv->NewByteArray(lenth);
    if (!evt_data) {
    ALOGE("Fail to new jbyteArray bt3dd_event_callback");
        checkAndClearExceptionFromCallback(sCallbackEnv, __FUNCTION__);
    return;}

    sCallbackEnv->SetByteArrayRegion(evt_data, 0, lenth, (jbyte*)data);
    ALOGI("%s, CallVoidMethod1", __FUNCTION__);
    if(mCallbacksObj == NULL)
        ALOGI("%s, mCallbacksObj is NULL", __FUNCTION__);
    if(method_on3ddEvent == NULL)
        ALOGI("%s, method_on3ddEvent is NULL", __FUNCTION__);
    if(mCallbacksObj != NULL && method_on3ddEvent != NULL)
    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_on3ddEvent, lenth,evt_data);

    ALOGI("%s, CallVoidMethod2", __FUNCTION__);
    checkAndClearExceptionFromCallback(sCallbackEnv, __FUNCTION__);
    sCallbackEnv->DeleteLocalRef(evt_data);
}

/*
*3dd callback
*/
static rtkbt_3dd_callbacks_t sBluetooth3ddCallbacks = {
    sizeof(sBluetooth3ddCallbacks),
    bt3dd_event_callback
};

/*
* native function definition
*/
static void initNative(JNIEnv *env, jobject object) {
    int status;
    ALOGI("%s", __FUNCTION__);
    /*bluetooth interface provide by bluedroid*/
    if(btInf == NULL)  {
    if ( (btInf = getBluetoothInterface()) == NULL) {
        ALOGE("Bluetooth module is not loaded");
        return;
    }
    }

#ifdef BLUETOOTH_RTK_3DD
    if ( (status = btInf->rtkbt_3dd_init(&sBluetooth3ddCallbacks)) != BT_STATUS_SUCCESS) {
    ALOGE("Failed to initialize Bluetooth3dd, status: %d", status);
    return;
    }
#endif

    ALOGE("Bluetooth init_3dd successfully");
    mCallbacksObj = env->NewGlobalRef(object);
}

static void cleanupNative(JNIEnv *env, jobject object) {
    int status;

    if (mCallbacksObj != NULL) {
        env->DeleteGlobalRef(mCallbacksObj);
        mCallbacksObj = NULL;
    }

    btInf = NULL;
}

static jboolean updateEirNative(JNIEnv *env, jobject object) {
    jbyte *addr;
    int status;

    /*santic chedk*/
    if (!btInf) return JNI_FALSE;

#ifdef BLUETOOTH_RTK_3DD
    if ((status = btInf->rtkbt_3dd_update_eir() )!= BT_STATUS_SUCCESS) {
        ALOGE("Failed HF connection, status: %d", status);
    }
#endif

    return (status == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static int send3ddCommandNative(JNIEnv *env, jobject object, jbyteArray command) {
    ALOGI("%s", __FUNCTION__);
    jbyte *data;
    uint8_t len;
    int status;
    if (!btInf) return 0;

    /*get command data and length*/
    data = env->GetByteArrayElements(command, NULL);
    if (!data) {
        jniThrowIOException(env, EINVAL);
        return JNI_FALSE;
    }
    len = env->GetArrayLength(command);
    if(len < 2)
        return 0;

    ALOGI("send3ddCommandNative, len = %d", len);

#ifdef BLUETOOTH_RTK_3DD
    if ( (status = btInf->rtkbt_3dd_send_command((uint8_t*)data, len)) != BT_STATUS_SUCCESS) {
        ALOGE("Failed send 3dd command, status: %d", status);
    }
#endif

    ALOGE("< === 3dd ===> jni : send 3dd command, status: %d", (uint8_t*)status);
    env->ReleaseByteArrayElements(command, data, 0);
    return len;
}

/**
 * JNI function definitinos
 */
static JNINativeMethod sMethods[] = {
    {"bt3ddProfileInitNative", "()V", (void *) initNative},
    {"bt3ddProfileCleanupNative", "()V", (void *) cleanupNative},
    {"bt3ddSend3ddCommandNative", "([B)I", (void *) send3ddCommandNative},
    {"bt3ddUpdateEIRNative", "()Z", (void *) updateEirNative},
};

/*
* register 3dd native method and get java methods
*/
int register_com_android_bluetooth_3dd(JNIEnv* env)
{
    jclass clazz = env->FindClass("com/android/bluetooth/rtk3dd/Sync3ddService");
    if (clazz == NULL) {
        ALOGE("Can't find com/android/bluetooth/rtk3dd/Sync3ddService");
        return -1;
    }

    method_on3ddEvent = env->GetMethodID(clazz, "on3ddEvent","(B[B)V");
    return jniRegisterNativeMethods(env, "com/android/bluetooth/rtk3dd/Sync3ddService",
                                    sMethods, NELEM(sMethods));
}
}
