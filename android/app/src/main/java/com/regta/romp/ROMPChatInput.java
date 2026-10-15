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
 * <p>Method {@code sendChatNative} diimplementasikan di
 * android/app/src/main/cpp/src/Code/ROMP/ROMP_JNI.cpp
 * ({@code Java_com_regta_romp_ROMPChatInput_sendChatNative}) yang memanggil
 * {@code ROMP::Client::Instance().SendChat(pesan)}.</p>
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
     * Meneruskan teks chat ke native (ROMP::Client::SendChat).
     * Diimplementasikan di ROMP_JNI.cpp sebagai
     * Java_com_regta_romp_ROMPChatInput_sendChatNative.
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
