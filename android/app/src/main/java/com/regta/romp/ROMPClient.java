package com.regta.romp;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * ROMPClient — facade Java untuk ROMP::Client (C++).
 *
 * Satu-satunya titik masuk Java ke koneksi game RO-MP. Alur pakai:
 *
 * <pre>
 *   ROMPClient c = ROMPClient.get(activity);
 *   c.setNick("Andi");
 *   c.init(nuiWebView);          // sambungkan platform + NUI WebView
 *   c.connect("127.0.0.1", 7777, "", (ok, msg) -> { ... });
 *   c.startAutoPump();           // nativeUpdate() ~30 Hz di UI thread
 *   c.sendChat("halo");
 *   c.disconnect();
 * </pre>
 *
 * Semua method native diimplementasikan di
 * android/app/src/main/cpp/src/Code/ROMP/ROMP_JNI.cpp.
 * Operasi network yang blokir (connect) jalan di background thread.
 */
public class ROMPClient {
    private static final String TAG = "ROMPClient";

    /** Callback hasil connect (selalu dipanggil di UI thread). */
    public interface ConnectListener {
        void onResult(boolean ok, String message);
    }

    /** Satu baris chat untuk di-render (warna ala SA-MP). */
    public static class ChatLine {
        public String kind;   // normal|server|me|pm|joinleave
        public int color;     // ARGB
        public String text;
        public long time;
    }

    /** Info satu pemain (lokal atau remote). */
    public static class PlayerInfo {
        public int id;
        public String name;
        public float x, y, z, heading;
        public boolean isLocal;
        public int pingMs;
    }

    private static ROMPClient instance;

    private Activity activity;
    private Handler mainHandler = new Handler(Looper.getMainLooper());
    private String nick = "RO-MP_Player";
    private boolean initialized = false;
    private boolean pumping = false;

