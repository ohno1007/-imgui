package com.aimgui;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Method;

/**
 * IME bridge for AImGui's native ELF. Launched via /system/bin/app_process.
 * Hidden 1×1 EditText hosts the IME; characters travel back over stdout.
 *
 * Protocol (line-based):
 *     →  READY / VIEW_OK / IME_SHOWN / IME_HIDDEN / PONG
 *     →  TEXT "<json-escaped string>"
 *     →  ERROR <msg>  /  FATAL <stack-trace>
 *     ←  SHOW / HIDE / CLEAR / PING / QUIT
 */
public final class Helper {

    private static volatile EditText sEdit;
    private static volatile InputMethodManager sImm;
    private static volatile WindowManager sWm;
    private static volatile Handler sHandler;

    public static void main(String[] args) {
        out("READY");

        try {
            runMain(args);
        } catch (Throwable t) {
            out("FATAL " + describe(t));
            System.exit(1);
        }
    }

    private static void runMain(String[] args) {
        // app_process may or may not have prepared the main looper. Guard
        // both possibilities.
        out("DEBUG looper-prepare myLooper=" + (Looper.myLooper() != null));
        if (Looper.myLooper() == null) {
            try {
                Looper.prepareMainLooper();
            } catch (IllegalStateException already) {
                // Some Android forks pre-init it; safe to continue.
            }
        }
        out("DEBUG looper-ready main=" + (Looper.getMainLooper() != null));

        sHandler = new Handler(Looper.getMainLooper());

        // Set up the view tree on the looper thread, with a top-level
        // catch + stack trace so a NPE in EditText init doesn't kill us.
        sHandler.post(new Runnable() {
            @Override public void run() {
                try {
                    setupViews();
                } catch (Throwable t) {
                    out("FATAL setupViews " + describe(t));
                }
            }
        });

        Thread reader = new Thread(new Runnable() {
            @Override public void run() { readCommandsLoop(); }
        }, "aimgui-stdin");
        reader.setDaemon(true);
        reader.start();

        Looper.loop();
        out("DEBUG loop-returned");
    }

