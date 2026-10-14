package com.regta.romp;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Favorit server browser — mirip FavoritesInfo di SA-MP Mobile Launcher.
 * Disimpan sebagai JSON: ROMP/favorites.json -> {"servers":[{"ip","port","name"}]}
 */
public class ROMPFavorites {

    public static class Entry {
        public String ip;
        public int port;
        public String name;

        public Entry(String ip, int port, String name) {
            this.ip = ip;
            this.port = port;
            this.name = (name == null || name.isEmpty())
                    ? ip + ":" + port : name;
        }

        public JSONObject toJson() {
            try {
                JSONObject o = new JSONObject();
                o.put("ip", ip);
                o.put("port", port);
                o.put("name", name);
                return o;
            } catch (Exception e) {
                return new JSONObject();
            }
        }
    }

    private static boolean loaded = false;
    private static final List<Entry> servers = new ArrayList<>();

    private static File file(Context ctx) {
        File dir = new File(ctx.getExternalFilesDir(null), "ROMP");
        dir.mkdirs();
        return new File(dir, "favorites.json");
    }

    public static synchronized void load(Context ctx) {
        if (loaded) return;
        loaded = true;
        servers.clear();
        try {
            File f = file(ctx);
            if (!f.exists()) return;
            StringBuilder sb = new StringBuilder();
            BufferedReader r = new BufferedReader(new FileReader(f));
            String line;
            while ((line = r.readLine()) != null) sb.append(line);
            r.close();
            if (sb.length() == 0) return;
            JSONArray arr = new JSONObject(sb.toString()).getJSONArray("servers");
            for (int i = 0; i < arr.length(); i++) {
                JSONObject o = arr.getJSONObject(i);
                servers.add(new Entry(
                        o.optString("ip", ""),
                        o.optInt("port", 7777),
                        o.optString("name", "")));
            }
        } catch (Exception e) {
            servers.clear();
        }
    }

    public static synchronized void save(Context ctx) {
        try {
            JSONObject root = new JSONObject();
            JSONArray arr = new JSONArray();
            for (Entry e : servers) arr.put(e.toJson());
            root.put("servers", arr);
            File f = file(ctx);
            BufferedWriter w = new BufferedWriter(new FileWriter(f, false));
            w.write(root.toString());
            w.close();
        } catch (Exception e) {
            // abaikan
        }
    }

    public static synchronized boolean add(Context ctx, String ip, int port,
                                           String name) {
        load(ctx);
        for (Entry e : servers) {
            if (e.ip.equals(ip) && e.port == port) return false;
        }
        servers.add(new Entry(ip, port, name));
        save(ctx);
        return true;
    }

    public static synchronized boolean remove(Context ctx, String ip, int port) {
        load(ctx);
        Iterator<Entry> it = servers.iterator();
        while (it.hasNext()) {
            Entry e = it.next();
            if (e.ip.equals(ip) && e.port == port) {
                it.remove();
                save(ctx);
                return true;
            }
        }
        return false;
    }

    public static synchronized boolean toggle(Context ctx, String ip, int port,
                                              String name) {
        if (isFavorite(ctx, ip, port)) {
            remove(ctx, ip, port);
            return false;
        } else {
            add(ctx, ip, port, name);
            return true;
        }
    }

    public static synchronized boolean isFavorite(Context ctx, String ip,
                                                  int port) {
        load(ctx);
        for (Entry e : servers) {
            if (e.ip.equals(ip) && e.port == port) return true;
        }
        return false;
    }

    public static synchronized List<Entry> getAll(Context ctx) {
        load(ctx);
        return new ArrayList<>(servers);
    }

    /** JSON array untuk dikirim ke halaman web. */
    public static synchronized String toJson(Context ctx) {
        load(ctx);
        JSONArray arr = new JSONArray();
        for (Entry e : servers) {
            JSONObject o = e.toJson();
            try {
                // samakan format dengan hasil QueryServer
                o.put("motd", "");
                o.put("version", "romp1");
                o.put("players", 0);
                o.put("maxplayers", 0);
                o.put("has_password", false);
                o.put("ping_ms", -1);
            } catch (Exception ignored) {}
            arr.put(o);
        }
        return arr.toString();
    }
}
