package com.regta.launcher;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;


import androidx.annotation.NonNull;

import com.regta.core.R;
import com.regta.core.REGTA;
import com.regta.romp.ROMPDataManager;

import java.util.List;

import pub.devrel.easypermissions.EasyPermissions;

public class MainActivity extends Activity implements EasyPermissions.PermissionCallbacks {

    private static final int MAIN_PERMISSIONS_REQUEST_CODE = 300;
    String[] main_permissions = {
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE
    };

    private void launchGame() {
        // Download game data first if missing, then start the game.
        ROMPDataManager.checkAndPrepare(this, () -> {
            startActivity(new Intent(this, REGTA.class));
            this.finish();
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.loadingscreen);
        if (EasyPermissions.hasPermissions(this, main_permissions)) {
            launchGame();
        } else {
            EasyPermissions.requestPermissions(this,
                    "The application needs permission to write to memory",
                    MAIN_PERMISSIONS_REQUEST_CODE, main_permissions
            );
        }
    }

    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        EasyPermissions.onRequestPermissionsResult(requestCode, permissions, grantResults, this);
    }

    public void onPermissionsGranted(int requestCode, @NonNull List<String> list) {
        if (requestCode == MAIN_PERMISSIONS_REQUEST_CODE) {
            launchGame();
        }
    }

    public void onPermissionsDenied(int requestCode, @NonNull List<String> list) {
        // TODO
    }
}