    private static void setupViews() throws Exception {
        out("DEBUG setup-1 get-context");
        Context base = systemContextViaReflection();
        out("DEBUG setup-2 ctx=" + (base != null));
        if (base == null) { out("ERROR no-context"); return; }

        // The bare system context has no theme — EditText's default
        // constructor tries to read text-appearance attrs and NPEs.
        // Wrap with a known theme.
        Context ctx = new ContextThemeWrapper(base, android.R.style.Theme_DeviceDefault);
        out("DEBUG setup-3 themed");

        sWm  = (WindowManager) ctx.getSystemService(Context.WINDOW_SERVICE);
        sImm = (InputMethodManager) ctx.getSystemService(Context.INPUT_METHOD_SERVICE);
        out("DEBUG setup-4 wm=" + (sWm != null) + " imm=" + (sImm != null));
        if (sWm == null || sImm == null) { out("ERROR no-services"); return; }

        sEdit = new EditText(ctx);
        out("DEBUG setup-5 edit-created");
        sEdit.setSingleLine(false);
        sEdit.setFocusableInTouchMode(true);

        // Detect IME visibility so the native side can mask its mouse input.
        sEdit.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            private boolean prev = false;
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                boolean shown;
                if (Build.VERSION.SDK_INT >= 30) {
                    shown = insets.isVisible(WindowInsets.Type.ime());
                } else {
                    shown = insets.getSystemWindowInsetBottom() > 100;
                }
                if (shown != prev) {
                    prev = shown;
                    out(shown ? "IME_SHOWN" : "IME_HIDDEN");
                }
                return insets;
            }
        });

        sEdit.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void afterTextChanged(Editable s) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (count > 0) {
                    out("TEXT " + jsonEscape(s.subSequence(start, start + count).toString()));
                }
            }
        });

        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                1, 1, 0, 0, 0,
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
                PixelFormat.TRANSLUCENT);
        lp.gravity = Gravity.TOP | Gravity.START;

        // Try the modern overlay type first, then older system overlays. Each
        // of these can fail with a SecurityException or BadTokenException
        // depending on the OEM build; report all failures so we can debug.
        int[] types = {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.TYPE_SYSTEM_OVERLAY,
            WindowManager.LayoutParams.TYPE_SYSTEM_ALERT,
            WindowManager.LayoutParams.TYPE_PHONE,
        };
        boolean added = false;
        for (int t : types) {
            lp.type = t;
            try {
                sWm.addView(sEdit, lp);
                out("VIEW_OK type=" + t);
                added = true;
                break;
            } catch (Throwable e) {
                out("DEBUG addView type=" + t + " " + e.getClass().getSimpleName() + " " + e.getMessage());
            }
        }
        if (!added) out("ERROR addView all-types-failed");
    }

    // ActivityThread.systemMain().getSystemContext() via reflection so we
    // build without a stub framework.jar.
    private static Context systemContextViaReflection() throws Exception {
        Class<?> at = Class.forName("android.app.ActivityThread");
        Object thread;
        try {
            thread = at.getMethod("systemMain").invoke(null);
        } catch (Throwable first) {
            // Some OEMs gate systemMain — try currentActivityThread / new.
            try {
                thread = at.getMethod("currentActivityThread").invoke(null);
                if (thread == null) thread = at.getConstructor().newInstance();
            } catch (Throwable second) {
                throw new RuntimeException(
                    "systemMain failed (" + first.getMessage() +
                    ") + currentActivityThread failed (" + second.getMessage() + ")",
                    first);
            }
        }
        Method getSystemContext = at.getMethod("getSystemContext");
        return (Context) getSystemContext.invoke(thread);
    }

    private static void readCommandsLoop() {
        try (BufferedReader br = new BufferedReader(new InputStreamReader(System.in))) {
            String line;
            while ((line = br.readLine()) != null) {
                final String cmd = line.trim();
                if (sHandler != null) {
                    sHandler.post(new Runnable() {
                        @Override public void run() {
                            try { handleCommand(cmd); }
                            catch (Throwable t) { out("ERROR cmd " + describe(t)); }
                        }
                    });
                } else {
                    try { handleCommand(cmd); }
                    catch (Throwable t) { out("ERROR cmd " + describe(t)); }
                }
            }
        } catch (Throwable ignored) {}
        System.exit(0);
    }

    private static void handleCommand(String cmd) {
        switch (cmd) {
            case "PING":  out("PONG"); break;
            case "SHOW":
                if (sEdit != null && sImm != null) {
                    sEdit.requestFocus();
                    sImm.showSoftInput(sEdit, InputMethodManager.SHOW_FORCED);
                } else {
                    out("DEBUG SHOW but edit=" + (sEdit != null) + " imm=" + (sImm != null));
                }
                break;
            case "HIDE":
                if (sEdit != null && sImm != null && sEdit.getWindowToken() != null) {
                    sImm.hideSoftInputFromWindow(sEdit.getWindowToken(), 0);
                }
                break;
            case "CLEAR":
                if (sEdit != null) sEdit.setText("");
                break;
            case "QUIT":
                System.exit(0);
                break;
            default: break;
        }
    }

    // ─── Output helpers ──────────────────────────────────────────────
    private static synchronized void out(String s) {
        System.out.println(s);
        System.out.flush();
    }

    private static String describe(Throwable t) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        pw.print(t.getClass().getName());
        pw.print(": ");
        pw.println(t.getMessage());
        for (StackTraceElement e : t.getStackTrace()) {
            pw.print("    at "); pw.println(e);
        }
        Throwable cause = t.getCause();
        if (cause != null && cause != t) {
            pw.print("  caused by ");
            pw.print(cause.getClass().getName()); pw.print(": "); pw.println(cause.getMessage());
        }
        return sw.toString().replace('\n', ' ');   // logcat-friendly single line
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
