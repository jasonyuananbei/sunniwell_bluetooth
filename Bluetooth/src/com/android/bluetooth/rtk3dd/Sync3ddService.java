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
 * limitations under the License.*/


package com.android.bluetooth.rtk3dd;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetooth;
import android.bluetooth.IBluetooth3ddDevice;
import com.android.bluetooth.Utils;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.provider.Settings;
import android.util.Log;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.Utils;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import android.content.pm.PackageManager;

/**
 * Provides Bluetooth Gatt Host profile, as a service in
 * the Bluetooth application.
 */
public class Sync3ddService extends ProfileService {
    private static final boolean DBG = true;
    private static final String TAG = "sync3ddService";

    private boolean BLUETOOT_RTK_3DD_flag = false;
    private static Sync3ddService mSync3ddService;
    private AdapterService mAdapterService;
    public static final String BLUETOOTH_3DD_PROFILE = "bluetooth_3dd_profile";
    public static final String EVENT_DATA = "com.android.bluetooth.btservice.Bluetooth3ddProfile.action.EVENT_DATA";
    public static final String ACTION_3DD_EVENT =
            "com.android.bluetooth.btservice.Bluetooth3ddProfile.action.3DD_EVENT";

    public String getName() {
        return TAG;
    }

    public IProfileServiceBinder initBinder() {
        if (DBG) log("initBinder()");
            return new Bluetooth3ddDeviceBinder(this);
    }

    protected boolean start() {
     if (DBG) log("start Bluetooth 3dd Service");
         setSync3ddService(this);
     mAdapterService = AdapterService.getAdapterService();
     BLUETOOT_RTK_3DD_flag = this.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_RTK_3DD);
     if(BLUETOOT_RTK_3DD_flag)
         bt3ddProfileInitNative();
         return true;
    }

    protected boolean stop() {
        if (DBG) log("Stopping Bluetooth 3dd Service");
            return true;
    }

    protected boolean cleanup() {
     log("cleanup Bluetooth 3dd Service");
     if(BLUETOOT_RTK_3DD_flag)
         bt3ddProfileCleanupNative();
         clearSync3ddService();
         return true;
    }

    public static synchronized Sync3ddService getSync3ddService() {
        if(mSync3ddService != null && mSync3ddService.isAvailable()) {
            if (DBG) Log.d(TAG, "getSync3ddService(): returning " + mSync3ddService);
            return mSync3ddService;
        }
        if(DBG){
            if(mSync3ddService == null) {
                Log.d(TAG, "getSync3ddService(): service is NULL");
            } else if (!(mSync3ddService.isAvailable())) {
                Log.d(TAG,"getSync3ddService(): service is not available");
            }
        }
        return null;
    }

    private static synchronized void setSync3ddService(Sync3ddService instance) {
        if (instance != null && instance.isAvailable()) {
            if (DBG) Log.d(TAG, "setSync3ddService(): set to: " + instance);
            mSync3ddService = instance;
        } else {
            if (DBG){
                if(mSync3ddService == null) {
                    Log.d(TAG, "setSync3ddService(): service not available");
                } else if (!mSync3ddService.isAvailable()) {
                    Log.d(TAG,"setSync3ddService(): service is cleaning up");
                }
            }
        }
    }

    private static synchronized void clearSync3ddService() {
        mSync3ddService = null;
    }


    /**
     * Handlers for incoming service calls
     */
    private static class Bluetooth3ddDeviceBinder extends
IBluetooth3ddDevice.Stub implements IProfileServiceBinder{

        private Sync3ddService mService;
        public Bluetooth3ddDeviceBinder(Sync3ddService svc) {
            mService = svc;
        }

        public boolean cleanup() {
            mService = null;
            return true;
        }

        private Sync3ddService getService() {
            if (!Utils.checkCaller()) {
                Log.w(TAG,"InputDevice call not allowed for non-active user");
                return null;
            }

            if (mService  != null && mService.isAvailable()) {
                return mService;
            }
            return getSync3ddService();
        }

        public void set3DScanFlag(boolean mode) {
            Sync3ddService service = getService();
            if (service == null) return ;
            service.set3DScanFlag(mode);
        }

        public boolean updateEIR() {
            Sync3ddService service = getService();
            if (service == null) return false;
            return service.updateEIR();
        }

    public void startScanning() {
            Sync3ddService service = getService();
            if (service == null) return ;
            service.startScanning();
        }

    public void stopScanning() {
            Sync3ddService service = getService();
            if (service == null) return ;
            service.stopScanning();
        }

        public boolean isBREDRScanning() {
            Sync3ddService service = getService();
            if (service == null) return false;
            return service.isBREDRScanning();
        }

        public boolean writeData(int len, byte[] data ) {
            Log.w(TAG, "writeData");
            Sync3ddService service = getService();
            boolean result = false;
            if (service == null) {
            Log.w(TAG, "service is null");
            service = getSync3ddService();//return false;
        }

        if(service == null) {
            Log.w(TAG, "service is still null, return false");
        }
        try{
            result =  service.writeData(len, data);
        } catch(RemoteException e) {
            Log.w(TAG, "write data catch remoteexception!");
        }
        return result;
        }
    };

    //APIs
    public boolean writeData(int len, byte[] data) throws RemoteException {
        log("<===3dd===>writeData, data len is " + len);
        if(BLUETOOT_RTK_3DD_flag)
            return len == bt3ddSend3ddCommandNative(data);
        else
            return false;
    }

    public boolean updateEIR() {
        log("<===3dd===>updateEIR");
        if(BLUETOOT_RTK_3DD_flag)
            return bt3ddUpdateEIRNative();
        else
            return false;
    }

    public boolean isBREDRScanning() {
        log("<===3dd===>isBREDRScanning");
        //return mAdapterService.isBREDRScanning();
        return false;
    }

    public void set3DScanFlag(boolean mode) {
        log("<===3dd===>set3DScanMode");
        //mAdapterService.set3DScanMode(mode);
    }

    public void startScanning() {
        log("<===3dd===>startScanning");
        if(mAdapterService != null)
            mAdapterService.rtkbt3ddStartScanning();
        else
            log("<===3dd===>stopScanning, mAdapterService == null");
        //mAdapterService.setScanMode(1, -1);
    }

    public void stopScanning() {
        log("<===3dd===>stopScanning");
        if(mAdapterService != null)
            mAdapterService.rtkbt3ddStopScanning();
        else
            log("<===3dd===>stopScanning, mAdapterService == null");
        //mAdapterService.setScanMode(2, -1);
    }

    public void on3ddEvent(byte len, byte[] data) {
        log("<===3dd===>on3ddEvent");

        if(data != null && len == data.length)
        {
            for(int i=0; i< len; i++)
                log("data " + data[i]);

            Intent intent = new Intent(ACTION_3DD_EVENT);
            intent.putExtra(EVENT_DATA, data);
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
            sendBroadcast(intent, BLUETOOTH_PERM);
        }
    }

    public void log(String message){
        if(DBG) Log.d(TAG, message);
    }

    //private native static void bt3ddClassInitNative();
    private native void bt3ddProfileInitNative();
    private native void bt3ddProfileCleanupNative();
    private native int bt3ddSend3ddCommandNative(byte[] data);
    private native boolean bt3ddUpdateEIRNative();
}


