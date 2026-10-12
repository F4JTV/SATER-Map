// KeepAliveService.java - keeps SATER Map running with the screen off.
//
// A foreground service in the application's own process: its notification
// is what lets Android keep the process alive (and with it the APRS-IS and
// KISS connections and the modem's audio threads) while the application is
// minimised or the phone locked. It does nothing itself. Started and
// stopped from AndroidUi.cpp as APRS is switched on and off. Taken from
// AX25Chess (and AX25Chat before it), where it was proven on a phone.
//
// The service type matters since Android 14: "microphone" keeps the
// microphone usable in the background (the built-in modem listens to the
// radio) and needs RECORD_AUDIO granted, and the application on screen,
// when the service starts; "specialUse" needs nothing; "dataSync" is for
// Android 10 to 13. When the microphone type is refused, the service runs
// without it rather than not at all. A partial wake lock keeps the CPU
// awake, a Wi-Fi lock the Wi-Fi (APRS-IS).
//
// From AX25Chess; part of SATER Map.
// SPDX-License-Identifier: GPL-2.0-or-later

package org.satermap.app;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

public class KeepAliveService extends Service {
    private static final String TAG = "SaterMap.KeepAlive";
    private static final String CHANNEL_ID = "satermap.running";
    private static final int NOTIFICATION_ID = 2;
    private static boolean running;
    private static String lastError = "";
    private static int runningTypes;
    // Starts sent and not yet through onStartCommand: a stop arriving
    // before startForeground() would have Android end the application
    // ("did not then call startForeground"), so it waits for them.
    private static int pendingStarts;
    private static boolean stopRequested;
    private PowerManager.WakeLock wakeLock;
    private WifiManager.WifiLock wifiLock;

    // ---- entry points from C++ (through the application context)

    // Start, or update the running service (its type, its texts): the
    // intent reaches onStartCommand again.
    public static synchronized void start(Context context, boolean microphone, String title, String text, String channel) {
        if (context == null) return;
        try {
            stopRequested = false;
            Intent intent = new Intent(context, KeepAliveService.class);
            intent.putExtra("microphone", microphone);
            intent.putExtra("title", title);
            intent.putExtra("text", text);
            intent.putExtra("channel", channel);
            if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(intent);
            else context.startService(intent);
            ++pendingStarts;
        } catch (Exception e) {
            Log.e(TAG, "start: " + e);
            lastError = e.toString();
        }
    }

    public static synchronized void stop(Context context) {
        if (context == null) return;
        if (pendingStarts > 0) {
            stopRequested = true;      // once startForeground() is done
            return;
        }
        try {
            context.stopService(new Intent(context, KeepAliveService.class));
        } catch (Exception e) {
            Log.e(TAG, "stop: " + e);
        }
    }

    public static synchronized boolean isRunning() {
        return running;
    }

    // The microphone type is in force (the modem can hear in the background).
    public static synchronized boolean hasMicrophone() {
        return running && Build.VERSION.SDK_INT >= 30 && (runningTypes & ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE) != 0;
    }

    // What happened at the last start, for the log.
    public static synchronized String status() {
        if (running) return "running, types " + runningTypes;
        return lastError.isEmpty() ? "not running" : "failed: " + lastError;
    }

