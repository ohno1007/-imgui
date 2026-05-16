package com.aimgui;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.reflect.Method;

/**
 * IME bridge for AImGui's native ELF.
 *
 * Launched via /system/bin/app_process from the native process. Sets up a
 * tiny invisible WindowManager view that hosts an EditText; commands come
 * in on stdin, typed text goes out on stdout.
 *
 * Protocol (line-based):
 *     →  READY                         (we print once on startup)
 *     ←  SHOW                          show soft keyboard
 *     ←  HIDE                          hide soft keyboard
 *     ←  CLEAR                         clear our edit field
 *     ←  PING                          → PONG
 *     ←  QUIT                          exit
 *     →  TEXT "<json-escaped string>"  user typed
 */
public final class Helper {

    private static volatile EditText sEdit;
    private static volatile InputMethodManager sImm;
    private static volatile WindowManager sWm;
    private static volatile Handler sHandler;

    public static void main(String[] args) {
        // The native side wants line-buffered output.
        System.out.println("READY");
        System.out.flush();

        Looper.prepareMainLooper();

        try {
            Context ctx = systemContextViaReflection();
            sHandler = new Handler(Looper.getMainLooper());
            final Context ctxFinal = ctx;
            sHandler.post(new Runnable() { @Override public void run() { setupViews(ctxFinal); } });
        } catch (Throwable t) {
            System.out.println("ERROR setupContext " + t.getMessage());
            System.out.flush();
            // Stay alive so the native side can still PING / QUIT.
        }

        Thread reader = new Thread(new Runnable() { @Override public void run() { readCommandsLoop(); } }, "aimgui-stdin");
        reader.setDaemon(true);
        reader.start();

        Looper.loop();
    }

    // ActivityThread.systemMain().getSystemContext() — all hidden APIs,
    // reach them with reflection so we don't need a framework stub jar.
    private static Context systemContextViaReflection() throws Exception {
        Class<?> at = Class.forName("android.app.ActivityThread");
        Method systemMain = at.getMethod("systemMain");
        Object thread = systemMain.invoke(null);
        Method getSystemContext = at.getMethod("getSystemContext");
        return (Context) getSystemContext.invoke(thread);
    }

    private static void setupViews(Context ctx) {
        sWm  = ctx.getSystemService(WindowManager.class);
        sImm = ctx.getSystemService(InputMethodManager.class);
        if (sWm == null || sImm == null) {
            System.out.println("ERROR no-window-manager");
            System.out.flush();
            return;
        }

        sEdit = new EditText(ctx);
        sEdit.setSingleLine(false);
        sEdit.setFocusableInTouchMode(true);
        sEdit.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void afterTextChanged(Editable s) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (count > 0) {
                    String added = s.subSequence(start, start + count).toString();
                    System.out.print("TEXT ");
                    System.out.println(jsonEscape(added));
                    System.out.flush();
                }
            }
        });

        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                1, 1,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
                PixelFormat.TRANSLUCENT);
        lp.gravity = Gravity.TOP | Gravity.START;

        try {
            sWm.addView(sEdit, lp);
            System.out.println("VIEW_OK");
        } catch (Throwable t) {
            // Fall back to deprecated overlay types if the modern one is gated.
            try {
                lp.type = WindowManager.LayoutParams.TYPE_SYSTEM_OVERLAY;
                sWm.addView(sEdit, lp);
                System.out.println("VIEW_OK_FALLBACK");
            } catch (Throwable t2) {
                System.out.println("ERROR addView " + t2.getMessage());
            }
        }
        System.out.flush();
    }

    private static void readCommandsLoop() {
        try (BufferedReader br = new BufferedReader(new InputStreamReader(System.in))) {
            String line;
            while ((line = br.readLine()) != null) {
                final String cmd = line.trim();
                if (sHandler != null) {
                    sHandler.post(new Runnable() { @Override public void run() { handleCommand(cmd); } });
                } else {
                    handleCommand(cmd);  // pre-handler PING / QUIT
                }
            }
        } catch (Throwable ignored) {}
        System.exit(0);
    }

    private static void handleCommand(String cmd) {
        switch (cmd) {
            case "PING":
                System.out.println("PONG"); System.out.flush(); break;
            case "SHOW":
                if (sEdit != null && sImm != null) {
                    sEdit.requestFocus();
                    sImm.showSoftInput(sEdit, InputMethodManager.SHOW_FORCED);
                }
                break;
            case "HIDE":
                if (sEdit != null && sImm != null) {
                    sImm.hideSoftInputFromWindow(sEdit.getWindowToken(), 0);
                }
                break;
            case "CLEAR":
                if (sEdit != null) sEdit.setText("");
                break;
            case "QUIT":
                System.exit(0); break;
            default:
                break;
        }
    }

    private static String jsonEscape(String s) {
        StringBuilder b = new StringBuilder("\"");
        for (int i = 0; i < s.length(); ++i) {
            char c = s.charAt(i);
            switch (c) {
                case '"':  b.append("\\\""); break;
                case '\\': b.append("\\\\"); break;
                case '\n': b.append("\\n");  break;
                case '\r': b.append("\\r");  break;
                case '\t': b.append("\\t");  break;
                default:
                    if (c < 0x20) b.append(String.format("\\u%04x", (int) c));
                    else          b.append(c);
            }
        }
        b.append('"');
        return b.toString();
    }
}
