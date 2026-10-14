package com.regta.romp;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;

/**
 * ROMPChatInput — dialog input chat ala SA-MP mobile.
 *
 * Muncul saat pemain menekan tombol chat di HUD: berisi EditText untuk
 * mengetik pesan + tombol "Kirim". Saat dikirim, teks diteruskan ke
 * method native {@link #sendChatNative(String)}.
 *
 * <p><b>PENTING — JNI BELUM DISAMBUNG (TODO):</b>
 * Method {@code sendChatNative} di bawah ini hanya dideklarasikan sebagai
 * {@code native}. Implementasi JNI-nya
 * ({@code Java_com_regta_romp_ROMPChatInput_sendChatNative} di reGTA.cpp)
 * <b>belum ditulis</b> — itu pekerjaan lanjutan. Fungsi JNI tersebut
 * nantinya cukup memanggil:
 * <pre>
 *   ROMP::Client::Instance().SendChat(pesan);
 * </pre>
 * Sampai JNI-nya ada, memanggil dialog ini akan melempar
 * UnsatisfiedLinkError saat tombol Kirim ditekan (ditangkap dan diabaikan
 * di bawah supaya tidak crash).</p>
 *
 * <p>File ini CONTOH dan tidak di-reference dari file lain, jadi aman
 * terhadap build yang sudah ada.</p>
 */
public class ROMPChatInput extends Dialog {

    private EditText inputText;

    public ROMPChatInput(Context context) {
        super(context);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        // Layout dibuat programmatic supaya tidak butuh file XML baru.
        LinearLayout root = new LinearLayout(getContext());
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (16 * getContext().getResources().getDisplayMetrics().density);
        root.setPadding(pad, pad, pad, pad);

        inputText = new EditText(getContext());
        inputText.setHint("Ketik pesan... (/help untuk command)");
        inputText.setSingleLine(true);
        inputText.setMaxLines(1);
        root.addView(inputText,
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

        Button sendButton = new Button(getContext());
        sendButton.setText("Kirim");
        sendButton.setOnClickListener(v -> {
            String text = inputText.getText().toString().trim();
            if (!text.isEmpty()) {
                try {
                    sendChatNative(text);
                } catch (UnsatisfiedLinkError e) {
                    // JNI belum disambung (lihat TODO di atas) — jangan crash.
                    android.util.Log.w("ROMPChatInput",
                            "sendChatNative belum ada implementasi JNI-nya", e);
                }
                inputText.setText("");
            }
            dismiss();
        });
        root.addView(sendButton,
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

        setContentView(root);
    }

    /**
     * Meneruskan teks chat ke native.
     * TODO(JNI): implementasikan di reGTA.cpp sebagai
     * Java_com_regta_romp_ROMPChatInput_sendChatNative, isinya panggil
     * ROMP::Client::Instance().SendChat(...).
     */
    public native void sendChatNative(String text);

    /** Helper: tampilkan dialog chat. */
    public static void show(Context context) {
        ROMPChatInput dialog = new ROMPChatInput(context);
        dialog.show();
        // Fokuskan keyboard ke EditText.
        dialog.getWindow().setSoftInputMode(
                android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
    }
}
