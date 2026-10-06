package dev.craftv.client.passthrough;

import static dev.craftv.link.Proto.*;

import com.mojang.blaze3d.pipeline.RenderTarget;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.renderpearl.api.buffers.GpuBuffer;
import com.mojang.renderpearl.api.buffers.GpuBufferSlice;
import com.mojang.renderpearl.api.commands.CommandEncoder;
import dev.craftv.CraftLog;
import dev.craftv.link.Messages;
import dev.craftv.link.Win32;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;
import org.joml.Vector4f;

/**
 * Hands the owner's view to the host through {@code Local\CraftV_Frame_v1} (PROTOCOL.md §11). The world layer
 * (colour + depth) is copied just before the hand is drawn; the colour target is then cleared so what follows (hand,
 * HUD, screens) forms the overlay layer, copied at the end of the frame. Readback is asynchronous: a ring of GPU
 * buffers, published when the GPU fence passes (the next frame at the latest). Adapted from
 * minecraft-gta5-passthrough's FrameExporter (rehan-remade, MIT).
 */
public final class FrameExporter {
	private static final int RING = 3;
	private static final long STUCK_NANOS = 1_000_000_000L;
	private static final Vector4f TRANSPARENT = new Vector4f(0.0F, 0.0F, 0.0F, 0.0F);
	private static final ValueLayout.OfInt INT = ValueLayout.JAVA_INT_UNALIGNED;
	private static final ValueLayout.OfLong LONG = ValueLayout.JAVA_LONG_UNALIGNED;
	private static final ValueLayout.OfFloat FLOAT = ValueLayout.JAVA_FLOAT_UNALIGNED;
	private static final ValueLayout.OfDouble DOUBLE = ValueLayout.JAVA_DOUBLE_UNALIGNED;
	private static final float NEAR = 0.05F; // Minecraft's near plane
	private static final long MAPPING_BYTES = FRAME_HEADER_BYTES + FRAME_SLOT_STRIDE * FRAME_SLOTS;

	private static float far = 1024.0F;
	private static MemorySegment mapping;
	private static boolean failed;
	private static boolean warnedSize;
	private static final Capture[] ring = new Capture[RING];
	private static int ringNext;
	private static int slotNext;
	private static long frameCounter;
	private static long publishCounter;
	private static Capture current;

	private FrameExporter() {
	}

	private static final class Capture {
		GpuBuffer color, depth, overlay;
		int width, height;
		long generation;
		boolean busy;
		long busySince;
		Messages.Camera pose;
		float far;
		long frame;
		long captureNanos;

		void allocate(int w, int h) {
			free();
			long n = (long) w * h * 4;
			int usage = GpuBuffer.USAGE_MAP_READ | GpuBuffer.USAGE_COPY_DST;
			color = RenderSystem.getDevice().createBuffer(() -> "craftv world colour", usage, n);
			depth = RenderSystem.getDevice().createBuffer(() -> "craftv world depth", usage, n);
			overlay = RenderSystem.getDevice().createBuffer(() -> "craftv overlay", usage, n);
			width = w;
			height = h;
		}

		void free() {
			for (GpuBuffer b : new GpuBuffer[] { color, depth, overlay }) {
				if (b != null) {
					b.close();
				}
			}
			color = depth = overlay = null;
		}
	}

	public static void setFar(float depthFar) {
		far = depthFar;
	}

	private static boolean ensureMapping() {
		if (mapping != null) {
			return true;
		}
		if (failed) {
			return false;
		}
		Win32.MappedView v = Win32.createOrOpenMapping(FRAME_MAPPING_NAME, MAPPING_BYTES);
		if (!v.ok() || v.viewBytes() < MAPPING_BYTES) {
			failed = true;
			CraftLog.warn("passthrough: frame export off, couldn't create " + FRAME_MAPPING_NAME + ": " + (v.ok() ? "view too small" : v.error()));
			return false;
		}
		MemorySegment m = v.base();
		m.set(INT, 4, FRAME_VERSION);
		m.set(INT, 8, FRAME_HEADER_BYTES);
		m.set(INT, 12, FRAME_SLOTS);
		m.set(LONG, 16, FRAME_SLOT_STRIDE);
		m.set(INT, 24, VIEW_MAX_WIDTH);
		m.set(INT, 28, VIEW_MAX_HEIGHT);
		m.set(INT, 40, -1);
		m.set(INT, 44, Win32.currentPid());
		VarHandle.fullFence();
		m.set(INT, 0, FRAME_MAGIC); // last: a reader that sees the magic sees a complete header
		mapping = m;
		CraftLog.info("passthrough: frame export ready (" + FRAME_MAPPING_NAME + ", " + (MAPPING_BYTES >> 20) + " MiB)");
		return true;
	}

