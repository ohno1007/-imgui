package com.aimgui;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Method;

/**
 * IME bridge for AImGui's native ELF. Launched via /system/bin/app_process.
 *
 * Hosts a bare focusable View (no EditText / TextView / Typeface — those
 * NPE inside a system Context that has no Theme + no Typeface defaults)
 * with a custom InputConnection. IMEs see it as a text editor and deliver
 * characters through commitText(), which we forward to native via stdout.
 *
 * Protocol (line-based):
 *     →  READY / VIEW_OK / IME_SHOWN / IME_HIDDEN / PONG
 *     →  TEXT "<json-escaped string>"
 *     →  KEY  BACKSPACE | ENTER
 *     →  DEBUG <msg>     /  ERROR <msg>     /  FATAL <stack-trace>
 *     ←  SHOW / HIDE / CLEAR / PING / QUIT
 */
public final class Helper {

    private static volatile InputView sView;
    private static volatile InputMethodManager sImm;
    private static volatile WindowManager sWm;
    private static volatile Handler sHandler;
    private static volatile Object  sActivityThread;

    public static void main(String[] args) {
        out("READY");

        // Bring up libbinder's thread pool BEFORE any system-service call —
        // attach()/addView() callback into our process from AMS / WMS, and
        // without a thread serving Binder transactions those callbacks just
        // queue until the caller times out and SIGKILLs us.
        startBinderThreadPool();

        try {
            runMain(args);
        } catch (Throwable t) {
            out("FATAL " + describe(t));
            System.exit(1);
        }
    }

    private static void startBinderThreadPool() {
        try {
            final Class<?> bi = Class.forName("android.os.BinderInternal");
            final java.lang.reflect.Method joinThreadPool = bi.getMethod("joinThreadPool");
            for (int i = 0; i < 2; ++i) {
                Thread bt = new Thread(new Runnable() {
                    @Override public void run() {
                        try { joinThreadPool.invoke(null); }
                        catch (Throwable t) { out("DEBUG binder-pool died " + t.getMessage()); }
                    }
                }, "binder-pool-" + i);
                bt.setDaemon(false);   // keep the JVM alive
                bt.start();
            }
            // Give libbinder a moment to register the worker threads.
            try { Thread.sleep(120); } catch (InterruptedException ignored) {}
            out("DEBUG binder-pool-started");
        } catch (Throwable t) {
            out("DEBUG binder-pool-failed " + t.getMessage());
        }
    }