    private final Runnable pumpRunnable = new Runnable() {
        @Override
        public void run() {
            if (!pumping) return;
            try {
                nativeUpdate();
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "nativeUpdate belum ter-link", e);
                pumping = false;
                return;
            }
            mainHandler.postDelayed(this, 33);  // ~30 Hz
        }
    };

    private ROMPClient(Activity activity) {
        this.activity = activity;
    }

    /** Ambil instance (per activity). */
    public static synchronized ROMPClient get(Activity activity) {
        if (instance == null) {
            instance = new ROMPClient(activity);
        } else {
            instance.activity = activity;
        }
        return instance;
    }

    public void setNick(String nick) {
        if (nick != null && !nick.trim().isEmpty()) {
            this.nick = nick.trim();
        }
    }

    public String getNick() {
        return nick;
    }

    /**
     * Sambungkan platform C++ + NUI WebView. Panggil sekali sebelum connect.
     * nuiView boleh null bila NUI tidak dipakai (pesan NUI hanya di-log).
     */
    public synchronized void init(ROMPWebView nuiView) {
        if (initialized) return;
        try {
            initialized = nativeInit(nuiView);
            if (!initialized) Log.w(TAG, "nativeInit gagal");
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "nativeInit belum ter-link", e);
        }
    }

    public boolean isInitialized() {
        return initialized;
    }

    /**
     * Connect ke server (background thread). Listener dipanggil di UI thread.
     * password boleh "" untuk server tanpa password.
     */
    public void connect(final String ip, final int port, final String password,
                        final ConnectListener listener) {
        if (!initialized) init(null);
        new Thread(() -> {
            boolean ok = false;
            String msg;
            try {
                ok = nativeConnect(ip, nick, port, password == null ? "" : password);
                msg = ok ? "Terhubung ke " + ip + ":" + port
                         : nativeGetLastReject();
            } catch (UnsatisfiedLinkError e) {
                msg = "JNI belum ter-link: " + e.getMessage();
            }
            final boolean fok = ok;
            final String fmsg = msg;
            activity.runOnUiThread(() -> listener.onResult(fok, fmsg));
        }).start();
    }

    public void disconnect() {
        stopAutoPump();
        try {
            nativeDisconnect();
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "nativeDisconnect belum ter-link", e);
        }
    }

    public boolean isConnected() {
        try {
            return nativeIsConnected();
        } catch (UnsatisfiedLinkError e) {
            return false;
        }
    }

    /** Panggil tiap frame game (atau pakai startAutoPump). */
    public void update() {
        try {
            nativeUpdate();
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "nativeUpdate belum ter-link", e);
        }
    }

    /** Jalankan nativeUpdate() otomatis ~30 Hz di UI thread. */
    public void startAutoPump() {
        if (pumping) return;
        pumping = true;
        mainHandler.post(pumpRunnable);
    }

    public void stopAutoPump() {
        pumping = false;
        mainHandler.removeCallbacks(pumpRunnable);
    }

    /** Kirim chat/command ke server. */
    public boolean sendChat(String text) {
        try {
            return nativeSendChat(text);
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "nativeSendChat belum ter-link", e);
            return false;
        }
    }

    /** Ambil baris chat yang tampil (tertua -> terbaru). */
    public List<ChatLine> getChatLines() {
        List<ChatLine> out = new ArrayList<>();
        try {
            JSONArray arr = new JSONArray(nativeGetChatLines());
            for (int i = 0; i < arr.length(); i++) {
                JSONObject o = arr.getJSONObject(i);
                ChatLine l = new ChatLine();
                l.kind = o.optString("kind", "normal");
                l.color = parseColor(o.optString("color", "#FFFFFF"));
                l.text = o.optString("text", "");
                l.time = o.optLong("time", 0);
                out.add(l);
            }
        } catch (Exception e) {
            Log.w(TAG, "getChatLines gagal", e);
        }
        return out;
    }

    /** Ambil semua pemain (entri pertama = pemain lokal). */
    public List<PlayerInfo> getPlayers() {
        List<PlayerInfo> out = new ArrayList<>();
        try {
            JSONArray arr = new JSONArray(nativeGetPlayers());
            for (int i = 0; i < arr.length(); i++) {
                JSONObject o = arr.getJSONObject(i);
                PlayerInfo p = new PlayerInfo();
                p.id = o.optInt("id", 0);
                p.name = o.optString("name", "?");
                p.x = (float) o.optDouble("x", 0);
                p.y = (float) o.optDouble("y", 0);
                p.z = (float) o.optDouble("z", 0);
                p.heading = (float) o.optDouble("heading", 0);
                p.isLocal = o.optBoolean("is_local", false);
                p.pingMs = o.optInt("ping_ms", -1);
                out.add(p);
            }
        } catch (Exception e) {
            Log.w(TAG, "getPlayers gagal", e);
        }
        return out;
    }

    /** Info server dari handshake WELCOME (null bila belum connect). */
    public JSONObject getServerInfo() {
        try {
            return new JSONObject(nativeGetServerInfo());
        } catch (Exception e) {
            Log.w(TAG, "getServerInfo gagal", e);
            return null;
        }
    }

    /** Set posisi pemain lokal (dipanggil game tiap frame sebelum update). */
    public void setLocalPos(float x, float y, float z, float heading) {
        try {
            nativeSetLocalPos(x, y, z, heading);
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "nativeSetLocalPos belum ter-link", e);
        }
    }

    /** RTT ping (ms) ke server, -1 bila belum terukur. */
    public int getPingMs() {
        try {
            return nativeGetPingMs();
        } catch (UnsatisfiedLinkError e) {
            return -1;
        }
    }

    public int getLocalId() {
        try {
            return nativeGetLocalId();
        } catch (UnsatisfiedLinkError e) {
            return -1;
        }
    }

    private static int parseColor(String hex) {
        try {
            String h = hex.startsWith("#") ? hex.substring(1) : hex;
            if (h.length() == 6) h = "FF" + h;
            return (int) Long.parseLong(h, 16);
        } catch (Exception e) {
            return 0xFFFFFFFF;
        }
    }

    // ---- Natives: diimplementasikan di ROMP_JNI.cpp ----
    private static native boolean nativeInit(ROMPWebView nuiView);
    private static native boolean nativeConnect(String host, String nick,
                                                int port, String password);
    private static native void nativeDisconnect();
    private static native void nativeUpdate();
    private static native boolean nativeIsConnected();
    private static native boolean nativeSendChat(String text);
    private static native String nativeGetChatLines();
    private static native String nativeGetPlayers();
    private static native String nativeGetServerInfo();
    private static native void nativeSetLocalPos(float x, float y, float z,
                                                 float heading);
    private static native int nativeGetPingMs();
    private static native int nativeGetLocalId();
    private static native String nativeGetLocalNick();
    private static native String nativeGetLastReject();
}
