// DeviceServices.java - what SATER Map asks of Android beyond its window:
// the exemption from battery optimisation (Doze), the notification
// permission, and a notification when a bearing arrives while the
// application is not on screen.
//
// Doze: with the screen off and the phone still, Android suspends the
// network for every application that is not on its exemption list, a
// foreground service and wake locks notwithstanding (learned on AX25Chat).
// The user grants the exemption through the system's own dialog.
//
// Called from AndroidUi.cpp through JNI; the application context comes
// through setContext() (Qt's public native interface).
//
// From AX25Chess; part of SATER Map.
// SPDX-License-Identifier: GPL-2.0-or-later

package org.satermap.app;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.PowerManager;
import android.provider.Settings;
import android.util.Log;

public class DeviceServices {
    private static final String TAG = "SaterMap.DeviceServices";
    private static final String BEARINGS_CHANNEL = "satermap.bearings";
    private static final int PERMISSION_REQUEST = 4711;
    private static Context context;
    private static int nextId = 100;

    public static synchronized void setContext(Context c) {
        context = c;
    }

    public static synchronized boolean ignoringBatteryOptimizations() {
        if (context == null || Build.VERSION.SDK_INT < 23) return true;
        try {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            return pm == null || pm.isIgnoringBatteryOptimizations(context.getPackageName());
        } catch (Exception e) {
            Log.e(TAG, "ignoringBatteryOptimizations: " + e);
            return true;
        }
    }

    public static synchronized void requestIgnoreBatteryOptimizations() {
        if (context == null || Build.VERSION.SDK_INT < 23) return;
        try {
            Intent intent = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
            intent.setData(Uri.parse("package:" + context.getPackageName()));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        } catch (Exception e) {
            // Some builds of Android hide the direct request; the list is the fallback.
            Log.e(TAG, "requestIgnoreBatteryOptimizations: " + e);
            try {
                Intent intent = new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            } catch (Exception e2) {
                Log.e(TAG, "battery optimisation settings: " + e2);
            }
        }
    }

    public static synchronized boolean notificationsAllowed() {
        if (context == null) return false;
        if (Build.VERSION.SDK_INT < 33) return true;
        return context.checkSelfPermission("android.permission.POST_NOTIFICATIONS") == PackageManager.PERMISSION_GRANTED;
    }

    // Android 13 and later: without it the service's notification is not
    // shown (the service runs all the same), nor the bearings received.
    public static synchronized void requestNotificationPermission() {
        if (context == null || Build.VERSION.SDK_INT < 33 || notificationsAllowed()) return;
        if (context instanceof Activity) {
            ((Activity) context).requestPermissions(new String[] {"android.permission.POST_NOTIFICATIONS"}, PERMISSION_REQUEST);
        }
    }

    // A bearing came in while SATER Map was not on screen.
    public static synchronized void notifyBearing(String channelName, String title, String text) {
        if (context == null || !notificationsAllowed()) return;
        try {
            NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            if (manager == null) return;
            if (Build.VERSION.SDK_INT >= 26) {
                NotificationChannel channel = new NotificationChannel(BEARINGS_CHANNEL, channelName, NotificationManager.IMPORTANCE_HIGH);
                manager.createNotificationChannel(channel);
            }
            int icon = context.getResources().getIdentifier("icon", "drawable", context.getPackageName());
            if (icon == 0) icon = android.R.drawable.ic_dialog_info;
            Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
            PendingIntent open = null;
            if (launch != null) {
                launch.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                open = PendingIntent.getActivity(context, 2, launch, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            }
            Notification.Builder builder = Build.VERSION.SDK_INT >= 26
                    ? new Notification.Builder(context, BEARINGS_CHANNEL)
                    : new Notification.Builder(context);
            builder.setSmallIcon(icon)
                   .setContentTitle(title)
                   .setContentText(text)
                   .setAutoCancel(true);
            if (open != null) builder.setContentIntent(open);
            manager.notify(nextId++, builder.build());
        } catch (Exception e) {
            Log.e(TAG, "notifyBearing: " + e);
        }
    }
}