    // ---- the service itself
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) {
            // A restart by the system after the process died: Qt is not
            // behind it (it starts from the activity), so no APRS either -
            // no notification claiming otherwise, no locks held for nothing.
            stopSelf(startId);
            return START_NOT_STICKY;
        }
        boolean microphone = intent.getBooleanExtra("microphone", false);
        String title = intent.getStringExtra("title");
        String text = intent.getStringExtra("text");
        String channel = intent.getStringExtra("channel");
        Notification notification = buildNotification(title, text, channel);
        int types = serviceTypes(microphone);
        try {
            startForegroundWith(notification, types);
        } catch (Exception e) {
            // Android 14 refuses the microphone type to an application that
            // is not on screen: run without it rather than not at all.
            Log.w(TAG, "startForeground types " + types + ": " + e);
            try {
                types = serviceTypes(false);
                startForegroundWith(notification, types);
            } catch (Exception e2) {
                Log.e(TAG, "startForeground: " + e2);
                synchronized (KeepAliveService.class) {
                    running = false;
                    lastError = e2.toString();
                    if (pendingStarts > 0) --pendingStarts;
                }
                stopSelf();
                return START_NOT_STICKY;
            }
        }
        synchronized (KeepAliveService.class) {
            if (pendingStarts > 0) --pendingStarts;
            if (stopRequested && pendingStarts == 0) {
                // stop() came while this start was on its way.
                stopRequested = false;
                running = false;
                try { stopForeground(true); } catch (Exception ignored) {}
                stopSelf();
                return START_NOT_STICKY;
            }
        }
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (pm != null && wakeLock == null) {
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SaterMap:aprs");
            wakeLock.acquire();
        }
        // Wi-Fi goes to sleep with the screen and takes the APRS-IS socket
        // with it (learned on AX25Chat): deprecated since Android 10, still
        // the only lock that does the job.
        WifiManager wm = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wm != null && wifiLock == null) {
            wifiLock = wm.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "SaterMap:aprs-is");
            wifiLock.acquire();
        }
        synchronized (KeepAliveService.class) { running = true; runningTypes = types; lastError = ""; }
        Log.i(TAG, "foreground service running, types " + types);
        // Not sticky: a service restarted alone by the system would have no
        // Qt, so no APRS, behind its notification.
        return START_NOT_STICKY;
    }

    private void startForegroundWith(Notification notification, int types) {
        if (Build.VERSION.SDK_INT >= 29 && types != 0) startForeground(NOTIFICATION_ID, notification, types);
        else startForeground(NOTIFICATION_ID, notification);
    }

    // The user swiped the application away from the recent tasks: that is
    // a quit. The service would keep the process alive (that is its job),
    // but a Qt application cannot be entered a second time in the same
    // process, so the next launch would hang on the splash screen. Stop
    // the service and end the process; the next launch starts clean.
    @Override
    public void onTaskRemoved(Intent rootIntent) {
        Log.i(TAG, "task removed: shutting the process down");
        try { stopForeground(true); } catch (Exception ignored) {}
        stopSelf();
        new android.os.Handler(android.os.Looper.getMainLooper()).postDelayed(
                () -> android.os.Process.killProcess(android.os.Process.myPid()), 300);
    }

    @Override
    public void onDestroy() {
        synchronized (KeepAliveService.class) { running = false; }
        if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
        wakeLock = null;
        if (wifiLock != null && wifiLock.isHeld()) wifiLock.release();
        wifiLock = null;
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private boolean granted(String permission) {
        return checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }

    private int serviceTypes(boolean microphone) {
        int types = 0;
        if (microphone && Build.VERSION.SDK_INT >= 30 && granted("android.permission.RECORD_AUDIO")) {
            types |= ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        }
        if (Build.VERSION.SDK_INT >= 34) {
            // "Special use", as APRSdroid declares it: Android 15 does not
            // cap it at six hours a day like dataSync.
            types |= ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE;
        } else if (types == 0 && Build.VERSION.SDK_INT >= 29) {
            types = ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC;
        }
        return types;
    }

    private Notification buildNotification(String title, String text, String channelName) {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (Build.VERSION.SDK_INT >= 26 && manager != null) {
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID,
                    channelName != null ? channelName : "APRS in the background", NotificationManager.IMPORTANCE_LOW);
            manager.createNotificationChannel(channel);
        }
        int icon = getResources().getIdentifier("icon", "drawable", getPackageName());
        if (icon == 0) icon = android.R.drawable.ic_dialog_info;
        Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
        PendingIntent open = null;
        if (launch != null) {
            launch.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            open = PendingIntent.getActivity(this, 1, launch, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        }
        Notification.Builder builder = Build.VERSION.SDK_INT >= 26
                ? new Notification.Builder(this, CHANNEL_ID)
                : new Notification.Builder(this);
        builder.setSmallIcon(icon)
               .setContentTitle(title != null ? title : "SATER Map")
               .setContentText(text != null ? text : "")
               .setOngoing(true)
               .setOnlyAlertOnce(true);
        if (open != null) builder.setContentIntent(open);
        return builder.build();
    }
}
