package com.kurdish.roleplay.launcher.util;

import android.content.Context;

import java.io.File;

public final class GameStorage {
    private static final String PACKAGE_DIR = "com.kurdish.roleplay";
    private static final String SAMP_DIR = "SAMP";

    private GameStorage() {
    }

    public static File getGameBaseDirectory(Context context) {
        File mediaRoot = new File("/storage/emulated/0/Android/media");
        File gameDir = new File(mediaRoot, PACKAGE_DIR);
        if (!gameDir.exists()) {
            gameDir.mkdirs();
        }

        if (gameDir.exists() || context == null) {
            return gameDir;
        }

        File fallback = context.getExternalFilesDir(null);
        if (fallback == null) {
            return gameDir;
        }
        return fallback;
    }

    public static String getGameBasePath(Context context) {
        return withTrailingSlash(getGameBaseDirectory(context).getAbsolutePath());
    }

    public static File getSampDirectory(Context context) {
        File sampDir = new File(getGameBaseDirectory(context), SAMP_DIR);
        if (!sampDir.exists()) {
            sampDir.mkdirs();
        }
        return sampDir;
    }

    public static File getSettingsFile(Context context) {
        return new File(getSampDirectory(context), "settings.ini");
    }

    public static File getDownloadDirectory(Context context) {
        File downloadDir = new File(getGameBaseDirectory(context), "download");
        if (!downloadDir.exists()) {
            downloadDir.mkdirs();
        }
        return downloadDir;
    }

    private static String withTrailingSlash(String path) {
        return path.endsWith("/") ? path : path + "/";
    }
}