    private static void runMain(String[] args) {
        out("DEBUG looper-prepare myLooper=" + (Looper.myLooper() != null));
        if (Looper.myLooper() == null) {
            try { Looper.prepareMainLooper(); } catch (IllegalStateException ignored) {}
        }
        out("DEBUG looper-ready main=" + (Looper.getMainLooper() != null));

        sHandler = new Handler(Looper.getMainLooper());

        sHandler.post(new Runnable() {
            @Override public void run() {
                try { setupViews(); }
                catch (Throwable t) { out("FATAL setupViews " + describe(t)); }
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
        Context ctx = systemContextViaReflection();
        out("DEBUG setup-2 ctx=" + (ctx != null));
        if (ctx == null) { out("ERROR no-context"); return; }

        // WMS keeps a process map keyed by PID; raw app_process invocations
        // aren't in it, so addView throws "Unknown pid". Try to register us
        // by calling ActivityThread.attach(false). Likely fails (AMS has no
        // pending record for our PID either) but works on some forks.
        tryAttachToAms();

        sWm  = (WindowManager) ctx.getSystemService(Context.WINDOW_SERVICE);
        sImm = (InputMethodManager) ctx.getSystemService(Context.INPUT_METHOD_SERVICE);
        out("DEBUG setup-3 wm=" + (sWm != null) + " imm=" + (sImm != null));
        if (sWm == null || sImm == null) { out("ERROR no-services"); return; }

        sView = new InputView(ctx);
        sView.setFocusable(true);
        sView.setFocusableInTouchMode(true);
        out("DEBUG setup-4 view-created");

        sView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
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

        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                1, 1, 0, 0, 0,
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
                PixelFormat.TRANSLUCENT);
        lp.gravity = Gravity.TOP | Gravity.START;

        int[] types = {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.TYPE_SYSTEM_OVERLAY,
            WindowManager.LayoutParams.TYPE_SYSTEM_ALERT,
            WindowManager.LayoutParams.TYPE_PHONE,
            WindowManager.LayoutParams.TYPE_PRIORITY_PHONE,
            WindowManager.LayoutParams.TYPE_SYSTEM_ERROR,
            WindowManager.LayoutParams.TYPE_TOAST,
        };
        boolean added = false;
        for (int t : types) {
            lp.type = t;
            try {
                sWm.addView(sView, lp);
                out("VIEW_OK type=" + t);
                added = true;
                break;
            } catch (Throwable e) {
                out("DEBUG addView type=" + t + " " + e.getClass().getSimpleName() + " " + e.getMessage());
            }
        }
        if (!added) out("ERROR addView all-types-failed");
    }

    // Bare focusable View that announces itself as a text editor and
    // gives the IME a minimal InputConnection that forwards everything
    // to the native side.
    private static final class InputView extends View {
        InputView(Context ctx) { super(ctx); }

        @Override public boolean onCheckIsTextEditor() { return true; }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            outAttrs.inputType  = InputType.TYPE_CLASS_TEXT;
            outAttrs.imeOptions = EditorInfo.IME_ACTION_NONE;

            return new BaseInputConnection(this, false) {
                @Override
                public boolean commitText(CharSequence text, int newCursorPosition) {
                    if (text != null && text.length() > 0) {
                        out("TEXT " + jsonEscape(text.toString()));
                    }
                    return true;
                }
                @Override
                public boolean setComposingText(CharSequence text, int newCursorPosition) {
                    // Skip the composing preview (e.g. pinyin candidates);
                    // we wait for the final commitText().
                    return true;
                }
                @Override
                public boolean finishComposingText() { return true; }

                @Override
                public boolean sendKeyEvent(KeyEvent ev) {
                    if (ev.getAction() != KeyEvent.ACTION_DOWN) return true;
                    int code = ev.getKeyCode();
                    if (code == KeyEvent.KEYCODE_DEL) {
                        out("KEY BACKSPACE"); return true;
                    }
                    if (code == KeyEvent.KEYCODE_ENTER ||
                        code == KeyEvent.KEYCODE_NUMPAD_ENTER) {
                        out("KEY ENTER"); return true;
                    }
                    int ch = ev.getUnicodeChar();
                    if (ch != 0) {
                        out("TEXT " + jsonEscape(String.valueOf((char) ch)));
                        return true;
                    }
                    return false;
                }

                @Override
                public boolean deleteSurroundingText(int before, int after) {
                    for (int i = 0; i < before; ++i) out("KEY BACKSPACE");
                    return true;
                }
            };
        }
    }

    private static Context systemContextViaReflection() throws Exception {
        Class<?> at = Class.forName("android.app.ActivityThread");
        Object thread;
        try {
            thread = at.getMethod("systemMain").invoke(null);
        } catch (Throwable first) {
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
        sActivityThread = thread;
        return (Context) at.getMethod("getSystemContext").invoke(thread);
    }

    // Attempt to register our PID with AMS via ActivityThread.attach(false).
    // Runs on a side thread with a 3-second cap so it never blocks setup —
    // attach() can stall when binder isn't healthy.
    private static void tryAttachToAms() {
        if (sActivityThread == null) { out("DEBUG ams-attach-no-thread"); return; }
        final Object thread = sActivityThread;
        final Class<?> at = thread.getClass();

        Thread t = new Thread(new Runnable() {
            @Override public void run() {
                for (Class<?>[] sig : new Class<?>[][] {
                        { boolean.class, long.class },
                        { boolean.class },
                }) {
                    try {
                        java.lang.reflect.Method m = at.getDeclaredMethod("attach", sig);
                        m.setAccessible(true);
                        Object[] args = sig.length == 2 ? new Object[]{Boolean.FALSE, 0L}
                                                        : new Object[]{Boolean.FALSE};
                        m.invoke(thread, args);
                        out("DEBUG ams-attach-ok sig=" + sig.length);
                        return;
                    } catch (NoSuchMethodException nsme) {
                        // try next signature
                    } catch (Throwable e) {
                        Throwable c = (e.getCause() != null) ? e.getCause() : e;
                        out("DEBUG ams-attach-failed sig=" + sig.length + " " +
                            c.getClass().getSimpleName() + " " + c.getMessage());
                        return;
                    }
                }
                out("DEBUG ams-attach-no-method");
            }
        }, "ams-attach");
        t.setDaemon(true);
        t.start();
        try { t.join(3000); } catch (InterruptedException ignored) {}
        if (t.isAlive()) out("DEBUG ams-attach-timeout-3s");
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
                if (sView != null && sImm != null) {
                    sView.requestFocus();
                    sImm.showSoftInput(sView, InputMethodManager.SHOW_FORCED);
                } else {
                    out("DEBUG SHOW but view=" + (sView != null) + " imm=" + (sImm != null));
                }
                break;
            case "HIDE":
                if (sView != null && sImm != null && sView.getWindowToken() != null) {
                    sImm.hideSoftInputFromWindow(sView.getWindowToken(), 0);
                }
                break;
            case "CLEAR": break;       // bare View has no text state to clear
            case "QUIT":  System.exit(0); break;
            default: break;
        }
    }

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
        return sw.toString().replace('\n', ' ');
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
