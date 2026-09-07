package com.bessy.thebeast;

import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

public class WolTileService extends TileService {
    static {
        System.loadLibrary("native_core");
    }

    private native int nativeExecuteWol(String config_path);

    private native int nativeExecuteShutdown(String config_path);
    private final String config_path = getFilesDir().getAbsolutePath() + "/config.bin";

    // First make this implmentation working then add ping on click before tile state change

    @Override
    public void onClick() {
        Tile tile = getQsTile();
        if (tile == null) return;

        int currentState = tile.getState();

        new Thread(() -> {
            if (currentState == Tile.STATE_INACTIVE) {
                if (nativeExecuteWol(config_path) == 0) {
                    tile.setState(Tile.STATE_ACTIVE);
                    tile.setLabel("Bessy");
                    tile.setSubtitle("On");
                }
            } else {
                if (nativeExecuteShutdown(config_path) == 0) {
                    tile.setState(Tile.STATE_INACTIVE);
                    tile.setLabel("Bessy");
                    tile.setSubtitle("Off");
                }
            }
            tile.updateTile();
        }).start();
    }
}
