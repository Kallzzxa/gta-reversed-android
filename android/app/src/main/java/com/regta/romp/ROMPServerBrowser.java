package com.regta.romp;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;

import org.json.JSONArray;
import org.json.JSONObject;

/**
 * ROMPServerBrowser — CONTOH server browser (daftar server yang nyala).
 *
 * Alur:
 *   1. Ambil daftar dari master server (FetchMasterList).
 *   2. Query tiap server via UDP untuk info live (nama, pemain, ping).
 *   3. Tampilkan di halaman browser.html; ketuk server untuk connect.
 *      Server ber-password (has_password=1) menampilkan dialog password
 *      ala DIALOG_STYLE_PASSWORD sebelum connect dipanggil.
 *
 * Semua network jalan di background thread; hasilnya di-push ke WebView
 * via onServers(json).
 *
 * TODO(GAME):
 *  - Tampilkan dari menu/launcher (mis. tombol "Multiplayer" di MainActivity).
 *    Nick diambil dari ROMPClient.get(activity).setNick(...) — default
 *    "RO-MP_Player" bila tidak di-set.
 *
 * Method native di bawah diimplementasikan di
 * android/app/src/main/cpp/src/Code/ROMP/ROMP_JNI.cpp
 * (Java_com_regta_romp_ROMPServerBrowser_native*). Connect dirutekan
 * lewat ROMPClient (satu-satunya pemilik koneksi).
 */
public class ROMPServerBrowser {
    private final Activity activity;
    private WebView webView;

    // Ganti dengan master server publik (mis. di VPS).
    private String masterHost = "127.0.0.1";
    private int masterPort = 7778;

    public ROMPServerBrowser(Activity activity) {
        this.activity = activity;
    }

    public void setMaster(String host, int port) {
        masterHost = host;
        masterPort = port;
    }

    @SuppressLint({"SetJavaScriptEnabled", "AddJavascriptInterface"})
    public void show() {
        activity.runOnUiThread(() -> {
            if (webView == null) {
                webView = new WebView(activity);
                webView.setBackgroundColor(Color.TRANSPARENT);
                WebSettings s = webView.getSettings();
                s.setJavaScriptEnabled(true);
                s.setDomStorageEnabled(true);
                webView.setWebViewClient(new WebViewClient());
                webView.addJavascriptInterface(new Bridge(), "romp");
                activity.addContentView(webView, new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT));
            }
            webView.setVisibility(View.VISIBLE);
            webView.loadUrl("file:///android_asset/romp/browser.html");
            refresh();
        });
    }

    public void hide() {
        activity.runOnUiThread(() -> {
            if (webView != null) webView.setVisibility(View.GONE);
        });
    }

    /** Ambil daftar + query di background thread, push ke halaman. */
    public void refresh() {
        new Thread(() -> {
            try {
                JSONArray arr = new JSONArray();
                // 1. Daftar dari master.
                String listJson = nativeFetchMasterList(masterHost, masterPort);
                JSONArray master = new JSONArray(listJson);
                for (int i = 0; i < master.length(); i++) {
                    JSONObject e = master.getJSONObject(i);
                    String ip = e.getString("ip");
                    int port = e.getInt("port");
                    // 2. Query info live per server.
                    String q = nativeQueryServer(ip, port);
                    arr.put(new JSONObject(q));
                }
                pushToPage(arr.toString());
            } catch (Exception ex) {
                pushToPage("[]");
            }
        }).start();
    }

    /** Scan LAN: dengarkan beacon broadcast (tanpa perlu master/VPS). */
    public void scanLan() {
        new Thread(() -> {
            try {
                String json = nativeListenLan(6000);
                pushToPage(json);
            } catch (Exception ex) {
                pushToPage("[]");
            }
        }).start();
    }

    private void pushToPage(final String json) {
        postToPage("onServers", json);
    }

    /** Kirim JSON ke fungsi JS window.<fn>(json). */
    private void postToPage(final String fn, final String json) {
        activity.runOnUiThread(() -> {
            if (webView == null) return;
            String esc = json.replace("\\", "\\\\")
                    .replace("'", "\\'").replace("\n", "\\n");
            webView.evaluateJavascript(
                    "window." + fn + " && window." + fn + "('" + esc + "')", null);
        });
    }

    private class Bridge {
        /** Dipanggil halaman saat user mengetuk server / tombol connect manual.
         *  Password sudah ditanyakan halaman lewat dialog ala
         *  DIALOG_STYLE_PASSWORD bila server punya password.
         *  Connect dirutekan lewat ROMPClient (pemilik tunggal koneksi);
         *  nick diambil dari ROMPClient.get(activity).setNick(...). */
        @JavascriptInterface
        public void connect(String ip, int port, String password) {
            ROMPClient client = ROMPClient.get(activity);
            client.connect(ip, port, password == null ? "" : password,
                    (ok, msg) -> {
                        if (ok) {
                            hide();
                        } else {
                            android.widget.Toast.makeText(activity, msg,
                                    android.widget.Toast.LENGTH_LONG).show();
                        }
                    });
        }

        /** Versi lama tanpa password (backward compat): connect tanpa password. */
        @JavascriptInterface
        public void connect(String ip, int port) {
            connect(ip, port, "");
        }

        @JavascriptInterface
        public void refresh() {
            ROMPServerBrowser.this.refresh();
        }

        @JavascriptInterface
        public void scanLan() {
            ROMPServerBrowser.this.scanLan();
        }

        @JavascriptInterface
        public void toggleFavorite(String ip, int port, String name) {
            ROMPFavorites.toggle(activity, ip, port, name);
            pushFavorites();
        }

        @JavascriptInterface
        public boolean isFavorite(String ip, int port) {
            return ROMPFavorites.isFavorite(activity, ip, port);
        }

        @JavascriptInterface
        public void loadFavorites() {
            pushFavorites();
        }
    }

    private void pushFavorites() {
        final String json = ROMPFavorites.toJson(activity);
        postToPage("onFavorites", json);
    }

    // Diimplementasikan di ROMP_JNI.cpp
    // (Java_com_regta_romp_ROMPServerBrowser_native*).
    // Format JSON:
    //   FetchMasterList -> [{"ip":"1.2.3.4","port":7777,"hostname":"..","motd":"..","players":3,"maxplayers":50}, ...]
    //   QueryServer     -> {"ip":"..","port":7777,"hostname":"..","motd":"..","version":"romp1",
    //                       "players":3,"maxplayers":50,"has_password":false,"ping_ms":12,
    //                       "player_list":[{"id":0,"name":"Andi"}]}
    //   ListenLan       -> array format QueryServer.
    private static native String nativeFetchMasterList(String host, int port);
    private static native String nativeQueryServer(String host, int port);
    // Dengarkan beacon LAN (JSON array, format sama seperti QueryServer).
    private static native String nativeListenLan(int timeoutMs);
}
