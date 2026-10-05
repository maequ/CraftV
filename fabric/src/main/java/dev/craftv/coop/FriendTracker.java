package dev.craftv.coop;

import static dev.craftv.link.Proto.*;

import dev.craftv.link.Messages;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.function.Consumer;

/**
 * Turns "which friends are in the mirror world this tick, and where" into REMOTE_PLAYER_JOIN / STATE /
 * LEAVE messages (PROTOCOL.md §7.8-7.10). Minecraft-free and single-threaded (the server thread owns it).
 * Velocity comes from the change in position since the previous tick, and SWING marks the tick an arm
 * swing started.
 */
public final class FriendTracker {
	static final float TICKS_PER_SECOND = 20.0F;

	/** One friend as seen this tick. {@code flags} excludes SWING; {@code swinging} is the raw arm state. */
	public record Sample(int id, UUID uuid, String name, double x, double y, double z, float yaw, float pitch, float bodyYaw, int flags,
		boolean swinging, int gameMode, float health, int tick) {
	}

	private static final class Known {
		double x, y, z;
		boolean swinging;
		boolean announced;
		UUID uuid;
		String name;
	}

	private final Map<Integer, Known> known = new HashMap<>();
	private final Consumer<Messages.Payload> out;

	public FriendTracker(Consumer<Messages.Payload> out) {
		this.out = out;
	}

	/**
	 * Call once per server tick with every friend currently in the mirror world. Friends missing from
	 * {@code present} have left it: {@code goneReason} says why for each id (default {@code LEAVE_LEFT}).
	 */
	public void tick(Collection<Sample> present, Map<Integer, Integer> goneReason) {
		Set<Integer> seen = new HashSet<>();
		for (Sample s : present) {
			seen.add(s.id());
			Known k = known.get(s.id());
			boolean fresh = k == null;
			if (fresh) {
				k = new Known();
				k.x = s.x();
				k.y = s.y();
				k.z = s.z();
				known.put(s.id(), k);
			}
			if (!k.announced) {
				out.accept(new Messages.RemotePlayerJoin(s.id(), 0, s.uuid().getMostSignificantBits(), s.uuid().getLeastSignificantBits(), s.name()));
				k.announced = true;
				k.uuid = s.uuid();
				k.name = s.name();
			}
			float vx = fresh ? 0 : (float) ((s.x() - k.x) * TICKS_PER_SECOND);
			float vy = fresh ? 0 : (float) ((s.y() - k.y) * TICKS_PER_SECOND);
			float vz = fresh ? 0 : (float) ((s.z() - k.z) * TICKS_PER_SECOND);
			if (vx * vx + vy * vy + vz * vz > MAX_SPEED * MAX_SPEED) {
				vx = vy = vz = 0; // a teleport, not a speed
			}
			int flags = s.flags() & ~REMOTE_SWING;
			if (s.swinging() && !k.swinging) {
				flags |= REMOTE_SWING;
			}
			k.x = s.x();
			k.y = s.y();
			k.z = s.z();
			k.swinging = s.swinging();
			int health = (int) Math.min(255, Math.max(0, Math.ceil(s.health())));
			out.accept(new Messages.RemotePlayerState(s.id(), flags, s.x(), s.y(), s.z(), vx, vy, vz, wrap(s.yaw()), clampPitch(s.pitch()), wrap(s.bodyYaw()), s.tick(),
				Math.min(Math.max(s.gameMode(), 0), GAME_MODE_MAX), health));
		}
		for (Iterator<Map.Entry<Integer, Known>> it = known.entrySet().iterator(); it.hasNext();) {
			Map.Entry<Integer, Known> e = it.next();
			if (!seen.contains(e.getKey())) {
				if (e.getValue().announced) {
					out.accept(new Messages.RemotePlayerLeave(e.getKey(), goneReason.getOrDefault(e.getKey(), LEAVE_LEFT)));
				}
				it.remove();
			}
		}
	}

	/** The host restarted or the link came back: announce everyone again on the next tick (§7.10). */
	public void resync() {
		for (Known k : known.values()) {
			k.announced = false;
		}
	}

	/** MC's world is closing: tell the host everyone is gone (§7.10 RESET). */
	public void resetAll() {
		for (Map.Entry<Integer, Known> e : known.entrySet()) {
			if (e.getValue().announced) {
				out.accept(new Messages.RemotePlayerLeave(e.getKey(), LEAVE_RESET));
			}
		}
		known.clear();
	}

	public int count() {
		return known.size();
	}

	static float wrap(float degrees) {
		float d = degrees % 360.0F;
		if (d >= 180.0F) {
			d -= 360.0F;
		} else if (d < -180.0F) {
			d += 360.0F;
		}
		return Float.isFinite(d) ? d : 0.0F;
	}

	static float clampPitch(float pitch) {
		return Float.isFinite(pitch) ? Math.max(-90.0F, Math.min(90.0F, pitch)) : 0.0F;
	}
}