	/** GameRenderer.renderLevel, just before the 3D HUD (hand): copy the world layer, then clear colour for the overlay. */
	public static void captureWorld(RenderTarget target) {
		current = null;
		Messages.Camera pose = HostCamera.frame();
		if (pose == null || !ensureMapping()) {
			return;
		}
		int w = target.width, h = target.height;
		if ((long) w * h > VIEW_MAX_PIXELS) {
			if (!warnedSize) {
				warnedSize = true;
				CraftLog.warn("passthrough: the window is " + w + "x" + h + ", more than " + VIEW_MAX_PIXELS + " pixels; not exporting until the host sizes it");
			}
			return;
		}
		Capture c = ring[ringNext];
		if (c == null) {
			c = ring[ringNext] = new Capture();
		}
		long now = System.nanoTime();
		if (c.busy && now - c.busySince < STUCK_NANOS) {
			return;
		}
		if (c.width != w || c.height != h || c.color == null) {
			c.allocate(w, h);
		}
		c.generation++;
		c.busy = true;
		c.busySince = now;
		c.pose = pose;
		c.far = far;
		c.frame = ++frameCounter;
		c.captureNanos = now;
		CommandEncoder encoder = RenderSystem.getDevice().createCommandEncoder();
		encoder.copyTextureToBuffer(target.getColorTexture(), c.color, 0L, () -> {
		}, 0);
		encoder.copyTextureToBuffer(target.getDepthTexture(), c.depth, 0L, () -> {
		}, 0);
		encoder.clearColorTexture(target.getColorTexture(), TRANSPARENT);
		current = c;
	}

	/** End of GameRenderer.render: the overlay (hand, HUD, screens) is complete. */
	public static void captureOverlay(RenderTarget target) {
		Capture c = current;
		current = null;
		if (c == null) {
			return;
		}
		if (target.width != c.width || target.height != c.height) {
			c.busy = false;
			return;
		}
		long generation = c.generation;
		RenderSystem.getDevice().createCommandEncoder().copyTextureToBuffer(target.getColorTexture(), c.overlay, 0L, () -> {
			if (c.generation == generation && c.busy) {
				publish(c);
			}
		}, 0);
		ringNext = (ringNext + 1) % RING;
	}

	private static void publish(Capture c) {
		try {
			MemorySegment m = mapping;
			int slot = slotNext;
			slotNext = (slotNext + 1) % FRAME_SLOTS;
			long desc = FRAME_SLOT_DESC_OFFSET + (long) FRAME_SLOT_DESC_BYTES * slot;
			long seq = m.get(LONG, desc);
			if ((seq & 1L) != 0L) {
				seq++;
			}
			m.set(LONG, desc, seq + 1L);
			VarHandle.fullFence();
			long base = FRAME_HEADER_BYTES + FRAME_SLOT_STRIDE * slot;
			long n = (long) c.width * c.height * 4;
			copy(c.color, m, base, n);
			copy(c.depth, m, base + n, n);
			copy(c.overlay, m, base + 2 * n, n);
			Messages.Camera p = c.pose;
			m.set(LONG, desc + 8, c.frame);
			m.set(LONG, desc + 16, p.frame());
			m.set(INT, desc + 24, c.width);
			m.set(INT, desc + 28, c.height);
			m.set(FLOAT, desc + 32, NEAR);
			m.set(FLOAT, desc + 36, c.far);
			m.set(FLOAT, desc + 40, p.fovY());
			m.set(INT, desc + 44, (RenderSystem.getDevice().getDeviceInfo().isZZeroToOne() ? FRAME_DEPTH_ZERO_TO_ONE : 0) | FRAME_BOTTOM_UP | FRAME_REVERSED_Z);
			m.set(DOUBLE, desc + 48, p.x());
			m.set(DOUBLE, desc + 56, p.y());
			m.set(DOUBLE, desc + 64, p.z());
			m.set(FLOAT, desc + 72, p.yaw());
			m.set(FLOAT, desc + 76, p.pitch());
			m.set(FLOAT, desc + 80, p.roll());
			m.set(INT, desc + 84, p.firstPerson() ? 1 : 0);
			m.set(LONG, desc + 88, c.captureNanos);
			m.set(LONG, desc + 96, System.nanoTime());
			VarHandle.fullFence();
			m.set(LONG, desc, seq + 2L);
			m.set(INT, 40, slot);
			VarHandle.fullFence();
			m.set(LONG, 32, ++publishCounter);
		} catch (RuntimeException e) {
			CraftLog.limited("frameexport", 5000, "passthrough: frame export failed: " + e);
		} finally {
			c.busy = false;
		}
	}

	private static void copy(GpuBuffer buffer, MemorySegment dst, long offset, long n) {
		try (GpuBufferSlice.MappedView view = buffer.map(true, false)) {
			MemorySegment.copy(MemorySegment.ofBuffer(view.data()), 0L, dst, offset, n);
		}
	}
}
