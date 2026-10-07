package dev.craftv.client.passthrough;

import static dev.craftv.link.Proto.*;

import dev.craftv.CraftLog;

/**
 * The owner-view settings the host's menu changes (PROTOCOL.md §7.17 INPUT OPTION, F8 in GTA). Client thread writes,
 * render thread reads; plain volatiles.
 */
public final class ViewOptions {
	public static volatile boolean crosshair = true;
	public static volatile boolean hand = true;
	/** 0 none, 1 placed blocks only (never the hidden ground), 2 everywhere. */
	public static volatile int outline = 1;
	/** Frames per second while composited; 0 = unlimited. */
	public static volatile int frameRate = 90;
	public static volatile boolean hud = true;
	/** Steve is drawn seated in host vehicles; off: hidden there (the host shows its own driver). */
	public static volatile boolean vehicleBody = true;

	private ViewOptions() {
	}

	public static void set(int option, int value) {
		switch (option) {
			case OPTION_CROSSHAIR -> crosshair = value != 0;
			case OPTION_HAND -> hand = value != 0;
			case OPTION_OUTLINE -> outline = Math.clamp(value, 0, 2);
			case OPTION_FRAME_RATE -> frameRate = value * 10;
			case OPTION_HUD -> hud = value != 0;
			case OPTION_VEHICLE_BODY -> vehicleBody = value != 0;
			default -> {
				return;
			}
		}
		CraftLog.limited("viewopt" + option, 500, "view option " + option + " = " + value + " (from the host's settings)");
	}
}
