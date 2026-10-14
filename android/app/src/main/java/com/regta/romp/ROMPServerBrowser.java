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
 *
 * Semua network jalan di background thread; hasilnya di-push ke WebView
 * via onServers(json).
 *
 * TODO(GAME):
 *  - Tampilkan dari menu/launcher (mis. tombol "Multiplayer" di MainActivity).
 *  - Implementasikan native methods via JNI:
 *      nativeFetchMasterList -> ROMP::ROMPBrowser::FetchMasterList
 *      nativeQueryServer     -> ROMP::ROMPBrowser::QueryServer
 *      nativeConnect         -> ROMP::Client::Instance().Connect(ip, port, nick)
 *    Kembalikan JSON dengan format yang dipakai di bawah.
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
        activity.runOnUiThread(() -> {
            if (webView == null) return;
            String esc = json.replace("\\", "\\\\")
                    .replace("'", "\\'").replace("\n", "\\n");
            webView.evaluateJavascript(
                    "window.onServers && window.onServers('" + esc + "')", null);
        });
    }

    private class Bridge {
        /** Dipanggil halaman saat user mengetuk server / tombol connect manual. */
        @JavascriptInterface
        public void connect(String ip, int port) {
            // TODO(GAME): nativeConnect -> Client::Connect(ip, port, nick).
            // Setelah connect sukses, hide() browser & masuk game.
            nativeConnect(ip, port);
            hide();
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
            boolean now = ROMPFavorites.toggle(getContext(), ip, port, name);
            pushFavorites();
        }

        @JavascriptInterface
        public boolean isFavorite(String ip, int port) {
            return ROMPFavorites.isFavorite(getContext(), ip, port);
        }

        @JavascriptInterface
        public void loadFavorites() {
            pushFavorites();
        }
    }

    private void pushFavorites() {
        final String json = ROMPFavorites.toJson(getContext());
        postToPage("onFavorites", json);
    }

    // TODO(GAME): implementasikan via JNI -> ROMP::ROMPBrowser.
    // Format JSON:
    //   FetchMasterList -> [{"ip":"1.2.3.4","port":7777,"hostname":"..","motd":"..","players":3,"maxplayers":50}, ...]
    //   QueryServer     -> {"ip":"..","port":7777,"hostname":"..","motd":"..","version":"romp1",
    //                       "players":3,"maxplayers":50,"has_password":false,"ping_ms":12,
    //                       "player_list":[{"id":0,"name":"Andi"}]}
    private static native String nativeFetchMasterList(String host, int port);
    private static native String nativeQueryServer(String host, int port);
    // Dengarkan beacon LAN (JSON array, format sama seperti QueryServer).
    private static native String nativeListenLan(int timeoutMs);
    private static native void nativeConnect(String ip, int port);
}
