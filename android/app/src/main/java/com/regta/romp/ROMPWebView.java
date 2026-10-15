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

/**
 * ROMPWebView — CONTOH overlay WebView untuk NUI ala FiveM.
 *
 * Alur:
 *   Lua server --(0x40 NUI_MESSAGE, JSON)--> C++ ROMP::Client
 *       --(platform OnNuiMessage)--> postToUI(json)
 *       --(JS onRomMessage)--> halaman HTML (inventory dsb.)
 *   JS --(window.romp.postMessage(name, data))--> nuiCallbackNative
 *       --(JNI, TODO)--> ROMP::Client::SendNuiCallback
 *       --(0x41 NUI_CALLBACK)--> Lua romp.onNUICallback(name, fn)
 *
 * Fokus sentuhan (mirip SetNuiFocus di FiveM): server mengirim 0x42
 * NUI_FOCUS; saat fokus aktif, WebView menelan sentuhan, saat tidak,
 * sentuhan diteruskan ke game.
 *
 * TODO(GAME):
 *  - Panggil attach(activity) sekali dari activity game (GTASA), atau lewat
 *    ROMPClient.get(activity).init(webView) yang otomatis memakai instance
 *    ini untuk callback NUI.
 *
 * Callback JS -&gt; native (window.romp.postMessage) diimplementasikan di
 * ROMP_JNI.cpp (Java_com_regta_romp_ROMPWebView_nuiCallbackNative) yang
 * memanggil ROMP::Client::SendNuiCallback.
 */
public class ROMPWebView {
    private final Activity activity;
    private WebView webView;
    private boolean uiFocused = false;

    public ROMPWebView(Activity activity) {
        this.activity = activity;
    }

    /** Pasang overlay WebView transparan di atas game. Panggil dari UI thread. */
    @SuppressLint({"SetJavaScriptEnabled", "AddJavascriptInterface"})
    public void attach() {
        activity.runOnUiThread(() -> {
            webView = new WebView(activity);
            webView.setBackgroundColor(Color.TRANSPARENT);

            WebSettings s = webView.getSettings();
            s.setJavaScriptEnabled(true);
            s.setDomStorageEnabled(true);
            s.setMediaPlaybackRequiresUserGesture(false);

            webView.setWebViewClient(new WebViewClient());
            webView.addJavascriptInterface(new RompBridge(), "romp");

            // Catatan sentuhan: WebView ini GONE saat UI tidak fokus, jadi
            // sentuhan otomatis jatuh ke game di bawahnya. Saat fokus
            // (setFocus(true)), WebView VISIBLE dan menelan sentuhan normal.

            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT);
            activity.addContentView(webView, lp);
            webView.setVisibility(View.GONE);
        });
    }

    /** Muat halaman UI, mis. "file:///android_asset/romp/nui/inventory.html". */
    public void loadPage(final String url) {
        activity.runOnUiThread(() -> {
            if (webView != null) webView.loadUrl(url);
        });
    }

    /** Kirim pesan JSON dari server ke JavaScript: onRomMessage(json).
     *  WebView boleh GONE — JS tetap jalan selama halaman sudah di-load. */
    public void postToUI(final String json) {
        activity.runOnUiThread(() -> {
            if (webView == null) return;
            String escaped = json
                    .replace("\\", "\\\\")
                    .replace("'", "\\'")
                    .replace("\n", "\\n");
            webView.evaluateJavascript(
                    "window.onRomMessage && window.onRomMessage('" + escaped + "')",
                    null);
        });
    }

    /** Terima fokus sentuhan dari server (0x42 NUI_FOCUS).
     *  fokus=true  -> WebView VISIBLE, sentuhan untuk UI.
     *  fokus=false -> WebView GONE, sentuhan kembali ke game. */
    public void setFocus(final boolean focused) {
        uiFocused = focused;
        activity.runOnUiThread(() -> {
            if (webView == null) return;
            webView.setVisibility(focused ? View.VISIBLE : View.GONE);
        });
    }

    /** Bridge JS -> native: window.romp.postMessage(name, dataJson). */
    private class RompBridge {
        @JavascriptInterface
        public void postMessage(String name, String dataJson) {
            nuiCallbackNative(name, dataJson);
        }
    }

    // Diimplementasikan di ROMP_JNI.cpp ->
    // ROMP::Client::SendNuiCallback (paket 0x41 NUI_CALLBACK ke server).
    private static native void nuiCallbackNative(String name, String dataJson);
}
